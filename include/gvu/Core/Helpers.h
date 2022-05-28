#ifndef GVU_HELPERS_H
#define GVU_HELPERS_H

#include "Cache/Objects.h"

namespace gvu
{

/**
 * @brief The BeginRendering struct
 *
 * This is a helper function used to set up the dynamic rendering
 *
 *   gvu::BeginRendering br;
 *   br.attachColor(0, m_fb_c)
 *     .setClearColor(0, {0.0f, 0.0f, 0.0f, 0.0f})
 *     .attachDepth(m_fb_d)
 *     .setClearDepth({1.0f,0})
 *     .begin(cmdBuffer);
 *
 *
 */
struct BeginRendering
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    VkRenderingAttachmentInfo              depthAttachment   = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingAttachmentInfo              stencilAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingInfo                        renderInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, {}, {}, 1};

    void begin(VkCommandBuffer cmd)
    {
        renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        renderInfo.pColorAttachments    = colorAttachments.data();
        renderInfo.pDepthAttachment     = depthAttachment.imageView ? &depthAttachment : nullptr;
        renderInfo.pStencilAttachment   = stencilAttachment.imageView ? &stencilAttachment : nullptr;;
        vkCmdBeginRendering(cmd, &renderInfo);
    }

    BeginRendering& setClearColor(uint32_t index, VkClearColorValue col)
    {
        if(index >= colorAttachments.size())
            colorAttachments.resize(index+1, {});
        colorAttachments[index].clearValue.color = col;
        return *this;
    }
    BeginRendering& setRenderArea( VkRect2D const & area)
    {
        renderInfo.renderArea = area;
        return *this;
    }

    BeginRendering& setClearDepth( VkClearDepthStencilValue col)
    {
        depthAttachment.clearValue.depthStencil = col;
        stencilAttachment.clearValue.depthStencil = col;
        return *this;
    }

    BeginRendering& attachColor(uint32_t index, gvu::TextureHandle i)
    {
        if(index >= colorAttachments.size())
            colorAttachments.resize(index+1, {});
        colorAttachments[index].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[index].pNext       = nullptr;
        colorAttachments[index].imageView   = i->getImageView();
        colorAttachments[index].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[index].resolveMode = VK_RESOLVE_MODE_NONE;
        colorAttachments[index].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachments[index].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachments[index].clearValue  = {{0.0f, 0.0f, 0.0f, 0.0f}};
        if(renderInfo.renderArea.extent.height*renderInfo.renderArea.extent.width == 0)
        {
            renderInfo.renderArea.extent = {i->getExtents().width,i->getExtents().height};
        }
        return *this;
    }

    BeginRendering& attachDepth(gvu::TextureHandle d)
    {
        depthAttachment.sType                        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.pNext                        = nullptr;
        depthAttachment.imageLayout                  = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.resolveMode                  = VK_RESOLVE_MODE_NONE;
        depthAttachment.loadOp                       = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp                      = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue                   = {1.0f, 0};
        if(d)
        {
            depthAttachment.imageView = d->getImageView();
        }
        return *this;
    }

    VkViewport getViewport() const
    {
        return VkViewport{ static_cast<float>(renderInfo.renderArea.offset.x),
                           static_cast<float>(renderInfo.renderArea.offset.y),
                           static_cast<float>(renderInfo.renderArea.extent.width),
                           static_cast<float>(renderInfo.renderArea.extent.height),0.f,1.0f};
    }
    VkRect2D getRenderArea() const
    {
        return renderInfo.renderArea;
    }
};

}

#endif
