#ifndef GVU_ADVANCED_IMAGEARRAYMANAGER_H
#define GVU_ADVANCED_IMAGEARRAYMANAGER_H
#pragma once

#include <gvu/Advanced/VulkanApplicationContext.h>
#include "../Containers/TextureMap.h"

namespace gvu
{

struct TextureArrayManager2
{
    struct Chain_t
    {
        TextureMap      images2D;
        TextureMap      imageCube;
        VkDescriptorSet set;
    };

    void init(std::shared_ptr<VulkanApplicationContext> ctx,
              uint32_t totalChains,
              uint32_t maxTextures, gvu::TextureHandle nullImage,
              uint32_t maxCubes,    gvu::TextureHandle nullCube,
              VkShaderStageFlags stageFlags)
    {
        m_context = ctx;
        m_chain.resize(totalChains);

        {
            gvu::DescriptorSetLayoutCreateInfo ci = {};

            {
                auto & tab = ci.bindings.emplace_back();
                tab.descriptorCount = maxTextures;
                tab.binding  = 0;
                tab.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                tab.pImmutableSamplers = nullptr;
                tab.stageFlags = stageFlags;
            }
            {
                auto & tab = ci.bindings.emplace_back();
                tab.descriptorCount = maxCubes;
                tab.binding  = 1;
                tab.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                tab.pImmutableSamplers = nullptr;
                tab.stageFlags = stageFlags;
            }
            m_layout = m_context->descriptorSetLayoutCache.create(ci);
        }

        for(auto & c : m_chain)
        {
            c.images2D.init(maxTextures, nullImage);
            c.imageCube.init(maxCubes, nullCube);

            c.set = m_context->allocateDescriptorSet(m_layout);
        }
    }

    void destroy()
    {
        for(auto & c : m_chain)
        {
            m_context->releaseDescriptorSet(c.set);
            c.images2D.destroy();
            c.imageCube.destroy();
        }
        *this = {};
    }

    /**
     * @brief updateDirty
     * @return
     *
     * Updates all the dirty descriptors and
     * returns the total number that haev beenupdated
     */
    uint32_t updateDirty()
    {
        auto & c = m_chain[m_currentChainIndex];
        return
            c.images2D.update(c.set  , 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            + c.imageCube.update(c.set, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    VkDescriptorSet getDescriptorSet() const
    {
        return m_chain[m_currentChainIndex].set;
    }

    void nextSet()
    {
        m_currentChainIndex = (m_currentChainIndex+1) % m_chain.size();
    }

    void setTextureCubeSampler(uint32_t i, VkFilter filter, VkSamplerAddressMode addressMode)
    {
        for(auto & c : m_chain)
        {
            c.imageCube.setSampler(i, filter ,addressMode);
        }
    }
    void setTextureSampler(uint32_t i, VkFilter filter, VkSamplerAddressMode addressMode)
    {
        for(auto & c : m_chain)
        {
            c.images2D.setSampler(i, filter ,addressMode);
        }
    }
    uint32_t getTextureCubeIndex(gvu::TextureHandle const &t) const
    {
        auto & c = m_chain[m_currentChainIndex];
        return c.imageCube.getIndex(t);
    }
    uint32_t getTexture2DIndex(gvu::TextureHandle const &t) const
    {
        auto & c = m_chain[m_currentChainIndex];
        return c.images2D.getIndex(t);
    }

    uint32_t insertTextureCube(gvu::TextureHandle const &t)
    {
        for(auto & c : m_chain)
        {
            c.imageCube.insertTexture(t);
        }
        return getTextureCubeIndex(t);
    }

    uint32_t insertTexture(gvu::TextureHandle const &t)
    {
        for(auto & c : m_chain)
        {
            c.images2D.insertTexture(t);
        }
        return getTexture2DIndex(t);
    }

protected:
    std::shared_ptr<VulkanApplicationContext> m_context;
    std::vector<Chain_t>                      m_chain;
    VkDescriptorSetLayout                     m_layout;
    uint32_t                                  m_currentChainIndex=0;
};

}

#endif
