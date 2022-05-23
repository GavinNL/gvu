#ifndef GVU_SAMPLER_CACHE_h
#define GVU_SAMPLER_CACHE_h

#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <cassert>
#include "Cache_t.h"

namespace gvu
{

struct SamplerCreateInfo
{
    using create_info_type = VkSamplerCreateInfo;
    using object_type      = VkSampler;

    VkSamplerCreateFlags flags                   = {};
    VkFilter             magFilter               = VK_FILTER_LINEAR;
    VkFilter             minFilter               = VK_FILTER_LINEAR;
    VkSamplerMipmapMode  mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float                mipLodBias              = 0.0f;
    VkBool32             anisotropyEnable        = VK_FALSE;
    float                maxAnisotropy           = 1;
    VkBool32             compareEnable           = VK_FALSE;
    VkCompareOp          compareOp               = VK_COMPARE_OP_ALWAYS;
    float                minLod                  = 0;
    float                maxLod                  = VK_LOD_CLAMP_NONE;
    VkBorderColor        borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32             unnormalizedCoordinates = VK_FALSE;

    size_t hash() const
    {
        size_t h = 156485465u;

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            using type = std::decay_t<decltype(v)>;
            if constexpr (std::is_enum_v<type>)
            {
                auto s = static_cast<size_t>(v);
                std::hash<size_t> hasher;
                seed ^= hasher(s) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            }
            else
            {
                std::hash<type> hasher;
                seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            }
        };

        hash_combine(h, flags);
        hash_combine(h, magFilter);
        hash_combine(h, minFilter);
        hash_combine(h, mipmapMode);
        hash_combine(h, addressModeU);
        hash_combine(h, addressModeV);
        hash_combine(h, addressModeW);
        hash_combine(h, mipLodBias);
        hash_combine(h, anisotropyEnable);
        hash_combine(h, maxAnisotropy);
        hash_combine(h, compareEnable);
        hash_combine(h, compareOp);
        hash_combine(h, minLod);
        hash_combine(h, maxLod);
        hash_combine(h, borderColor);
        hash_combine(h, unnormalizedCoordinates);

        return h;
    }

    bool operator==(SamplerCreateInfo const & B) const
    {
        return
        flags                       == B.flags
        && magFilter                == B.magFilter
        && minFilter                == B.minFilter
        && mipmapMode               == B.mipmapMode
        && addressModeU             == B.addressModeU
        && addressModeV             == B.addressModeV
        && addressModeW             == B.addressModeW
        && mipLodBias               == B.mipLodBias
        && anisotropyEnable         == B.anisotropyEnable
        && maxAnisotropy            == B.maxAnisotropy
        && compareEnable            == B.compareEnable
        && compareOp                == B.compareOp
        && minLod                   == B.minLod
        && maxLod                   == B.maxLod
        && borderColor              == B.borderColor
        && unnormalizedCoordinates  == B.unnormalizedCoordinates;
    }

    SamplerCreateInfo()
    {
    }
    SamplerCreateInfo(create_info_type const & info)
    {
        flags                   = info.flags                  ;
        magFilter               = info.magFilter              ;
        minFilter               = info.minFilter              ;
        mipmapMode              = info.mipmapMode             ;
        addressModeU            = info.addressModeU           ;
        addressModeV            = info.addressModeV           ;
        addressModeW            = info.addressModeW           ;
        mipLodBias              = info.mipLodBias             ;
        anisotropyEnable        = info.anisotropyEnable       ;
        maxAnisotropy           = info.maxAnisotropy          ;
        compareEnable           = info.compareEnable          ;
        compareOp               = info.compareOp              ;
        minLod                  = info.minLod                 ;
        maxLod                  = info.maxLod                 ;
        borderColor             = info.borderColor            ;
        unnormalizedCoordinates = info.unnormalizedCoordinates;
    }

    template<typename callable_t>
    void generateVkCreateInfo(callable_t && c) const
    {
        VkSamplerCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.flags                   = flags                  ;
        ci.magFilter               = magFilter              ;
        ci.minFilter               = minFilter              ;
        ci.mipmapMode              = mipmapMode             ;
        ci.addressModeU            = addressModeU           ;
        ci.addressModeV            = addressModeV           ;
        ci.addressModeW            = addressModeW           ;
        ci.mipLodBias              = mipLodBias             ;
        ci.anisotropyEnable        = anisotropyEnable       ;
        ci.maxAnisotropy           = maxAnisotropy          ;
        ci.compareEnable           = compareEnable          ;
        ci.compareOp               = compareOp              ;
        ci.minLod                  = minLod                 ;
        ci.maxLod                  = maxLod                 ;
        ci.borderColor             = borderColor            ;
        ci.unnormalizedCoordinates = unnormalizedCoordinates;
        c(ci);
    }

    static object_type create(VkDevice device, create_info_type const & C)
    {
        object_type obj = VK_NULL_HANDLE;
        auto result = vkCreateSampler(device, &C, nullptr, &obj);
        if( result != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return obj;
    }
    static void destroy(VkDevice device, object_type c)
    {
        vkDestroySampler(device, c, nullptr);
    }
};


using SamplerCache = Cache_t<SamplerCreateInfo>;

}
#endif
