#ifndef GVU_RENDER_PASS_CACHE_H
#define GVU_RENDER_PASS_CACHE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <optional>
#include <cassert>
#include "Cache_t.h"

namespace gvu
{

struct SubpassDescription
{
    VkSubpassDescriptionFlags          flags;
    VkPipelineBindPoint                pipelineBindPoint;
    std::vector<VkAttachmentReference> inputAttachments;
    std::vector<VkAttachmentReference> colorAttachments;
    std::vector<VkAttachmentReference>  resolveAttachments;
    std::optional<VkAttachmentReference>  depthStencilAttachment;
    std::vector<uint32_t>              preserveAttachments;

    SubpassDescription() = default;
    SubpassDescription(VkSubpassDescription const & B)
    {
        flags = B.flags;
        pipelineBindPoint = B.pipelineBindPoint;
        for(uint32_t i=0;i<B.inputAttachmentCount;i++)
        {
            inputAttachments.push_back(B.pInputAttachments[i]);
        }
        for(uint32_t i=0;i<B.colorAttachmentCount;i++)
        {
            colorAttachments.push_back(B.pColorAttachments[i]);
        }
        if(B.pResolveAttachments)
        {
            for(uint32_t i=0;i<B.colorAttachmentCount;i++)
            {
                colorAttachments.push_back(B.pResolveAttachments[i]);
            }
        }
        if(B.pDepthStencilAttachment)
        {
            depthStencilAttachment = *B.pDepthStencilAttachment;
        }
        for(uint32_t i=0;i<B.preserveAttachmentCount;i++)
        {
            preserveAttachments.push_back(B.pPreserveAttachments[i]);
        }
    }

    VkSubpassDescription createDescription() const
    {
        VkSubpassDescription d;
        d.flags = flags;
        d.pipelineBindPoint = pipelineBindPoint;

        d.inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size());
        d.pInputAttachments    = inputAttachments.data();

        d.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        d.pColorAttachments = colorAttachments.data();

        d.pResolveAttachments = resolveAttachments.data();

        if(depthStencilAttachment)
        {
            d.pDepthStencilAttachment = &depthStencilAttachment.value();
        }
        d.preserveAttachmentCount = static_cast<uint32_t>(preserveAttachments.size());
        d.pPreserveAttachments = preserveAttachments.data();

        return d;
    }
    size_t hash() const
    {
        std::hash<size_t> Hs;
        auto h = Hs(flags);

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            std::hash<std::decay_t<decltype(v)> > hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        };

        hash_combine(h, flags);
        hash_combine(h, pipelineBindPoint);
        for(auto & b : inputAttachments)
        {
            hash_combine(h, b.attachment);
            hash_combine(h, b.layout);
        }
        for(auto & b : colorAttachments)
        {
            hash_combine(h, b.attachment);
            hash_combine(h, b.layout);
        }
        for(auto & b : resolveAttachments)
        {
            hash_combine(h, b.attachment);
            hash_combine(h, b.layout);
        }
        hash_combine(h, depthStencilAttachment.has_value());
        if(depthStencilAttachment.has_value())
        {
            hash_combine(h, depthStencilAttachment->attachment);
            hash_combine(h, depthStencilAttachment->layout);
        }
        for(auto & b : preserveAttachments)
        {
            hash_combine(h, b);
        }

        return h;
    }

    bool operator==(SubpassDescription const & B) const
    {
        auto attachRefEq = [](VkAttachmentReference const & a, VkAttachmentReference const & b)
        {
            return std::tie(a.attachment,a.layout) ==
                   std::tie(b.attachment,b.layout);

        };

        if(depthStencilAttachment.has_value() != depthStencilAttachment.has_value() )
            return false;

        if(depthStencilAttachment.has_value())
        {
            if(!attachRefEq(*depthStencilAttachment, *B.depthStencilAttachment))
            {
                return false;
            }
        }
        return
                 flags == B.flags
                 && pipelineBindPoint == B.pipelineBindPoint
                 && std::equal(preserveAttachments.begin(), preserveAttachments.end(), B.preserveAttachments.begin())
                 && std::equal(colorAttachments.begin(), colorAttachments.end(), B.colorAttachments.begin(),attachRefEq)
                 && std::equal(inputAttachments.begin(), inputAttachments.end(), B.inputAttachments.begin(),attachRefEq)
                 && std::equal(resolveAttachments.begin(), resolveAttachments.end(), B.resolveAttachments.begin(),attachRefEq);
    }

};

struct RenderPassCreateInfo
{
    using create_info_type = VkRenderPassCreateInfo;
    using object_type      = VkRenderPass;

    VkRenderPassCreateFlags                flags = {};
    std::vector<VkAttachmentDescription>   attachments;
    std::vector<SubpassDescription>        subpasses;
    std::vector<VkSubpassDependency>       dependencies;

    static RenderPassCreateInfo createSimpleRenderPass(std::vector< std::pair<VkFormat,VkImageLayout> > colors,
                                                       std::pair<VkFormat,VkImageLayout> depthFormat = {VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED})
    {
        RenderPassCreateInfo R;

        for(auto & c : colors)
            R.attachments.emplace_back(VkAttachmentDescription{ {},
                                       c.first,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_ATTACHMENT_STORE_OP_STORE,
              /*stencil load*/         VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              /*stencil load*/         VK_ATTACHMENT_STORE_OP_DONT_CARE,
              /*initial layout*/       VK_IMAGE_LAYOUT_UNDEFINED,
              /*final   layout*/       c.second});

        if( depthFormat.first != VK_FORMAT_UNDEFINED)
            R.attachments.emplace_back(VkAttachmentDescription{ {},
                                       depthFormat.first,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_ATTACHMENT_STORE_OP_STORE,
              /*stencil load*/         VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              /*stencil load*/         VK_ATTACHMENT_STORE_OP_DONT_CARE,
              /*initial layout*/       VK_IMAGE_LAYOUT_UNDEFINED,
              /*final   layout*/       depthFormat.second});

        R.dependencies.resize(2);
        R.dependencies[0].srcSubpass    = VK_SUBPASS_EXTERNAL;								// Producer of the dependency
        R.dependencies[0].dstSubpass    = 0;													// Consumer is our single subpass that will wait for the execution depdendency
        R.dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        R.dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        R.dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        R.dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        R.dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Second dependency at the end the renderpass
        // Does the transition from the initial to the final layout
        R.dependencies[1].srcSubpass      = 0;													// Producer of the dependency is our single subpass
        R.dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;								// Consumer are all commands outside of the renderpass
        R.dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        R.dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        R.dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;// vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        R.dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        R.dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


        auto & s = R.subpasses.emplace_back();

        s.pipelineBindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS;

        uint32_t i=0;
        for(auto & c : colors)
        {
            (void)c;
            s.colorAttachments.emplace_back( VkAttachmentReference{i++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        }


        if( depthFormat.first !=  VK_FORMAT_UNDEFINED)
            s.depthStencilAttachment =  VkAttachmentReference{i++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        return R;
    }
    size_t hash() const
    {
        std::hash<size_t> Hs;
        auto h = Hs(flags);

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            std::hash<std::decay_t<decltype(v)> > hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        };

        hash_combine(h, flags);
        for(auto & b : subpasses)
        {
            hash_combine(h, b.hash());
        }
        for(auto & b : attachments)
        {
            hash_combine(h, b.flags);
            hash_combine(h, b.format);
            hash_combine(h, b.samples);
            hash_combine(h, b.loadOp);
            hash_combine(h, b.storeOp);
            hash_combine(h, b.stencilLoadOp);
            hash_combine(h, b.stencilStoreOp);
            hash_combine(h, b.initialLayout);
            hash_combine(h, b.finalLayout);
        }
        for(auto & b : dependencies)
        {
            hash_combine(h, b.srcSubpass);
            hash_combine(h, b.dstSubpass);
            hash_combine(h, b.srcStageMask);
            hash_combine(h, b.dstStageMask);
            hash_combine(h, b.srcAccessMask);
            hash_combine(h, b.dstAccessMask);
            hash_combine(h, b.dependencyFlags);
        }

        return h;
    }

    bool operator==(RenderPassCreateInfo const & B) const
    {
        return   flags == B.flags
                 && std::equal(attachments.begin(), attachments.end(), B.attachments.begin() ,[](auto && a, auto && b)
                    {
                         return std::tie(
                         a.flags,
                         a.format,
                         a.samples,
                         a.loadOp,
                         a.storeOp,
                         a.stencilLoadOp,
                         a.stencilStoreOp,
                         a.initialLayout,
                         a.finalLayout) ==
                                 std::tie(
                                 b.flags,
                                 b.format,
                                 b.samples,
                                 b.loadOp,
                                 b.storeOp,
                                 b.stencilLoadOp,
                                 b.stencilStoreOp,
                                 b.initialLayout,
                                 b.finalLayout);
                    })
                 && std::equal(subpasses.begin(), subpasses.end(), B.subpasses.begin())
                 && std::equal(dependencies.begin(), dependencies.end(), B.dependencies.begin(),[](auto && a, auto && b)
                    {
                        return
                        std::tie(
                          a.srcSubpass,
                          a.dstSubpass,
                          a.srcStageMask,
                          a.dstStageMask,
                          a.srcAccessMask,
                          a.dstAccessMask,
                          a.dependencyFlags) ==
                                std::tie(
                                  b.srcSubpass,
                                  b.dstSubpass,
                                  b.srcStageMask,
                                  b.dstStageMask,
                                  b.srcAccessMask,
                                  b.dstAccessMask,
                                  b.dependencyFlags);

                    });
    }

    RenderPassCreateInfo() = default;
    RenderPassCreateInfo(create_info_type const & info)
    {
        flags = info.flags;
        for(uint32_t i=0;i<info.attachmentCount;i++)
        {
            attachments.push_back( info.pAttachments[i]);
        }
        for(uint32_t i=0;i<info.subpassCount;i++)
        {
            subpasses.push_back(SubpassDescription(info.pSubpasses[i]));
        }
        for(uint32_t i=0;i<info.dependencyCount;i++)
        {
            dependencies.push_back( info.pDependencies[i]);
        }
    }

    template<typename callable_t>
    void generateVkCreateInfo(callable_t && c) const
    {
        create_info_type ci = {};
        ci.sType              = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.flags = flags;

        ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        ci.pAttachments = attachments.data();

        std::vector<VkSubpassDescription> desc;
        for(auto & d : subpasses)
            desc.push_back(d.createDescription());
        ci.subpassCount = static_cast<uint32_t>(subpasses.size());
        ci.pSubpasses = desc.data();

        ci.dependencyCount = static_cast<uint32_t>(dependencies.size());
        ci.pDependencies = dependencies.data();


        c(ci);
    }

    static object_type create(VkDevice device, create_info_type const & C)
    {
        object_type obj = VK_NULL_HANDLE;
        auto result = vkCreateRenderPass(device, &C, nullptr, &obj);
        if( result != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return obj;
    }
    static void destroy(VkDevice device, object_type c)
    {
        vkDestroyRenderPass(device, c, nullptr);
    }

};

using RenderPassCache = Cache_t<RenderPassCreateInfo>;

}

#endif
