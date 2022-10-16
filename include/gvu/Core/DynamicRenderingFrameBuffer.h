#ifndef GVU_DYNAMIC_RENDERING_FRAMEBUFFER_H
#define GVU_DYNAMIC_RENDERING_FRAMEBUFFER_H

#include <gvu/Core/Cache/TextureCache.h>

namespace gvu
{

/**
 * @brief The DynamicRenderingFrameBuffer struct
 *
 * The DyanmicRenderingFrameBuffer object is used to help
 * setup rendering to non-swapchain targets
 *
 * DynamicRenderingFrameBuffer FB;
 *
 * FB.setColorAttachment(0, colorTexture);
 * FB.setDepthAttachment(depthTexture);
 *
 * FB.beginRendering(cmd);
 *
 *
 *
 * FB.endRendering(cmd);
 *
 * NOTE: when calling setColorAttachment, the class keeps a copy
 * of the shared pointer reference.
 */
struct DynamicRenderingFrameBuffer
{
    void setColorAttachment(uint32_t attachmentIndex, gvu::TextureHandle h)
    {
        if(colorImages.size() == 0)
        {
            setRenderArea(0,0, h->getExtents().width, h->getExtents().height);
        }
        colorImages.resize( std::max<size_t>(attachmentIndex,colorImages.size()+1), {});
        colorAttachments.resize( std::max<size_t>(attachmentIndex,colorAttachments.size()+1), {});

        colorImages.at(attachmentIndex) = h;
        auto & colorAttachment = colorAttachments.at(attachmentIndex);
        colorAttachment.sType            = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView        = h->getImageView();
        colorAttachment.imageLayout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachment.resolveMode      = VK_RESOLVE_MODE_NONE;
        colorAttachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        //colorAttachment.clearValue.color.float32 = { 0.0f,0.0f,0.0f,0.0f };

    }

    void setDepthAttachment(gvu::TextureHandle h)
    {
        depthAttachment.sType            = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView        = h->getImageView();
        depthAttachment.imageLayout      = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        //colorAttachment.clearValue.color.float32 = { 0.0f,0.0f,0.0f,0.0f };
        depthAttachment.clearValue                   = {1.0f, 0};
        depthImage = h;

       // if(gvu::getFormatInfo(h->getFormat()).flags & gvu::FORMAT_SIZE_STENCIL_BIT)
       // {
       //
       // }
    }

    auto getDepthAttachmentImage() const
    {
        return depthImage;
    }
    auto getColorAttachmentImage(uint32_t index) const
    {
        return colorImages.at(index);
    }

    void setClearDepthStencilValue(float depth, uint32_t stencil)
    {
        depthAttachment.clearValue.depthStencil.depth = depth;
        depthAttachment.clearValue.depthStencil.stencil = stencil;
    }

    void setClearColorValue(uint32_t attachmentIndex, float r,float g,float b,float a)
    {
        colorAttachments.at(attachmentIndex).clearValue.color.float32[0] = r;
        colorAttachments.at(attachmentIndex).clearValue.color.float32[1] = g;
        colorAttachments.at(attachmentIndex).clearValue.color.float32[2] = b;
        colorAttachments.at(attachmentIndex).clearValue.color.float32[3] = a;
    }

    void setClearColorValue(uint32_t attachmentIndex, int32_t r,int32_t g,int32_t b,int32_t a)
    {
        colorAttachments.at(attachmentIndex).clearValue.color.int32[0] = r;
        colorAttachments.at(attachmentIndex).clearValue.color.int32[1] = g;
        colorAttachments.at(attachmentIndex).clearValue.color.int32[2] = b;
        colorAttachments.at(attachmentIndex).clearValue.color.int32[3] = a;
    }
    void setClearColorValue(uint32_t attachmentIndex, uint32_t r,uint32_t g,uint32_t b,uint32_t a)
    {
        colorAttachments.at(attachmentIndex).clearValue.color.uint32[0] = r;
        colorAttachments.at(attachmentIndex).clearValue.color.uint32[1] = g;
        colorAttachments.at(attachmentIndex).clearValue.color.uint32[2] = b;
        colorAttachments.at(attachmentIndex).clearValue.color.uint32[3] = a;
    }

    void setLoadOp(uint32_t attachmentIndex, VkAttachmentLoadOp op)
    {
        colorAttachments.at(attachmentIndex).loadOp = op;
    }
    void setStoreOp(uint32_t attachmentIndex, VkAttachmentStoreOp op)
    {
        colorAttachments.at(attachmentIndex).storeOp = op;
    }
    void setRenderArea(uint32_t offsetx, uint32_t offsety,uint32_t width, uint32_t height)
    {
        renderingInfo.renderArea = { {int32_t(offsetx), int32_t(offsety)},{ width, height }};
    }

    VkViewport getViewport() const
    {
        return VkViewport{ static_cast<float>(renderingInfo.renderArea.offset.x),
                           static_cast<float>(renderingInfo.renderArea.offset.y),
                           static_cast<float>(renderingInfo.renderArea.extent.width),
                           static_cast<float>(renderingInfo.renderArea.extent.height),0.f,1.0f};
    }
    VkRect2D getRenderArea() const
    {
        return renderingInfo.renderArea;
    }
    /**
     * @brief beginRendering
     * @param cmd
     * @param convertImages
     *
     * Starts rendering to the images you have supplied.
     *
     * if convertImages is set to true, it will transition the layout
     * of the images to the appropriate layout for rendering
     *
     * you should calle endRendering() when you are finished
     * rendering to this framebuffer
     */
    void beginRendering(VkCommandBuffer cmd, bool convertImages=true, bool defaultViewPortAndScissor=true)
    {
        //VkRenderingInfoKHR renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        //renderingInfo.renderArea = { 0, 0, width, height };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        renderingInfo.pColorAttachments    = colorAttachments.data();
        renderingInfo.pDepthAttachment     = depthAttachment.imageView != VK_NULL_HANDLE ? &depthAttachment : nullptr;
        renderingInfo.pStencilAttachment   = stencilAttachment.imageView != VK_NULL_HANDLE ? &stencilAttachment : nullptr;

        if(convertImages)
        {
            for(auto & i : colorImages)
            {
                i->transitionForRendering(cmd);
            }
            if(depthImage)
                depthImage->transitionForRendering(cmd);
        }

        // Begin dynamic rendering
        vkCmdBeginRendering(cmd, &renderingInfo);

        if(defaultViewPortAndScissor)
        {
            VkViewport vp = getViewport();
            auto ra = getRenderArea();
            vkCmdSetScissor( cmd,0, 1, &ra);
            vkCmdSetViewport(cmd,0, 1, &vp);
        }
    }

    /**
     * @brief endRendering
     * @param cmd
     * @param convertImagesToSampled
     *
     * End the rendering to the images supplied
     * If convertImagesToSampled == true (default)
     * then it will convert all the images so that they can be
     * sampled in another shader
     */
    void endRendering(VkCommandBuffer cmd,
                      bool convertColorImagesToSampled=true,
                      bool convertDepthImagesToSampled=true)
    {
        vkCmdEndRendering(cmd);
        if(convertColorImagesToSampled)
        {
            for(auto & i : colorImages)
            {
                i->transitionForSampling(cmd);
            }
        }
        if(convertDepthImagesToSampled && depthImage)
            depthImage->transitionForSampling(cmd);
    }

    std::vector<gvu::TextureHandle>           colorImages;
    gvu::TextureHandle                        depthImage;

    std::vector<VkRenderingAttachmentInfo   > colorAttachments = {};
    VkRenderingAttachmentInfo                 depthAttachment = {};
    VkRenderingAttachmentInfo                 stencilAttachment = {};
    VkRenderingInfo                           renderingInfo = {};
};



}

#endif
