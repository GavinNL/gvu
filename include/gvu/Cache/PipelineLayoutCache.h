#ifndef GVU_PIPELINE_LAYOUT_CACHE_H
#define GVU_PIPELINE_LAYOUT_CACHE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <cassert>
#include "Cache_t.h"

namespace gvu
{

struct PipelineLayoutCreateInfo
{
    using create_info_type = VkPipelineLayoutCreateInfo;
    using object_type      = VkPipelineLayout;

    VkPipelineLayoutCreateFlags          flags = {};
    std::vector<VkDescriptorSetLayout>   setLayouts;
    std::vector<VkPushConstantRange>     pushConstantRanges;

    size_t hash() const
    {
        std::hash<size_t> Hs;
        auto h = Hs(flags);

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            std::hash<std::decay_t<decltype(v)> > hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        };

        for(auto & b : pushConstantRanges)
        {
            hash_combine(h, b.stageFlags);
            hash_combine(h, static_cast<uint32_t>(b.offset));
            hash_combine(h, static_cast<uint32_t>(b.size));
        }
        for(auto & b : setLayouts)
        {
            hash_combine(h, b);
        }
        hash_combine(h, static_cast<uint32_t>(flags));
        return h;
    }

    bool operator==(PipelineLayoutCreateInfo const & B) const
    {
        return   flags == B.flags
                 && setLayouts.size() == B.setLayouts.size()
                 && pushConstantRanges.size() == B.pushConstantRanges.size()
                 && std::equal(setLayouts.begin(), setLayouts.end(), B.setLayouts.begin())
                 && std::equal(pushConstantRanges.begin(), pushConstantRanges.end(), B.pushConstantRanges.begin(),[](auto && a, auto && b)
                    {
                        return std::tie(a.stageFlags,a.offset,a.size) ==
                               std::tie(b.stageFlags,b.offset,b.size);

                    });
    }

    PipelineLayoutCreateInfo() = default;
    PipelineLayoutCreateInfo(create_info_type const & info)
    {
        flags = info.flags;
        for(uint32_t i=0;i<info.pushConstantRangeCount;i++)
        {
            pushConstantRanges.push_back( info.pPushConstantRanges[i]);
        }
        for(uint32_t i=0;i<info.setLayoutCount;i++)
        {
            setLayouts.push_back( info.pSetLayouts[i]);
        }
    }

    template<typename callable_t>
    void generateVkCreateInfo(callable_t && c) const
    {
        create_info_type ci = {};
        ci.sType              = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.pSetLayouts        = setLayouts.data();
        ci.setLayoutCount     = static_cast<uint32_t>(setLayouts.size());
        ci.pPushConstantRanges= pushConstantRanges.data();
        ci.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        ci.flags              = flags;
        c(ci);
    }

    static object_type create(VkDevice device, create_info_type const & C)
    {
        object_type obj = VK_NULL_HANDLE;
        auto result = vkCreatePipelineLayout(device, &C, nullptr, &obj);
        if( result != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return obj;
    }
    static void destroy(VkDevice device, object_type c)
    {
        vkDestroyPipelineLayout(device, c, nullptr);
    }

};

using PipelineLayoutCache = Cache_t<PipelineLayoutCreateInfo>;

}

#endif
