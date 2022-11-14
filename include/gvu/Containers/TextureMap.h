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
    /**
     * @brief init
     * @param maxTextures
     * @param nullTexture
     *
     * Initialize the texture map with a specific size. You
     * should not change this size after it has been created.
     *
     * The maxTexture should be the same size as the array of
     * textures you plan on using in your shader, eg:
     *
     * layout (set = 1, binding = 0) uniform sampler2D   u_TextureArray[32];
     *
     * You must also pass it a null texture, this is a texture that will
     * be used when are no other textures in the array. This can be a
     * small white texture if you like
     */
    void init(size_t maxTextures, gvu::TextureHandle nullTexture)
    {
        m_textures.resize(maxTextures, nullTexture);
        m_textureInfo.resize(maxTextures);

        for(uint32_t i=0;i<m_textures.size();i++)
        {
            auto & d = m_textureInfo[i];
            d.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            d.sampler     = m_textures[i]->getLinearSampler();
            d.imageView   = m_textures[i]->getImageView();
            m_needsUpdate.push_back(i);

            m_freeIndices.push_back(maxTextures-1-i);
        }
        assert(m_freeIndices.back() == 0);
        m_freeIndices.pop_back();
    }

    /**
     * @brief destroy
     *
     * Clears all the textures releasing their shared pointers
     */
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
     *
     * The texture will be replaced with the null texture
     */
    void removeTexture(uint32_t i)
    {
        assert(i!=0);
        m_textureToIndex.erase( m_textures[i] );
        m_textures[i] = m_textures[0];
        m_textureInfo[i] = m_textureInfo[0];
        m_needsUpdate.push_back(i);
        m_freeIndices.push_back(i);
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
     *
     * Removes a texture from the array given the texture's handle
     *
     */
    void removeTexture(gvu::TextureHandle const & t)
    {
        auto i = getIndex(t);
        if(i>0)
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

        auto it = m_textureToIndex.find(t);
        if(it != m_textureToIndex.end())
            return it->second;

        assert(m_freeIndices.size() > 0);

        auto i = m_freeIndices.back();
        m_freeIndices.pop_back();

        assert(m_textures[i] == m_textures.front());

        m_textures[i] = t;
        m_textureInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_textureInfo[i].sampler     = m_textures[i]->getLinearSampler();
        m_textureInfo[i].imageView   = m_textures[i]->getImageView();

        m_needsUpdate.push_back(i);
        m_textureToIndex[t] = i;

        //for(uint32_t i=1; i<m_textures.size();i++)
        //{
        //    if(m_textures[i] == m_textures.front())
        //    {
        //        m_textures[i] = t;
        //        m_textureInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //        m_textureInfo[i].sampler     = m_textures[i]->getLinearSampler();
        //        m_textureInfo[i].imageView   = m_textures[i]->getImageView();
        //        m_needsUpdate.push_back(i);
        //        m_textureToIndex[t] = i;
        //        return i;
        //    }
        //}
        return i;
    }


    void setSampler(uint32_t i, VkFilter filter, VkSamplerAddressMode addressMode)
    {
        m_textureInfo[i].sampler = m_textures[i]->getSampler(filter, addressMode);
        m_needsUpdate.push_back(i);
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
     * Update all the texures that have been flagged as dirty
     *
     */
    uint32_t update(VkDescriptorSet s, uint32_t binding, VkDescriptorType ty)
    {
        if(m_needsUpdate.empty())
            return 0;

        std::vector<VkWriteDescriptorSet> writeSet;

        std::sort(m_needsUpdate.begin(), m_needsUpdate.end());
        auto it = std::unique(m_needsUpdate.begin(), m_needsUpdate.end());
        m_needsUpdate.erase(it, m_needsUpdate.end());

        writeSet.reserve(m_needsUpdate.size());

        for(auto i : m_needsUpdate)
        {
            auto & w = writeSet.emplace_back();
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = nullptr;
            w.dstSet = s;
            w.descriptorCount = 1;
            w.dstArrayElement = i;
            w.dstBinding = binding;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &m_textureInfo[i];
        }

        auto device = m_textures[0]->getDevice();
        vkUpdateDescriptorSets(device, uint32_t(writeSet.size()), &writeSet[0], 0, nullptr);

        m_needsUpdate.clear();

        return uint32_t(writeSet.size());
    }
protected:
    std::vector<gvu::TextureHandle>        m_textures;
    std::vector<VkDescriptorImageInfo>     m_textureInfo;
    std::unordered_map<gvu::TextureHandle, uint32_t> m_textureToIndex;
    std::vector<uint32_t>                  m_needsUpdate;
    std::vector<uint32_t>                  m_freeIndices;
};



}

#endif
