#ifndef GVU_DESCRIPTOR_SET_LAYOUT_CACHE_h
#define GVU_DESCRIPTOR_SET_LAYOUT_CACHE_h

#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <cassert>
#include "Cache_t.h"

namespace gvu
{

struct DescriptorSetLayoutCreateInfo
{
    using create_info_type = VkDescriptorSetLayoutCreateInfo;
    using object_type = VkDescriptorSetLayout;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutCreateFlags          flags = {};

    size_t hash() const
    {
        std::hash<size_t> Hs;
        auto h = Hs(bindings.size());

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            std::hash<std::decay_t<decltype(v)> > hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        };

        for(auto & b : bindings)
        {
            hash_combine(h, b.descriptorCount);
            hash_combine(h, static_cast<uint32_t>(b.descriptorType));
            hash_combine(h, static_cast<uint32_t>(b.stageFlags));
        }
        hash_combine(h, static_cast<uint32_t>(flags));
        return h;
    }

    bool operator==(DescriptorSetLayoutCreateInfo const & B) const
    {
        if(bindings.size() == B.bindings.size())
        {
            return std::equal(bindings.begin(),bindings.end(),B.bindings.begin(),
                       [](auto const & a, auto const & b)
            {
                return std::tie(a.binding,a.descriptorCount,a.descriptorType,a.stageFlags,a.pImmutableSamplers) ==
                        std::tie(b.binding,b.descriptorCount,b.descriptorType,b.stageFlags,b.pImmutableSamplers);
            });
        }
        return true;
    }

    DescriptorSetLayoutCreateInfo() = default;

    DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateInfo const & info)
    {
        flags = info.flags;
        for(uint32_t i=0;i<info.bindingCount;i++)
        {
            if(info.pBindings[i].pImmutableSamplers == nullptr)
            {
                throw std::runtime_error("Immutable samplers not currently supported");
            }

            bindings.push_back(info.pBindings[i]);
        }
    }

    template<typename callable_t>
    void generateVkCreateInfo(callable_t && c) const
    {
        create_info_type ci = {};
        ci.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.pBindings          = bindings.data();
        ci.bindingCount       = static_cast<uint32_t>(bindings.size());
        ci.flags              = flags;
        c(ci);
    }

    static object_type create(VkDevice device, create_info_type const & C)
    {
        object_type obj = VK_NULL_HANDLE;
        auto result = vkCreateDescriptorSetLayout(device, &C, nullptr, &obj);
        if( result != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return obj;
    }

    static void destroy(VkDevice device, object_type c)
    {
        vkDestroyDescriptorSetLayout(device, c, nullptr);
    }

};

using DescriptorSetLayoutCache = Cache_t<DescriptorSetLayoutCreateInfo>;

}

#endif
