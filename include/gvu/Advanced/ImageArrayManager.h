#ifndef GVU_ADVANCED_IMAGEARRAYMANAGER_H
#define GVU_ADVANCED_IMAGEARRAYMANAGER_H
#pragma once

#include <gvu/Advanced/VulkanApplicationContext.h>

namespace gvu
{

/**
 * @brief The ImageArrayManager struct
 *
 * The ImageArrayManager manages a set of Arrays of Textures (not TextureArrays) and is meant
 * to be used with descriptor sets that look like the following:
 *
 * layout (set = 1, binding = 0) uniform sampler2D   u_TextureArray[maxImages];
 * layout (set = 1, binding = 1) uniform samplerCube u_TextureArray[maxImages];
 *
 * You add textures into the ArrayManager and it will place the texture somewhere
 * in the array. You can then query which index your texture is in
 * and pass that information to your shader so that it can be looked up.
 *
 *
 */
struct ImageArrayManager
{
    static constexpr uint32_t                      maxImages     = 1024;
    static constexpr uint32_t                      maxImageCubes = 1024;
    std::shared_ptr<gvu::VulkanApplicationContext> m_context = std::make_shared<gvu::VulkanApplicationContext>();

    void init(std::shared_ptr<gvu::VulkanApplicationContext> c )
    {
        m_context = c;

        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT ;

        gvu::DescriptorSetLayoutCreateInfo ci;
        ci.bindings.push_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxImages    , stageFlags, nullptr});
        ci.bindings.push_back(VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxImageCubes, stageFlags, nullptr});

        m_layout = c->descriptorSetLayoutCache.create(ci);

        // Create a dummy image which can be used when the texture you want to look up
        // doesn't exist
        {
            m_nullImage = c->memoryCache.allocateTexture2D(8,8,VK_FORMAT_R8G8B8A8_UNORM,1, VK_IMAGE_USAGE_SAMPLED_BIT);
            std::vector<uint8_t> _raw(8*8*4, 255);
            m_nullImage->setData(_raw.data());
        }

        {
            m_nullCubeImage = c->memoryCache.allocateTextureCube(8,VK_FORMAT_R8G8B8A8_UNORM,1, VK_IMAGE_USAGE_SAMPLED_BIT);
            std::vector<uint8_t> _raw(8*8*6, 255);
            m_nullCubeImage->setData(_raw.data());
        }

        images = std::vector<gvu::TextureHandle>(maxImages, m_nullImage);
        imageCubes = std::vector<gvu::TextureHandle>(maxImageCubes, m_nullCubeImage);

        for(uint32_t i=0;i<5;i++)
        {
            descriptorSets.push_back( {c->descriptorSetAllocator.allocate(m_layout), {}});
        }
        for(auto & x : descriptorSets)
        {
            for(uint32_t i=0;i<maxImages;i++)
            {
                x.dirty.insert(i);
            }
            for(uint32_t i=0;i<maxImageCubes;i++)
            {
                x.imageCubeDirty.insert(i);
            }
        }
    }

    /**
     * @brief insert
     * @param id
     * @return
     *
     * Inserts a texture into the manager and returns the
     * index of where it currently is in the texture array.
     *
     * returns 0 if the texture is not in the array.
     * 0 is always the null image, a bank white texture.
     *
     * Note that any textures inserted will not be reflected in the
     * descriptor set until updateDirty() is called
     */
    uint32_t insertTexture2D(gvu::TextureHandle id)
    {
        auto it = imageToIndex.find(id);
        if(it == imageToIndex.end())
        {
            uint32_t i=1;
            for(; i< images.size();i++)
            {
                if(images[i] == m_nullImage)
                {
                    images[i] = id;
                    setDirty(i);
                    return i;
                }
            }
            return 0;
        }
        return it->second;
    }



    /**
     * @brief findTexture
     * @param id
     * @return
     *
     * Finds a texture handle which had originally been placed
     * in the manager. Returns 0 if the texture handle cannot
     * be found
     */
    uint32_t findTexture2D(gvu::TextureHandle id) const
    {
        auto it = imageToIndex.find(id);
        if(it == imageToIndex.end())
        {
            return 0;
        }
        return it->second;
    }

    /**
     * @brief removeTexture
     * @param id
     *
     * Removes a texture from the array and replces it with
     * the null texture
     */
    void removeTexture2D(gvu::TextureHandle id)
    {
        uint32_t i=0;
        for(auto & x : images)
        {
            if(x==id)
            {
                x = m_nullImage;
                setDirty(i);
            }
            ++i;
        }
    }



    uint32_t insertTextureCube(gvu::TextureHandle id)
    {
        auto it = imageCubesToIndex.find(id);
        if(it == imageCubesToIndex.end())
        {
            uint32_t i=1;
            for(; i< imageCubes.size();i++)
            {
                if(imageCubes[i] == m_nullImage)
                {
                    imageCubes[i] = id;
                    setTextureCubeDirty(i);
                    return i;
                }
            }
            return 0;
        }
        return it->second;
    }


    uint32_t findTextureCube(gvu::TextureHandle id) const
    {
        auto it = imageCubesToIndex.find(id);
        if(it == imageCubesToIndex.end())
        {
            return 0;
        }
        return it->second;
    }


    void removeTextureCube(gvu::TextureHandle id)
    {
        uint32_t i=0;
        for(auto & x : images)
        {
            if(x==id)
            {
                x = m_nullImage;
                setTextureCubeDirty(i);
            }
            ++i;
        }
    }

    /**
     * @brief getDescriptorSet
     * @return
     *
     * Returns the current descriptor set in the chain.
     * Any images that have been insert()'d into the
     * manager will not be available
     */
    VkDescriptorSet getDescriptorSet() const
    {
        return descriptorSets.front().set;
    }

    /**
     * @brief shift
     *
     * Pops the active descriptor set and pushes it to the end of the
     * queue.
     */
    void shift()
    {
        std::rotate(descriptorSets.begin(), descriptorSets.begin() + 1, descriptorSets.end());
    }

    /**
     * @brief updateDirty
     * @param device
     *
     * Updates the first descriptor set in the chain. The one that
     * will be returned by getDescriptorSet
     */
    void updateDirty(VkDevice device)
    {
        auto set = descriptorSets.front().set;

        std::vector<VkDescriptorImageInfo> imageInfo;
        imageInfo.reserve(maxImages + maxImageCubes);
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(descriptorSets.front().dirty.size() + descriptorSets.front().imageCubeDirty.size());
        for(auto index : descriptorSets.front().dirty)
        {
            auto & I = imageInfo.emplace_back();
            I.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            I.imageView   = images.at(index)->getImageView();
            I.sampler     = images.at(index)->getLinearSampler();

            VkWriteDescriptorSet write = {};
            write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstArrayElement      = index;
            write.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.dstBinding           = 0;
            write.descriptorCount      = 1;
            write.pImageInfo           = &I;
            write.dstSet               = set;
            writes.push_back(write);
        }
        for(auto index : descriptorSets.front().imageCubeDirty)
        {
            auto & I = imageInfo.emplace_back();
            I.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            I.imageView   = imageCubes.at(index)->getImageView();
            I.sampler     = imageCubes.at(index)->getLinearSampler();

            VkWriteDescriptorSet write = {};
            write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstArrayElement      = index;
            write.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.dstBinding           = 1;
            write.descriptorCount      = 1;
            write.pImageInfo           = &I;
            write.dstSet               = set;
            writes.push_back(write);
        }

        if(writes.size())
        {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            std::cout << "Updated " << writes.size() << " sets" << std::endl;
            descriptorSets.front().dirty.clear();
            descriptorSets.front().imageCubeDirty.clear();
        }
    }

protected:
    gvu::TextureHandle m_nullImage;
    gvu::TextureHandle m_nullCubeImage;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;

    std::vector<gvu::TextureHandle>                  images;
    std::vector<gvu::TextureHandle>                  imageCubes;
    std::unordered_map<gvu::TextureHandle, uint32_t> imageToIndex;
    std::unordered_map<gvu::TextureHandle, uint32_t> imageCubesToIndex;

    struct _setInfo
    {
        VkDescriptorSet set = VK_NULL_HANDLE;
        std::unordered_set<uint32_t> dirty;
        std::unordered_set<uint32_t> imageCubeDirty;
    };

    std::vector<_setInfo>                     descriptorSets;

    void setTextureCubeDirty(uint32_t index)
    {
        for(auto & x : descriptorSets)
        {
            x.imageCubeDirty.insert(index);
        }
    }
    void setDirty(uint32_t index)
    {
        for(auto & x : descriptorSets)
        {
            x.dirty.insert(index);
        }
    }
};

}

#endif
