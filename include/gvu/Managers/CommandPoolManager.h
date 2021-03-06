#ifndef GVU_COMMAND_POOL_MANAGER_H
#define GVU_COMMAND_POOL_MANAGER_H

#include <memory>
#include <iostream>
#include <cassert>
#include <vector>
#include <vulkan/vulkan.h>

namespace gvu
{

#define LDEBUG(...)
#if defined(__ANDROID__)
#define GVK_CHECK_RESULT(f)																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        LOGE("Fatal : VkResult is \" %d \" in %s at line %d", res, __FILE__, __LINE__);					\
        assert(res == VK_SUCCESS);																		\
    }																									\
}
#else
#define GVK_CHECK_RESULT(f)																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        std::cout << "Fatal : VkResult is \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
        assert(res == VK_SUCCESS);																		\
    }																									\
}
#endif


struct ScopedFence
{
    ~ScopedFence()
    {
        if( m_fence != VK_NULL_HANDLE)
        {
            wait();
            destroy();
        }
    }

    ScopedFence()
    {
    }
    ScopedFence(ScopedFence const &) = delete ;
    ScopedFence& operator = (ScopedFence const&) = delete ;

    ScopedFence(ScopedFence && A) = default;
    ScopedFence& operator = (ScopedFence && A) = default;

    void wait() const
    {
        LDEBUG("Waiting on fence");
        GVK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));
        LDEBUG("Waiting finished");
    }

    bool ready() const
    {
        return VK_SUCCESS == vkGetFenceStatus(m_device, m_fence);
    }

    void destroy()
    {
        if( m_fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_device, m_fence, nullptr);
        }
        if( m_buffer != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_device, m_pool, 1, &m_buffer);
        }

        m_device = VK_NULL_HANDLE;
        m_fence = VK_NULL_HANDLE;
        m_pool= VK_NULL_HANDLE;
        m_buffer= VK_NULL_HANDLE;
    }

    VkDevice m_device = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    VkCommandPool m_pool= VK_NULL_HANDLE;
    VkCommandBuffer m_buffer= VK_NULL_HANDLE;
};


struct CommandBuffer
{
    CommandBuffer() = default;
    CommandBuffer(VkCommandBuffer _c):cmd(_c){}
    operator VkCommandBuffer() const
    {
        return cmd;
    }


    /**
      Transition the image to a different layout. The default values are provided so that
      the entire image will be transitioned
    **/
    void imageTransitionLayout( VkImage image,
                                VkImageLayout oldLayout,
                                VkImageLayout newLayout,
                                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS})
    {
        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout        = oldLayout;
        imageMemoryBarrier.newLayout        = newLayout;
        imageMemoryBarrier.srcAccessMask    = 0;
        imageMemoryBarrier.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.image            = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }

    /**
     * @brief imageCopyFromBuffer
     * @param _img
     * @param _imgDstOffset
     * @param _imgDstExtent
     * @param _imgDstArrayBaseLayer
     * @param _imgDstMipLevel
     * @param _buffer
     * @param _srcBufferImageWidth
     * @param _srcBufferImageHeight
     *
     * Copy image data from a buffer to the image
     */
    void imageCopyFromBuffer(VkImage    _img,
                             VkOffset3D _imgDstOffset,
                             VkExtent3D _imgDstExtent,
                             uint32_t   _imgDstArrayBaseLayer,
                             uint32_t   _imgDstMipLevel,
                             VkBuffer   _buffer,
                             uint32_t   _srcBufferImageWidth  = 0,
                             uint32_t   _srcBufferImageHeight = 0)
    {
        //auto & m_images = m_image2D.images;
        // do buffer copy
        VkBufferImageCopy region = {};

        region.bufferOffset                    = 0;
        region.bufferRowLength                 = _srcBufferImageWidth;
        region.bufferImageHeight               = _srcBufferImageHeight;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.baseArrayLayer = _imgDstArrayBaseLayer;
        region.imageSubresource.layerCount     = 1;
        region.imageSubresource.mipLevel       = _imgDstMipLevel;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = _imgDstOffset;
        region.imageExtent                     = _imgDstExtent;

        vkCmdCopyBufferToImage(
            cmd,
            _buffer,
            _img,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(1),
            &region
        );
    }
    VkCommandBuffer cmd = VK_NULL_HANDLE;
};

struct CommandPoolManager
{
    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue;

    //==========================
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue)
    {
        m_graphicsQueue = graphicsQueue;
        m_physicalDevice = physicalDevice;
        m_device = device;

        m_pool = createCommandPool(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT);
    }

    void destroy()
    {
        vkDestroyCommandPool(m_device, m_pool, nullptr);
        m_device = VK_NULL_HANDLE;
        m_physicalDevice = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
    }
    //=====================

    /**
     * @brief allocateCommandBuffer
     * @param level
     * @param begin
     * @return
     *
     * Allocate a command buffer, if begin==true, then it will also call vkBeginCommandBuffer().
     *
     * It is recommended to use the beginRecording function instead
     *
     * auto scopedFence = beginRecording([](auto cmd)
     * {
     *
     * }, true);
     *
     *  // fence is waited on when scopedFence goes out of scope
     */
    VkCommandBuffer allocateCommandBuffer(VkCommandBufferLevel level, bool begin)
    {
        VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
        cmdBufAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufAllocateInfo.commandPool        = m_pool;
        cmdBufAllocateInfo.level              = level;
        cmdBufAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuffer;
        GVK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &cmdBuffer));

        // If requested, also start recording for the new command buffer
        if (begin)
        {
            VkCommandBufferBeginInfo commandBufferBI{};
            commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            GVK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &commandBufferBI));
        }

        return cmdBuffer;
    }

    /**
     * @brief freeCommandBuffer
     * @param cmd
     *
     * Free a command buffer
     */
    void freeCommandBuffer(VkCommandBuffer cmd)
    {
        vkFreeCommandBuffers(m_device, m_pool, 1, &cmd);
    }

    /**
     * @brief submitCommandBuffer
     * @param commandBuffer
     * @param queue
     * @param waitTillFinished
     * @return
     *
     * Submits a command buffer to a particular queue.
     * Returns a scoped fence which can be used to wait for the
     * command buffer to finish
     */
    std::unique_ptr<ScopedFence> submitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool generateFence)
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if(generateFence)
        {
            // Create fence to ensure that the command buffer has finished executing
            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;

            GVK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

            GVK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));

            auto D = std::make_unique<ScopedFence>();
            D->m_fence  = fence;
            D->m_device = m_device;
            D->m_pool   = m_pool;
            D->m_buffer = commandBuffer;

            return D;
        }
        else
        {
            GVK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
            return nullptr;
        }
    }


    template<typename Callable_t>
    std::unique_ptr<ScopedFence> beginRecording(Callable_t && c, bool returnFence)
    {
        VkCommandBuffer cmd = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        c(cmd);

        GVK_CHECK_RESULT(vkEndCommandBuffer(cmd));

        return submitCommandBuffer(cmd, m_graphicsQueue, returnFence);
    }





protected:



    VkCommandPool createCommandPool( VkQueueFlags queueFlagBits, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    {
        VkCommandPoolCreateInfo cmdPoolInfo = {};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        cmdPoolInfo.queueFamilyIndex = getQueueFamilyIndex(queueFlagBits, m_physicalDevice);

        cmdPoolInfo.flags = createFlags;
        VkCommandPool cmdPool;
        GVK_CHECK_RESULT(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &cmdPool));
        return cmdPool;
    }

    static uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags, VkPhysicalDevice physicalDevice)
    {
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        assert(queueFamilyCount > 0);
        queueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        // Dedicated queue for compute
        // Try to find a queue family index that supports compute but not graphics
        if (queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
                if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
                    return i;
                    break;
                }
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if (queueFamilyProperties[i].queueFlags & queueFlags) {
                return i;
                break;
            }
        }

        throw std::runtime_error("Could not find a matching queue family index");
    }

};

}


#endif
