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

#include <gvu/FormatInfo.h>
#include <gvu/Managers/CommandPoolManager.h>
#include <gvu/Managers/DescriptorPoolManager.h>

namespace gvu
{
struct SharedData;

struct ImageInfo
{
    /**
     * @brief setData
     * @param data
     *
     * Sets the data for the first mapmap level for this image.
     */
    void setData(void * data);


    /**
     * @brief copyData
     * @param data
     * @param width
     * @param height
     * @param x_offset
     * @param y_offset
     *
     * Copy raw data into a specific section of the image.
     *
     * width and height refer to the image that is stored in *data;
     *
     * data must be a pointer to width*height*pixelSize() bytes.
     *
     * The arrayLayer/mipLevel will be transitioned to TRANSFER_DST
     * the image data will be copied
     * and the image layer/level will be transitioned to SHADER_READ_ONLY_OPTIMAL
     */
    void copyData(void * data, uint32_t width, uint32_t height,
                  uint32_t arrayLevel = 0, uint32_t mipLevel = 0,
                  uint32_t x_ImageOffset=0, uint32_t y_ImageOffset=0);



    //==================================================================
    // Command Buffer related functions.
    // These functions must be called between beginUpdate() and endUpdate()
    // when endUpdate() is called, the command buffer is executed
    // and waited until finished.
    //==================================================================
    void beginUpdate();
    void endUpdate();

    /**
     * @brief cmdCopyData
     * @param data
     * @param width
     * @param height
     * @param arrayLevel
     * @param mipLevel
     * @param x_ImageOffset
     * @param y_ImageOffset
     *
     * Writes a copy command to copy data from the host to the image.
     * data/width/height specify the image information on the host.
     *
     * arrayLayer, mipLevel/ x_ImageOffset/y_ImageOffset specify where
     * in in the image to copy the data to.
     */
    void cmdCopyData(void * data, uint32_t width, uint32_t height,
                     uint32_t arrayLayer = 0, uint32_t mipLevel = 0,
                     uint32_t x_ImageOffset=0, uint32_t y_ImageOffset=0);

    /**
     * @brief cmdTransitionImage
     * @param arrayLayer
     * @param mipLevel
     * @param currentLayout
     * @param finalLayout
     *
     * Transition the image layer/level to a specific layout.
     */
    void cmdTransitionImage(uint32_t arrayLayer,
                            uint32_t mipLevel,
                            VkImageLayout currentLayout,
                            VkImageLayout finalLayout);

    /**
     * @brief cmdTransitionImage
     * @param otherImage
     * @param arrayLayer
     * @param mipLevel
     * @param currentLayout
     * @param finalLayout
     *
     * Transition another image's layer/miplevel to another layout
     */
    void cmdTransitionImage(std::shared_ptr<ImageInfo> otherImage,
                            uint32_t arrayLayer,
                            uint32_t mipLevel,
                            VkImageLayout currentLayout,
                            VkImageLayout finalLayout);

    void cmdBlitFromImage(ImageInfo & srcImage,
                          uint32_t srcArrayLayer, uint32_t srcMipLevel,
                          VkOffset3D srcOffset,
                          VkExtent3D srcExtents,
                          ImageInfo & dstImage,
                          uint32_t dstArrayLayer, uint32_t dstMipLevel,
                          VkOffset3D dstOffset,
                          VkExtent3D dstExtents)
    {
        VkImageBlit region = {};
        region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = srcArrayLayer;
        region.srcSubresource.layerCount     = 1;
        region.srcSubresource.mipLevel       = srcMipLevel;

        region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = dstArrayLayer;
        region.dstSubresource.layerCount     = 1;
        region.dstSubresource.mipLevel       = dstMipLevel;

        region.srcOffsets[0].x = srcOffset.x;//width;
        region.srcOffsets[0].y = srcOffset.y;//height;
        region.srcOffsets[0].z = srcOffset.z;//depth;

        region.dstOffsets[0].x = dstOffset.x;//width;
        region.dstOffsets[0].y = dstOffset.y;//height;
        region.dstOffsets[0].z = dstOffset.z;//depth;

        region.srcOffsets[1].x = srcExtents.width + srcOffset.x;//width;
        region.srcOffsets[1].y = srcExtents.height + srcOffset.y;//height;
        region.srcOffsets[1].z = srcExtents.depth + srcOffset.z;//depth;
        region.dstOffsets[1].x = dstExtents.width + dstOffset.x;//width;
        region.dstOffsets[1].y = dstExtents.height + dstOffset.y;//height;
        region.dstOffsets[1].z = dstExtents.depth + dstOffset.z;//depth;

        vkCmdBlitImage(m_updateCommandBuffer,
                       srcImage.getImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region,VK_FILTER_LINEAR);
    }

    /**
     * @brief cmdGenerateMipMap
     * @param arrayLayer
     *
     * Generates mipmaps for a particular array layer. This works
     * by copying the mipmap from the previous later using a linear filter
     *
     * Requirement: Mip 0 must be in SHADER_READ_ONLY_OPTIMAL
     */
    void cmdGenerateMipMap(uint32_t arrayLayer, VkImageLayout mip0CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        VkExtent3D srcExtents = getExtents();
        VkExtent3D dstExtents = getExtents();
        VkOffset3D srcOffset = {};
        VkOffset3D dstOffset = {};

        if( mip0CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            cmdTransitionImage(arrayLayer, 0, mip0CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        for(uint32_t m=1;m<getMipLevels();m++)
        {
            cmdTransitionImage(arrayLayer, m, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            dstExtents.width  = srcExtents.width  >> 1;
            dstExtents.height = srcExtents.height >> 1;
            dstExtents.depth  = 1;//srcExtents.depth << 1;

            cmdBlitFromImage(*this, arrayLayer, m-1, srcOffset,srcExtents,*this,arrayLayer,m, dstOffset,dstExtents);

            cmdTransitionImage(arrayLayer, m, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            srcExtents = dstExtents;
        }


        for(uint32_t m=0;m<getMipLevels();m++)
        {
            cmdTransitionImage(arrayLayer, m, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    uint32_t getMipLevels() const
    {
        return info.mipLevels;
    }
    uint32_t getLayerCount() const
    {
        return info.arrayLayers;
    }
    VkExtent3D const & getExtents() const
    {
        return info.extent;
    }
    VkImage getImage() const
    {
        return image;
    }
    VkImageView getImageView() const
    {
        return imageView;
    }
    VkImageViewType getImageViewType() const
    {
        return viewType;
    }
    VkImageView getMipMapView(uint32_t mipLevel) const
    {
        return mipMapViews.at(mipLevel);
    }
    VkSampler getNearestSampler() const
    {
        return sampler.nearest;
    }
    VkSampler getLinearSampler() const
    {
        return sampler.linear;
    }
    VkDescriptorSet getSingleDescriptorSet() const
    {
        return singleDescriptorSet;
    }
    uint32_t pixelSize() const
    {
        return getFormatInfo(info.format).blockSizeInBits / 8;
    }
    uint64_t getByteSize() const
    {
        return byteSize;
    }

protected:
    VkImage                  image      = VK_NULL_HANDLE;
    VkImageView              imageView  = VK_NULL_HANDLE;
    VkImageCreateInfo        info       = {};
    VmaAllocation            allocation = nullptr;
    VmaAllocationInfo        allocInfo  = {};
    VkImageViewType          viewType;
    std::vector<VkImageView> mipMapViews; // one image view per mipmap
    uint64_t byteSize = 0;
    struct
    {
        VkSampler linear  = VK_NULL_HANDLE;
        VkSampler nearest = VK_NULL_HANDLE;

        //VkSampler current = VK_NULL_HANDLE; // do not destroy
    } sampler;

    // a single descriptor set which is used for this
    // image so that it can be passed to IMGUI
    VkDescriptorSet singleDescriptorSet = VK_NULL_HANDLE;

    std::shared_ptr<SharedData> sharedData;
    bool selfManaged = true;
    VkCommandBuffer m_updateCommandBuffer = VK_NULL_HANDLE;
    friend class TextureCache;
};

using TextureHandle   = std::shared_ptr<ImageInfo>;
using WTextureHandle  = std::weak_ptr<ImageInfo>;

struct SharedData
{
    gvu::CommandPoolManager    commandPool;
    VmaAllocator               allocator = nullptr;
    std::vector<TextureHandle> images;
    DescriptorSetLayoutCache   layoutCache;
    DescriptorPoolManager      descriptorPool;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAllocation = nullptr;
    void * stagingBufferMapped = nullptr;
};


/**
 * @brief The TextureCache class
 *
 * The texture cache is used to allocate ALL textures.
 *
 * It works by allocating a texture whenever you need it. Then
 * when you are finished with the texture you "return" it to the cache.
 * The texture is not destroyed and the image is not freed. The next time
 * you allocate a texture with the same dimensions/type/etc, it will search
 * its cache for any textures that have been returned and returns that.
 */
class TextureCache
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

        if(m_sharedData->stagingBufferAllocation)
        {
            vmaUnmapMemory(  m_sharedData->allocator, m_sharedData->stagingBufferAllocation);
            vmaDestroyBuffer(m_sharedData->allocator, m_sharedData->stagingBuffer, m_sharedData->stagingBufferAllocation);

            m_sharedData->stagingBufferAllocation = nullptr;
            m_sharedData->stagingBuffer = VK_NULL_HANDLE;
            m_sharedData->stagingBufferMapped = nullptr;
        }
        m_sharedData->descriptorPool.destroy();
        m_sharedData->layoutCache.destroy();
        m_sharedData->commandPool.destroy();
    }
#if 0
    std::pair<const_texture_handle_type, bool> allocateColorAttachment(VkExtent2D        ext,
                                                                       VkFormat          format,
                                                                       VkImageLayout     finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                       VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        return allocateTexture( {ext.width, ext.height, 1}, format, VK_IMAGE_VIEW_TYPE_2D, 1, 1, finalLayout, usage);
    }
    std::pair<const_texture_handle_type, bool> allocateDepthAttachment(VkExtent2D        ext,
                                                                       VkFormat          format = VK_FORMAT_D32_SFLOAT,
                                                                       VkImageLayout     finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                       VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        return allocateTexture( {ext.width, ext.height, 1}, format, VK_IMAGE_VIEW_TYPE_2D, 1, 1, finalLayout, usage);
    }
#endif

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
    texture_handle_type allocateTexture2D(uint32_t width, uint32_t height, VkFormat format, uint32_t mipmaps=0)
    {
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        if(mipmaps!=0)
            mipLevels = std::min(mipmaps, mipLevels);
        return allocateTexture({width,height,1}, format, VK_IMAGE_VIEW_TYPE_2D, 1, mipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_USAGE_SAMPLED_BIT);
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

            {
                id->singleDescriptorSet =  m_sharedData->descriptorPool.allocateDescriptorSet();

                VkDescriptorImageInfo imageInfo = {};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = id->getImageView();
                imageInfo.sampler   = id->getLinearSampler();

                VkWriteDescriptorSet wr = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                wr.descriptorCount      = 1;
                wr.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wr.dstSet               = id->singleDescriptorSet;
                wr.pImageInfo           = &imageInfo;

                vkUpdateDescriptorSets(getDevice(), 1, &wr, 0, nullptr);
            }

            id->sharedData = m_sharedData;
            return id;
        }

    }
#if 0
    /**
     * @brief destroyTexture
     * @param i
     *
     * Destroys the texture if it is no no longer used by anything else.
     * Use this function for textures such off-screen render images that
     * change size.
     *
     * Texture is only destroyed if the use count == 2
     */
    bool destroyTextureIfUnused(const_texture_handle_type &i)
    {
        if(i.use_count() == 2)
        {
            for(auto & x : m_images)
            {
                if( x.get() == i.get() && x.get() != nullptr)
                {
                    // actually destroy the texture
                    destroyTexture(*x);
                    std::swap(x, m_images.back());

                    m_images.pop_back();

                    return true;
                }
            }
            throw std::runtime_error("That texture is not managed by this cache");
        }
        return false;
    }
#endif

    VkDevice getDevice() const
    {
        return m_sharedData->commandPool.m_device;
    }
    VmaAllocator getAllocator() const
    {
        return m_sharedData->allocator;
    }
protected:

    void destroyTexture(ImageInfo & I)
    {
        auto dev = m_sharedData->commandPool.m_device;

        vkDestroySampler(  dev, I.sampler.linear,  nullptr);
        vkDestroySampler(  dev, I.sampler.nearest, nullptr);
        vkDestroyImageView(dev, I.imageView      , nullptr);

        for(auto & v : I.mipMapViews)
        {
            vkDestroyImageView(dev, v, nullptr);
        }
        std::cerr << "Destroying Image: " << I.image << std::endl;
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


void ImageInfo::setData(void * data)
{
    auto allocator = sharedData->allocator;

    auto byteSize = getExtents().width * getExtents().height * getFormatInfo(info.format).blockSizeInBits/8;

    if(sharedData->stagingBufferAllocation)
    {
        VmaAllocationInfo stagingBufferInfo = {};
        vmaGetAllocationInfo(allocator, sharedData->stagingBufferAllocation, &stagingBufferInfo);
        if(stagingBufferInfo.size < byteSize)
        {
            vmaUnmapMemory(sharedData->allocator, sharedData->stagingBufferAllocation);
            vmaDestroyBuffer(sharedData->allocator, sharedData->stagingBuffer, allocation);

            sharedData->stagingBufferAllocation = nullptr;
            sharedData->stagingBuffer = VK_NULL_HANDLE;
            sharedData->stagingBufferMapped = nullptr;
        }
    }


    if(sharedData->stagingBuffer == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size  = byteSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

        auto suc = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &sharedData->stagingBuffer, &sharedData->stagingBufferAllocation, nullptr);
        assert(suc == VK_SUCCESS);
        void * mapped = nullptr;
        suc = vmaMapMemory(allocator, sharedData->stagingBufferAllocation, &mapped);
        sharedData->stagingBufferMapped = mapped;
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->stagingBufferMapped, data, byteSize);
        vmaFlushAllocation(allocator, sharedData->stagingBufferAllocation, 0, VK_WHOLE_SIZE);
    }

    {
        auto fence = sharedData->commandPool.beginRecording([buffer=sharedData->stagingBuffer,this](auto cmd)
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

    if(sharedData->stagingBufferAllocation)
    {
        VmaAllocationInfo stagingBufferInfo = {};
        vmaGetAllocationInfo(allocator, sharedData->stagingBufferAllocation, &stagingBufferInfo);
        if(stagingBufferInfo.size < byteSize)
        {
            vmaUnmapMemory(sharedData->allocator, sharedData->stagingBufferAllocation);
            vmaDestroyBuffer(sharedData->allocator, sharedData->stagingBuffer, allocation);

            sharedData->stagingBufferAllocation = nullptr;
            sharedData->stagingBuffer = VK_NULL_HANDLE;
            sharedData->stagingBufferMapped = nullptr;
        }
    }


    if(sharedData->stagingBuffer == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size  = byteSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

        auto suc = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &sharedData->stagingBuffer, &sharedData->stagingBufferAllocation, nullptr);
        assert(suc == VK_SUCCESS);
        void * mapped = nullptr;
        suc = vmaMapMemory(allocator, sharedData->stagingBufferAllocation, &mapped);
        sharedData->stagingBufferMapped = mapped;
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->stagingBufferMapped, data, byteSize);
        vmaFlushAllocation(allocator, sharedData->stagingBufferAllocation, 0, VK_WHOLE_SIZE);
    }

    {
        auto fence = sharedData->commandPool.beginRecording([buffer=sharedData->stagingBuffer,width, height,arrayLayer, mipLevel, x_ImageOffset, y_ImageOffset, this](auto cmd)
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

    if(sharedData->stagingBufferAllocation)
    {
        VmaAllocationInfo stagingBufferInfo = {};
        vmaGetAllocationInfo(allocator, sharedData->stagingBufferAllocation, &stagingBufferInfo);
        if(stagingBufferInfo.size < byteSize)
        {
            vmaUnmapMemory(sharedData->allocator, sharedData->stagingBufferAllocation);
            vmaDestroyBuffer(sharedData->allocator, sharedData->stagingBuffer, allocation);

            sharedData->stagingBufferAllocation = nullptr;
            sharedData->stagingBuffer = VK_NULL_HANDLE;
            sharedData->stagingBufferMapped = nullptr;
        }
    }


    if(sharedData->stagingBuffer == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size  = byteSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

        auto suc = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &sharedData->stagingBuffer, &sharedData->stagingBufferAllocation, nullptr);
        assert(suc == VK_SUCCESS);
        void * mapped = nullptr;
        suc = vmaMapMemory(allocator, sharedData->stagingBufferAllocation, &mapped);
        sharedData->stagingBufferMapped = mapped;
    }

    // copy to staging buffer
    {
        std::memcpy(sharedData->stagingBufferMapped, data, byteSize);
        vmaFlushAllocation(allocator, sharedData->stagingBufferAllocation, 0, VK_WHOLE_SIZE);
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
        vkCmdCopyBufferToImage(m_updateCommandBuffer, sharedData->stagingBuffer, getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
}

inline void ImageInfo::beginUpdate()
{
    m_updateCommandBuffer = sharedData->commandPool.allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
}

inline void ImageInfo::endUpdate()
{
    GVK_CHECK_RESULT(vkEndCommandBuffer(m_updateCommandBuffer));
    auto fence = sharedData->commandPool.submitCommandBuffer(m_updateCommandBuffer, sharedData->commandPool.m_graphicsQueue, true);
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

}
#endif



