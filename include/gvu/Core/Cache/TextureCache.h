#ifndef GVU_TEXTURE_CACHE_H
#define GVU_TEXTURE_CACHE_H

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <iostream>
#include <variant>
#include <cstring>
#include <algorithm>
#include <memory>
#include <unordered_set>
#include <chrono>
#include <vk_mem_alloc.h>

#include <gvu/Core/FormatInfo.h>
#include <gvu/Core/Managers/CommandPoolManager.h>
#include <gvu/Core/Managers/DescriptorPoolManager.h>
#include "Objects.h"

namespace gvu
{

struct SharedData
{
    gvu::CommandPoolManager    commandPool;
    VmaAllocator               allocator = nullptr;
    std::vector<TextureHandle> images;
    DescriptorSetLayoutCache   layoutCache;
    DescriptorPoolManager      descriptorPool;

    BufferHandle _stagingBuffer;
};


/**
 * @brief The MemoryCache class
 *
 * The texture cache is used to allocate ALL textures.
 *
 * It works by allocating a texture whenever you need it. Then
 * when you are finished with the texture you "return" it to the cache.
 * The texture is not destroyed and the image is not freed. The next time
 * you allocate a texture with the same dimensions/type/etc, it will search
 * its cache for any textures that have been returned and returns that.
 */
class MemoryCache
{
public:

    using texture_handle_type = TextureHandle;


    void init(VkPhysicalDevice physicalDevice,
              VkDevice         device,
              VkQueue          graphicsQueue,
              VmaAllocator     allocator)
    {
        auto data = std::make_shared<SharedData>();

        data->commandPool.init(device, physicalDevice, graphicsQueue);
        m_sharedData = data;
        m_sharedData->allocator = allocator;
        m_sharedData->layoutCache.init(device);

        DescriptorSetLayoutCreateInfo dslci;
        dslci.bindings.push_back( VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        auto layout = m_sharedData->layoutCache.create(dslci);
        m_sharedData->descriptorPool.init(device, &m_sharedData->layoutCache, layout, 1024);

        m_sharedData->_stagingBuffer = allocateBuffer(1024*1024*4, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    /**
     * @brief destroy
     *
     * Destroys the texture cache and frees all memory
     * that was created.
     */
    void destroy()
    {
        auto & m_images = m_sharedData->images;
        while(m_images.size())
        {
            auto useCount = m_images.back().use_count();
            if( useCount > 1)
            {
                std::cerr << "WARNING: Image " << m_images.back().get() << " is still being referenced! Use Count: " << useCount << std::endl;
            }
            destroyTexture(*m_images.back());
            m_images.pop_back();
        }

        if(m_sharedData->_stagingBuffer)
        {
            m_sharedData->_stagingBuffer->destroy();
        }

        m_sharedData->descriptorPool.destroy();
        m_sharedData->layoutCache.destroy();
        m_sharedData->commandPool.destroy();
    }

    /**
     * @brief allocateTexture2D
     * @param width
     * @param height
     * @param format
     * @param mipmaps
     * @return
     *
     * Allocate a 2D texture used for sampling. This is a GPU DEVICE ONLY texture. You cannot
     * map this texture.
     */
    texture_handle_type allocateTexture2D(uint32_t width, uint32_t height, VkFormat format, uint32_t mipmaps=0, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        if(mipmaps!=0)
            mipLevels = std::min(mipmaps, mipLevels);
        return allocateTexture({width,height,1}, format, VK_IMAGE_VIEW_TYPE_2D, 1, mipLevels, VK_IMAGE_LAYOUT_UNDEFINED, usage);
    }

    texture_handle_type allocateTextureCube(uint32_t length,VkFormat format, uint32_t mipmaps=0, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(length, length)))) + 1;
        if(mipmaps!=0)
            mipLevels = std::min(mipmaps, mipLevels);
        return allocateTexture({length,length,1}, format, VK_IMAGE_VIEW_TYPE_CUBE, 6, mipLevels, VK_IMAGE_LAYOUT_UNDEFINED, usage);
    }

    /**
     * @brief allocateBuffer
     * @param bytes
     * @param usage
     * @param memUsage
     * @return
     *
     * Allocate a buffer
     */
    BufferHandle allocateBuffer(size_t bytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
    {
        auto b = std::make_shared<BufferInfo>();
        b->allocator = getAllocator();

        VkBufferCreateInfo & bufferInfo = b->bufferInfo;
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = bytes;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo & allocCInfo = b->allocationCreateInfo;
        allocCInfo.usage = memUsage;
        allocCInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // can always set this bit,
                                                             // vma will not allow device local
                                                             // memory to be mapped

        VkBuffer &         buffer     = b->buffer;
        VmaAllocation &    allocation = b->allocation;
        VmaAllocationInfo &allocInfo  = b->allocationInfo;

        auto result = vmaCreateBuffer(getAllocator(), &bufferInfo, &allocCInfo, &buffer, &allocation, &allocInfo);

        if( result != VK_SUCCESS)
        {
           throw std::runtime_error( "Error allocating VMA Buffer");
        }
        b->sharedData = m_sharedData;
        return b;
    }


    /**
     * @brief allocateTexture
     * @param ext3d
     * @param format
     * @param viewType
     * @param layers
     * @param mipMaps
     * @param finalLayout
     * @param usage
     * @return
     *
     * Allocate a texture or return one that has been cached.
     * Memory is only allocated if no texture of the same size/type
     * is available in the cache
     */
    texture_handle_type        allocateTexture(VkExtent3D        ext3d,
                                               VkFormat          format,
                                               VkImageViewType   viewType,
                                               uint32_t          layers,
                                               uint32_t          mipMaps,
                                               VkImageLayout     finalLayout,
                                               VkImageUsageFlags usage)
    {
        // Loop through all the free images.
        // These are images which were no longer needed, but are still
        // allocated. They can be reused.
        // So find one that fits the dimensions/format/etc
        // and return that one. none exist, then
        // we will have to create a new texture
        {
            auto id = _findAvailable(format, ext3d, viewType, usage, layers, mipMaps);

            if( id )
            {
                // a free image was found
                return id;
            }
        }

        {
            auto img = image_Create( getDevice(), getAllocator(), ext3d, format, viewType, layers, mipMaps, usage);

            auto id = std::make_shared<ImageInfo>(img);
            m_sharedData->images.push_back(id);

            if( finalLayout != VK_IMAGE_LAYOUT_UNDEFINED )
            {
                m_sharedData->commandPool.beginRecording(
                            [=](auto cmd)
                            {
                                gvu::CommandBuffer C(cmd);
                                C.imageTransitionLayout(id->image,
                                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                                        finalLayout,
                                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                            },
                            true);
            }



            id->sharedData = m_sharedData;
            return id;
        }

    }

    VkDevice getDevice() const
    {
        return m_sharedData->commandPool.getDevice();
    }

    VmaAllocator getAllocator() const
    {
        return m_sharedData->allocator;
    }

    static bool isDepth(VkFormat f)
    {
        switch(f)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return true;
                break;
            default:
                return false;
                break;
        }
    }


    size_t getAllocatedTextureCount() const
    {
        return m_sharedData->images.size();
    }

protected:

    void destroyTexture(ImageInfo & I)
    {
        auto dev = m_sharedData->commandPool.getDevice();

        vkDestroySampler(  dev, I.sampler.linear,  nullptr);
        vkDestroySampler(  dev, I.sampler.nearest, nullptr);
        vkDestroyImageView(dev, I.imageView      , nullptr);

        for(auto & v : I.mipMapViews)
        {
            vkDestroyImageView(dev, v, nullptr);
        }
        for(auto & v : I.m_imageViews)
        {
            vkDestroyImageView(dev, v.second, nullptr);
        }

        //std::cerr << "Destroying Image: " << I.image << std::endl;
        vmaDestroyImage(m_sharedData->allocator, I.image, I.allocation);

        I.allocation     = nullptr;
        I.image          = VK_NULL_HANDLE;
        I.imageView      = VK_NULL_HANDLE;
        I.sampler.linear = I.sampler.nearest = VK_NULL_HANDLE;
        I.mipMapViews.clear();
    }

    texture_handle_type _findAvailable(VkFormat format, VkExtent3D extent, VkImageViewType viewType, VkImageUsageFlags usage, uint32_t arrayLayers, uint32_t mipMaps)
    {
        for(auto & i : m_sharedData->images)
        {
            if( i.use_count() == 1) // The texture Cache is the only object that is holding a reference to this image
            {
                auto & _info = i->info;
                if( _info.extent.width  == extent.width &&
                    _info.extent.height == extent.height &&
                    _info.extent.depth  == extent.depth  &&
                    _info.arrayLayers   == arrayLayers &&
                    _info.mipLevels     == mipMaps &&
                    _info.format        == format &&
                    i->viewType         == viewType &&
                    _info.usage         == usage )
                {
                    auto ret = i;
                    return i;
                }
            }
        }
        return nullptr;
    }

    static
    ImageInfo  image_Create(  VkDevice device
                             ,VmaAllocator m_allocator
                             ,VkExtent3D extent
                             ,VkFormat format
                             ,VkImageViewType viewType
                             ,uint32_t arrayLayers
                             ,uint32_t miplevels // maximum mip levels
                             ,VkImageUsageFlags additionalUsageFlags
                                    )
    {
        VkImageCreateInfo imageInfo{};

        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = extent.depth==1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
        imageInfo.format        = format;
        imageInfo.extent        = extent;
        imageInfo.mipLevels     = miplevels;
        imageInfo.arrayLayers   = arrayLayers;

        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;// vk::SampleCountFlagBits::e1;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;// vk::ImageTiling::eOptimal;
        imageInfo.usage         = additionalUsageFlags | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;// vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;// vk::SharingMode::eExclusive;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// vk::ImageLayout::eUndefined;

        if( arrayLayers == 6)
            imageInfo.flags |=  VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;// vk::ImageCreateFlagBits::eCubeCompatible;

        VmaAllocationCreateInfo allocCInfo = {};
        allocCInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkImage           image;
        VmaAllocation     allocation;
        VmaAllocationInfo allocInfo;

        VkImageCreateInfo & imageInfo_c = imageInfo;
        auto suc = vmaCreateImage(m_allocator,  &imageInfo_c,  &allocCInfo, &image, &allocation, &allocInfo);
        assert(suc == VK_SUCCESS);

        ImageInfo I;
        I.image      = image;
        I.info       = imageInfo;
        I.allocInfo  = allocInfo;
        I.allocation = allocation;
        I.viewType   = viewType;
        I.byteSize   = allocInfo.size;

        // create the image view
        {
            {
                VkImageViewCreateInfo ci{};
                ci.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ci.image      = I.image;
                ci.viewType   = viewType;
                ci.format     = format;
                ci.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

                ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

                ci.subresourceRange.baseMipLevel = 0;
                ci.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

                ci.subresourceRange.baseArrayLayer = 0;
                ci.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                switch(ci.format)
                {
                    case VK_FORMAT_D16_UNORM:
                    case VK_FORMAT_D32_SFLOAT:
                    case VK_FORMAT_D16_UNORM_S8_UINT:
                    case VK_FORMAT_D24_UNORM_S8_UINT:
                    case VK_FORMAT_D32_SFLOAT_S8_UINT:
                        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// vk::ImageAspectFlagBits::eDepth;
                        break;
                    default:
                        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //vk::ImageAspectFlagBits::eColor;
                        break;
                }

                suc = vkCreateImageView(device, &ci, nullptr, &I.imageView);
                assert(suc == VK_SUCCESS);

                // Create one image view per mipmap level
                for(uint32_t i=0;i<miplevels;i++)
                {
                    ci.subresourceRange.baseMipLevel = i;
                    ci.subresourceRange.levelCount = 1;

                    VkImageView vv;
                    suc = vkCreateImageView(device, &ci, nullptr, &vv);
                    assert(suc == VK_SUCCESS);
                    I.mipMapViews.push_back(vv);
                }
            }
        }


        // create a sampler
        {
            // Temporary 1-mipmap sampler
            VkSamplerCreateInfo ci;
            ci.sType                   =  VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            ci.magFilter               =  VK_FILTER_LINEAR;//  vk::Filter::eLinear;
            ci.minFilter               =  VK_FILTER_LINEAR;//  vk::Filter::eLinear;
            ci.mipmapMode              =  VK_SAMPLER_MIPMAP_MODE_LINEAR;// vk::SamplerMipmapMode::eLinear ;
            ci.addressModeU            =  VK_SAMPLER_ADDRESS_MODE_REPEAT;//vk::SamplerAddressMode::eRepeat ;
            ci.addressModeV            =  VK_SAMPLER_ADDRESS_MODE_REPEAT;//vk::SamplerAddressMode::eRepeat ;
            ci.addressModeW            =  VK_SAMPLER_ADDRESS_MODE_REPEAT;// vk::SamplerAddressMode::eRepeat ;
            ci.mipLodBias              =  0.0f  ;
            ci.anisotropyEnable        =  VK_FALSE;
            ci.maxAnisotropy           =  1 ;
            ci.compareEnable           =  VK_FALSE ;
            ci.compareOp               =  VK_COMPARE_OP_ALWAYS;// vk::CompareOp::eAlways ;
            ci.minLod                  =  0 ;
            ci.maxLod                  =  VK_LOD_CLAMP_NONE;//static_cast<float>(miplevels);
            ci.borderColor             =  VK_BORDER_COLOR_INT_OPAQUE_BLACK;// vk::BorderColor::eIntOpaqueBlack ;
            ci.unnormalizedCoordinates =  VK_FALSE ;

            ci.magFilter               =  VK_FILTER_LINEAR;//vk::Filter::eLinear;
            ci.minFilter               =  VK_FILTER_LINEAR;//vk::Filter::eLinear;

            vkCreateSampler(device, &ci, nullptr, &I.sampler.linear);

            ci.magFilter               =  VK_FILTER_NEAREST;//vk::Filter::eNearest;
            ci.minFilter               =  VK_FILTER_NEAREST;//vk::Filter::eNearest;

            vkCreateSampler(device, &ci, nullptr, &I.sampler.nearest);
        }


        return I;
    }

protected:
    std::shared_ptr<SharedData>                      m_sharedData;
};

void BufferInfo::setData(void *data, VkDeviceSize byteSize, VkDeviceSize offset)
{
    auto allocator = sharedData->allocator;

    if(sharedData->_stagingBuffer)
    {
        if( sharedData->_stagingBuffer->getBufferSize() < byteSize)
        {
            sharedData->_stagingBuffer->resize(byteSize);
        }
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->_stagingBuffer->mapData(), data, byteSize);
        sharedData->_stagingBuffer->flush();
    }

    {
        auto fence = sharedData->commandPool.beginRecording([srcBuffer=sharedData->_stagingBuffer->getBuffer(),
                                                             dstBuffer=this->getBuffer(),byteSize,offset,this](auto cmd)
        {
            VkBufferCopy region;
            region.dstOffset = offset;
            region.size      = byteSize;
            region.srcOffset = 0;
            vkCmdCopyBuffer(cmd, srcBuffer ,dstBuffer, 1, &region);

        }, true);
    }
}

inline void BufferInfo::resize(VkDeviceSize bytes)
{
    destroy();

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = bytes;
    //bufferInfo.usage = usage;

    VmaAllocationCreateInfo & allocCInfo = allocationCreateInfo;
    //allocCInfo.usage = memUsage;
    allocCInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // can always set this bit,
    // vma will not allow device local
    // memory to be mapped


    auto result = vmaCreateBuffer(sharedData->allocator, &bufferInfo, &allocCInfo, &buffer, &allocation, &allocationInfo);

    if( result != VK_SUCCESS)
    {
        throw std::runtime_error( "Error allocating VMA Buffer");
    }
}

void ImageInfo::setData(void * data)
{
    auto allocator = sharedData->allocator;

    auto byteSize = getExtents().width * getExtents().height * getFormatInfo(info.format).blockSizeInBits/8;

    if(sharedData->_stagingBuffer)
    {
        if( sharedData->_stagingBuffer->getBufferSize() < byteSize)
        {
            sharedData->_stagingBuffer->resize(byteSize);
        }
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->_stagingBuffer->mapData(), data, byteSize);
        sharedData->_stagingBuffer->flush();
    }

    {
        auto fence = sharedData->commandPool.beginRecording([buffer=sharedData->_stagingBuffer->getBuffer(),this](auto cmd)
        {
            VkImageMemoryBarrier copy_barrier = {};
            copy_barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copy_barrier.dstAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
            copy_barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
            copy_barrier.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copy_barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.image                       = getImage();
            copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.layerCount = 1;
            copy_barrier.subresourceRange.baseArrayLayer = 0;
            copy_barrier.subresourceRange.baseMipLevel = 0;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);


            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width           = getExtents().width;
            region.imageExtent.height          = getExtents().height;
            region.imageExtent.depth           = getExtents().depth;
            vkCmdCopyBufferToImage(cmd, buffer, getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier = {};
            use_barrier.sType                            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier.srcAccessMask                    = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier.dstAccessMask                    = VK_ACCESS_SHADER_READ_BIT;
            use_barrier.oldLayout                        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier.newLayout                        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            use_barrier.srcQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.dstQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.image                            = getImage();
            use_barrier.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier.subresourceRange.levelCount      = 1;
            use_barrier.subresourceRange.layerCount      = 1;
            copy_barrier.subresourceRange.baseArrayLayer = 0;
            copy_barrier.subresourceRange.baseMipLevel   = 0;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);


        }, true);
    }
}


void ImageInfo::copyData(void * data, uint32_t width, uint32_t height,
                         uint32_t arrayLayer, uint32_t mipLevel,
                         uint32_t x_ImageOffset, uint32_t y_ImageOffset)
{
    auto allocator = sharedData->allocator;

    auto byteSize = width * height * getFormatInfo(info.format).blockSizeInBits/8;

    if(sharedData->_stagingBuffer)
    {
        if( sharedData->_stagingBuffer->getBufferSize() < byteSize)
        {
            sharedData->_stagingBuffer->resize(byteSize);
        }
    }
    // copy to staging buffer
    {
        std::memcpy(sharedData->_stagingBuffer->mapData(), data, byteSize);
        sharedData->_stagingBuffer->flush();
    }

    {
        auto fence = sharedData->commandPool.beginRecording([buffer=sharedData->_stagingBuffer->getBuffer(),width, height,arrayLayer, mipLevel, x_ImageOffset, y_ImageOffset, this](auto cmd)
        {
            VkImageMemoryBarrier copy_barrier = {};
            copy_barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copy_barrier.dstAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
            copy_barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
            copy_barrier.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copy_barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.image                       = getImage();
            copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.layerCount = 1;
            copy_barrier.subresourceRange.baseArrayLayer = arrayLayer;
            copy_barrier.subresourceRange.baseMipLevel = mipLevel;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);


            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.bufferRowLength             = height;
            region.bufferImageHeight           = width;
            region.imageOffset.x               = x_ImageOffset;
            region.imageOffset.y               = y_ImageOffset;
            region.imageOffset.z               = 0;
            region.imageExtent.width           = width;// getExtents().width;
            region.imageExtent.height          = height; //getExtents().height;
            region.imageExtent.depth           = 1;//getExtents().depth;
            vkCmdCopyBufferToImage(cmd, buffer, getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier = {};
            use_barrier.sType                            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier.srcAccessMask                    = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier.dstAccessMask                    = VK_ACCESS_SHADER_READ_BIT;
            use_barrier.oldLayout                        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier.newLayout                        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            use_barrier.srcQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.dstQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.image                            = getImage();
            use_barrier.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier.subresourceRange.levelCount      = 1;
            use_barrier.subresourceRange.layerCount      = 1;
            copy_barrier.subresourceRange.baseArrayLayer = arrayLayer;
            copy_barrier.subresourceRange.baseMipLevel   = mipLevel;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);


        }, true);
    }
}

void ImageInfo::cmdCopyData(void * data, uint32_t width, uint32_t height,
                         uint32_t arrayLayer, uint32_t mipLevel,
                         uint32_t x_ImageOffset, uint32_t y_ImageOffset)
{
    auto allocator = sharedData->allocator;

    auto byteSize = width * height * getFormatInfo(info.format).blockSizeInBits/8;

    if(sharedData->_stagingBuffer)
    {
        if( sharedData->_stagingBuffer->getBufferSize() < byteSize)
        {
            sharedData->_stagingBuffer->resize(byteSize);
        }
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->_stagingBuffer->mapData(), data, byteSize);
        sharedData->_stagingBuffer->flush();
    }

    {
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.bufferRowLength             = height;
        region.bufferImageHeight           = width;
        region.imageOffset.x               = x_ImageOffset;
        region.imageOffset.y               = y_ImageOffset;
        region.imageOffset.z               = 0;
        region.imageExtent.width           = width;// getExtents().width;
        region.imageExtent.height          = height; //getExtents().height;
        region.imageExtent.depth           = 1;//getExtents().depth;
        vkCmdCopyBufferToImage(m_updateCommandBuffer, sharedData->_stagingBuffer->getBuffer(), getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
}

inline void ImageInfo::beginUpdate()
{
    m_updateCommandBuffer = sharedData->commandPool.allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
}

inline void ImageInfo::endUpdate()
{
    GVK_CHECK_RESULT(vkEndCommandBuffer(m_updateCommandBuffer));
    auto fence = sharedData->commandPool.submitCommandBuffer(m_updateCommandBuffer, sharedData->commandPool.getGraphicsQueue(), true);
}

inline void ImageInfo::cmdTransitionImage(uint32_t arrayLayer, uint32_t mipLevel, VkImageLayout currentLayout, VkImageLayout finalLayout)
{
    VkImageMemoryBarrier copy_barrier = {};
    copy_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier.oldLayout                       = currentLayout;
    copy_barrier.newLayout                       = finalLayout;
    copy_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.image                           = getImage();
    copy_barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier.subresourceRange.levelCount     = 1;
    copy_barrier.subresourceRange.layerCount     = 1;
    copy_barrier.subresourceRange.baseArrayLayer = arrayLayer;
    copy_barrier.subresourceRange.baseMipLevel   = mipLevel;
    vkCmdPipelineBarrier(m_updateCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);
}

inline void ImageInfo::cmdTransitionImage(std::shared_ptr<ImageInfo> other,uint32_t arrayLayer, uint32_t mipLevel, VkImageLayout currentLayout, VkImageLayout finalLayout)
{
    VkImageMemoryBarrier copy_barrier = {};
    copy_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier.oldLayout                       = currentLayout;
    copy_barrier.newLayout                       = finalLayout;
    copy_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.image                           = other->getImage();
    copy_barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier.subresourceRange.levelCount     = 1;
    copy_barrier.subresourceRange.layerCount     = 1;
    copy_barrier.subresourceRange.baseArrayLayer = arrayLayer;
    copy_barrier.subresourceRange.baseMipLevel   = mipLevel;
    vkCmdPipelineBarrier(m_updateCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);
}

VkDescriptorSet ImageInfo::getSingleImageSet(uint32_t layer, uint32_t mip)
{
    auto it = arrayMipDescriptorSet.find({layer,mip});
    if(it != arrayMipDescriptorSet.end() )
        return it->second;

    auto set =  sharedData->descriptorPool.allocateDescriptorSet();

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView             = getImageView(layer,1, mip,1, VK_IMAGE_VIEW_TYPE_2D);
    imageInfo.sampler               = getLinearSampler();

    VkWriteDescriptorSet wr = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wr.descriptorCount      = 1;
    wr.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.dstSet               = set;
    wr.pImageInfo           = &imageInfo;

    vkUpdateDescriptorSets(sharedData->commandPool.getDevice(), 1, &wr, 0, nullptr);
    arrayMipDescriptorSet[{layer,mip}] = set;
    return set;
}

VkImageView ImageInfo::getImageView(uint32_t layer, uint32_t layerCount, uint32_t mip, uint32_t mipCount, VkImageViewType type)
{
    ImageViewRange r{layer,layerCount,mip,mipCount,type};

    auto it = m_imageViews.find(r);
    if( it != m_imageViews.end())
        return it->second;

    VkImageViewCreateInfo ci{};
    ci.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image      = image;
    ci.viewType   = type==VK_IMAGE_VIEW_TYPE_MAX_ENUM ? viewType : type;
    ci.format     = info.format;
    ci.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    ci.subresourceRange.baseMipLevel = mip;
    ci.subresourceRange.levelCount = mipCount;

    ci.subresourceRange.baseArrayLayer = layer;
    ci.subresourceRange.layerCount = layerCount;

    ci.subresourceRange.aspectMask = MemoryCache::isDepth(info.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    auto device = sharedData->commandPool.getDevice();
    VkImageView v = VK_NULL_HANDLE;
    auto suc = vkCreateImageView(device, &ci, nullptr, &v);
    assert(suc == VK_SUCCESS);
    m_imageViews[r] = v;
    return v;
}

}
#endif


