#ifndef GVU_TEXTURE_MAP_H
#define GVU_TEXTURE_MAP_H

#include <vector>
#include <unordered_map>
#include "../Core/Cache/TextureCache.h"

namespace gvu
{
/**
 * @brief The TextureArrayManager class
 *
 * The TextureMap is used to manage an array of textures which can be
 * looked up in a shader.
 *
 * You must initialize it with the size of the array you want to use
 * in your shader, and a nullTexture. The nullTexture is the texture
 * used when no other texture is available
 *
 * TextureMap TM;
 * TM.init(32, nullTexture);
 *
 * auto myTexture = memoryCache.allocateTexture(...)
 * auto index = TM.insertTexture(myTexture);
 *
 * You can then pass 'index' into your shader as a push constant
 * which can be looked up in the array.
 *
 * In the shader your texture declaration would look something like this:
 *
 * layout (set = 1, binding = 0) uniform sampler2D   u_TextureArray[32];
 *
 */
struct TextureMap
{
    void init(size_t maxTextures, gvu::TextureHandle nullTexture)
    {
        m_textures.resize(maxTextures, nullTexture);
        for(uint32_t i=0;i<m_textures.size();i++)
        {
            m_needsUpdate.push_back(i);
        }
    }

    void destroy()
    {
        *this = {};
    }
    /**
     * @brief removeTexture
     * @param i
     *
     * Removes a texture at the particular index.
     * i must be > 0. You cannot remove the null
     * texure.
     */
    void removeTexture(uint32_t i)
    {
        assert(i!=0);
        m_textureToIndex.erase( m_textures[i] );
        m_textures[i] = m_textures[0];
        m_needsUpdate.push_back(i);
    }

    /**
     * @brief getIndex
     * @param t
     * @return
     *
     * Get the index where the texture is located. Returns 0 (the nulltexture)
     * if the texture is not managed by this texture map
     */
    uint32_t getIndex(gvu::TextureHandle const & t) const
    {
        auto it = m_textureToIndex.find(t);
        if(it==m_textureToIndex.end()) return 0;
        return it->second;
    }

    /**
     * @brief removeTexture
     * @param t
     */
    void removeTexture(gvu::TextureHandle const & t)
    {
        auto i = getIndex(t);
        removeTexture(i);
    }

    /**
     * @brief insertTexture
     * @param t
     * @return
     *
     * Inserts a texture into the texture map and return
     * the index. The inserted texture must be of teh same
     * ImageViewType() as the nullTexture.
     */
    uint32_t insertTexture(gvu::TextureHandle const &t)
    {
        assert(t->getImageViewType() == m_textures[0]->getImageViewType());

        for(uint32_t i=1; i<m_textures.size();i++)
        {
            if(m_textures[i] == m_textures.front())
            {
                m_textures[i] = t;
                m_needsUpdate.push_back(i);
                return i;
            }
        }
        return 0;
    }

    /**
     * @brief dirtyCount
     * @return
     *
     * Returns the total number of textures that
     * are flagged as dirty and need to be updated
     *
     */
    size_t dirtyCount() const
    {
        return m_needsUpdate.size();
    }

    /**
     * @brief arraySize
     * @return
     *
     * Returns the size of the texture array
     */
    uint32_t arraySize() const
    {
        return uint32_t(m_textures.size());
    }
    /**
     * @brief update
     * @param s
     * @param binding
     *
     * Update a descriptor set with all the dirty
     */
    uint32_t update(VkDescriptorSet s, uint32_t binding, VkDescriptorType ty)
    {
        if(m_needsUpdate.empty())
            return 0;

        std::vector<VkWriteDescriptorSet> m_write;
        std::vector<VkDescriptorImageInfo> imageWrites;

        std::sort(m_needsUpdate.begin(), m_needsUpdate.end());
        auto it = std::unique(m_needsUpdate.begin(), m_needsUpdate.end());
        m_needsUpdate.erase(it, m_needsUpdate.end());

        imageWrites.reserve(m_needsUpdate.size());
        m_write.reserve(m_needsUpdate.size());

        for(auto i : m_needsUpdate)
        {
            auto & d = imageWrites.emplace_back();
            d.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            d.sampler     = m_textures[i]->getLinearSampler();
            d.imageView   = m_textures[i]->getImageView();

            auto & w = m_write.emplace_back();
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = nullptr;
            w.dstSet = s;
            w.descriptorCount = 1;
            w.dstArrayElement = i;
            w.dstBinding = binding;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &d;
        }

        auto device = m_textures[0]->getDevice();
        vkUpdateDescriptorSets(device, uint32_t(m_write.size()), &m_write[0], 0, nullptr);
        m_needsUpdate.clear();
        return uint32_t(m_write.size());
    }

    std::vector<gvu::TextureHandle>        m_textures;
    std::unordered_map<gvu::TextureHandle, uint32_t> m_textureToIndex;
    std::vector<uint32_t>                  m_needsUpdate;
};



}

#endif
