#ifndef GVU_TEXTURE_CACHE_OBJECTS_H
#define GVU_TEXTURE_CACHE_OBJECTS_H

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
#include <map>
#include <chrono>
#include <tuple>
#include <vk_mem_alloc.h>

#include <gvu/Core/FormatInfo.h>

namespace gvu
{

struct SharedData;

struct ImageViewRange
{
    uint32_t layer;
    uint32_t layerCount;
    uint32_t mip;
    uint32_t mipCount;
    VkImageViewType viewType;
    bool operator<(ImageViewRange const & r) const
    {
        return std::tie(layer,layerCount, mip, mipCount,viewType) < std::tie(r.layer, r.layerCount, r.mip, r.mipCount, viewType);
    }
};

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
     * @brief getImageView
     * @param layer
     * @param layerCount
     * @param mip
     * @param mipCount
     * @return
     *
     * Creates or returns an image view for the specific range
     */
    VkImageView getImageView(uint32_t layer, uint32_t layerCount, uint32_t mip, uint32_t mipCount, VkImageViewType type = VK_IMAGE_VIEW_TYPE_MAX_ENUM);

    /**
     * @brief getSingleImageSet
     * @param layer
     * @param mip
     * @return
     *
     * Returns a descriptor set on binding 0 for a specific layer/mip level
     * This is really only meant to be used for rendering images in ImGui
     */
    VkDescriptorSet getSingleImageSet(uint32_t layer, uint32_t mip);


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
    void copyData(void const * data, uint32_t width, uint32_t height,
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

    //=====================================================================================================================



    void transition(VkCommandBuffer c,
                    VkImageLayout oldLayout, VkImageLayout newLayout,
                    uint32_t baseMipLevel, uint32_t mipLevelCount,
                    uint32_t baseArrayLayer=0, uint32_t arrayLayerCount=VK_REMAINING_ARRAY_LAYERS,
                    VkPipelineStageFlags srcStage=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlags dstStage=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                    )
    {
        VkImageSubresourceRange range{};
        auto format = getFormat();

        switch(format)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// vk::ImageAspectFlagBits::eDepth;
                break;
            default:
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //vk::ImageAspectFlagBits::eColor;
                break;
        }
        //range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = baseMipLevel;
        range.levelCount     = mipLevelCount;
        range.baseArrayLayer = baseArrayLayer;
        range.layerCount     = arrayLayerCount;

        if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                        0,
                                        oldLayout,
                                        newLayout,
                                        srcStage,
                                        dstStage,
                                        range);
        }
        else
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        0,
                                        0,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                        range);
        }
    }

    /**
     * @brief transitionMipLevel
     * @param c
     * @param oldLayout
     * @param newLayout
     * @param mipLevel
     * @param mipLevelCount
     * @param srcStage
     * @param dstStage
     *
     * Specific specific mimpmap levels to another layout. This will convert ALL layers to the new layout
     */
    void transitionMipLevel(VkCommandBuffer c,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t mipLevel, uint32_t mipLevelCount=1,
                            VkPipelineStageFlags srcStage=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlags dstStage=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
    {
        transition(c, oldLayout, newLayout, mipLevel, mipLevelCount, 0, VK_REMAINING_ARRAY_LAYERS,srcStage, dstStage);
    }

    /**
     * @brief transitionArrayLayer
     * @param c
     * @param oldLayout
     * @param newLayout
     * @param arrayLayer
     * @param arrayLayerCount
     * @param srcStage
     * @param dstStage
     *
     * Transitions speciifc array layers to another layout. This will convert ALL mipmaps for that layotu
     */
    void transitionArrayLayer(VkCommandBuffer c,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t arrayLayer, uint32_t arrayLayerCount=1,
                            VkPipelineStageFlags srcStage=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlags dstStage=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
    {
        transition(c, oldLayout, newLayout, 0, VK_REMAINING_MIP_LEVELS, arrayLayer, arrayLayerCount,srcStage, dstStage);
    }

    /**
     * @brief transitionArrayLayerMipLevel
     * @param c
     * @param oldLayout
     * @param newLayout
     * @param arrayLayer
     * @param mipLevel
     * @param srcStage
     * @param dstStage
     *
     * Transitions a single array/miplevel to another layout
     */
    void transitionArrayLayerMipLevel(VkCommandBuffer c,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t arrayLayer, uint32_t mipLevel,
                            VkPipelineStageFlags srcStage=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkPipelineStageFlags dstStage=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
    {
        transition(c, oldLayout, newLayout, mipLevel, 1, arrayLayer, 1,srcStage, dstStage);
    }

    /**
     * @brief transitionForRendering
     * @param c
     *
     * Transitions this image so that it can be used for rendering
     * using Dynamic Rendering.
     * This only works if the image was created with the ATTACHMENT useage bit set
     */
    void transitionForRendering(VkCommandBuffer c)
    {
        VkImageSubresourceRange range{};
        auto format = getFormat();

        switch(format)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// vk::ImageAspectFlagBits::eDepth;
                break;
            default:
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //vk::ImageAspectFlagBits::eColor;
                break;
        }
        //range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount     = VK_REMAINING_ARRAY_LAYERS;

        if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        0,
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, range);
        }
        else
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        0,
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, range);

        }
    }

    /**
     * @brief transitionForSampling
     * @param c
     *
     * Transitions this image so that it can be used for sampling.
     * This is meant to be called after
     */
    void transitionForSampling(VkCommandBuffer c, VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        VkImageSubresourceRange range{};
        auto format = getFormat();

        switch(format)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// vk::ImageAspectFlagBits::eDepth;
                break;
            default:
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //vk::ImageAspectFlagBits::eColor;
                break;
        }
        //range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount     = VK_REMAINING_ARRAY_LAYERS;

        if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                        0,
                                        oldLayout,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                        range);
        }
        else
        {
            insert_image_memory_barrier(c,
                                        getImage(),
                                        0,
                                        0,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                        range);
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
    VkFormat getFormat() const
    {
        return info.format;
    }

protected:
    VkImage                  image      = VK_NULL_HANDLE;
    VkImageView              imageView  = VK_NULL_HANDLE;
    VkImageCreateInfo        info       = {};
    VmaAllocation            allocation = nullptr;
    VmaAllocationInfo        allocInfo  = {};
    VkImageViewType          viewType;
    std::vector<VkImageView> mipMapViews; // one image view per mipmap
    std::map<ImageViewRange, VkImageView> m_imageViews;
    uint64_t byteSize = 0;
    struct
    {
        VkSampler linear  = VK_NULL_HANDLE;
        VkSampler nearest = VK_NULL_HANDLE;

        //VkSampler current = VK_NULL_HANDLE; // do not destroy
    } sampler;

    // a map of descriptor sets which have been created
    // for a specific layer/mip level. Mostly only used
    // for ImGui
    std::map< std::pair<uint32_t, uint32_t>, VkDescriptorSet > arrayMipDescriptorSet;
    // a single descriptor set which is used for this
    // image so that it can be passed to IMGUI
    VkDescriptorSet singleDescriptorSet = VK_NULL_HANDLE;

    std::shared_ptr<SharedData> sharedData;
    bool selfManaged = true;
    VkCommandBuffer m_updateCommandBuffer = VK_NULL_HANDLE;
    friend class MemoryCache;

    static void insert_image_memory_barrier(
        VkCommandBuffer         command_buffer,
        VkImage                 image,
        VkAccessFlags           src_access_mask,
        VkAccessFlags           dst_access_mask,
        VkImageLayout           old_layout,
        VkImageLayout           new_layout,
        VkPipelineStageFlags    src_stage_mask,
        VkPipelineStageFlags    dst_stage_mask,
        VkImageSubresourceRange subresource_range)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask       = src_access_mask;
        barrier.dstAccessMask       = dst_access_mask;
        barrier.oldLayout           = old_layout;
        barrier.newLayout           = new_layout;
        barrier.image               = image;
        barrier.subresourceRange    = subresource_range;

        vkCmdPipelineBarrier(
            command_buffer,
            src_stage_mask,
            dst_stage_mask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }
};


struct BufferInfo
{
    /**
     * @brief setData
     * @param data
     * @param byteSize
     * @param offset
     *
     * Copies the data from the host to the buffer
     */
    void setData(void const *data, VkDeviceSize byteSize, VkDeviceSize offset);

    /**
     * @brief beginUpdate
     * @param data
     * @param size
     * @param offset
     *
     * Updates the buffer data using VkCmdUpdateBuffer. This is just here for
     * testing and probably not recommended to use.
     *
     * VkCmdUpdateBuffer can only write 2^16 bytes at a time.
     *
     */
    void beginUpdate(void const * data, VkDeviceSize size, VkDeviceSize offset);

    VkBuffer getBuffer() const
    {
        return buffer;
    }

    VkDeviceSize getBufferSize() const
    {
        return bufferInfo.size;
    }

    VkDeviceSize getAllocationSize() const
    {
        return allocationInfo.size;
    }

    /**
     * @brief mapData
     * @return
     *
     * Map data, but only if it is host mappable
     */
    void* mapData()
    {
        if(mapped)
        {
            return mapped;
        }
        vmaMapMemory(allocator, allocation, &mapped);
        return mapData();
    }
    void flush()
    {
        vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }
    void destroy()
    {
        if(mapped)
        {
            vmaUnmapMemory(allocator, allocation);
            mapped = nullptr;
        }
        vmaDestroyBuffer(allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = nullptr;
    }

    /**
     * @brief resize
     * @param size
     *
     * Reallocate the data
     */
    void resize(VkDeviceSize bytes);

    template<typename T>
    uint32_t pushStorage(T const *v, size_t count)
    {
        return static_cast<uint32_t>(push_back(v, count, sizeof(T) ));
    }
    void clearStorageIterator()
    {
        m_itr = 0;
    }
protected:
    VkBuffer                                  buffer         = VK_NULL_HANDLE;
    VmaAllocation                             allocation     = nullptr;
    VmaAllocationInfo                         allocationInfo = {};
    VmaAllocator                              allocator;
    VmaAllocationCreateInfo                   allocationCreateInfo = {};
    VkBufferCreateInfo                        bufferInfo           = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    void *                                    mapped               = nullptr;
    std::shared_ptr<SharedData>               sharedData;
    friend class MemoryCache;

    VkDeviceSize     m_itr    = 0;
    static auto _roundUp(size_t numToRound, size_t multiple) -> size_t
    {
        //assert(multiple);
        return ((numToRound + multiple - 1) / multiple) * multiple;
    };

    size_t push_back(void const * value, size_t count, uint32_t sizeofValue)
    {
        auto totalByteSize = sizeofValue*count;

        auto startIndex = _roundUp(m_itr, sizeofValue );

        if( startIndex + totalByteSize >= getBufferSize())
        {
            m_itr = 0;
            startIndex = 0;
            m_itr = startIndex;
        }

        auto m_mapped = static_cast<unsigned char*>(mapData());
        auto r = startIndex;
        std::memcpy(&m_mapped[startIndex],
                    value,
                    totalByteSize);

        m_itr = startIndex + count * sizeofValue;

        return r / sizeofValue;
    }

};

using BufferHandle   = std::shared_ptr<BufferInfo>;
using TextureHandle   = std::shared_ptr<ImageInfo>;
using WTextureHandle  = std::weak_ptr<ImageInfo>;


}
#endif



