#ifndef GVU_DESCRIPTOR_SET_UPDATER_H
#define GVU_DESCRIPTOR_SET_UPDATER_H

#include <vector>
#include <vulkan/vulkan.h>

namespace gvu
{

/**
 * @brief The DescriptorSetUpdater struct
 *
 * Helper class to update descriptors within a descriptor set
 * The following shows show to update 3 descriptors: two textures in an array
 * and one buffer on its own
 *
 *  gvu::DescriptorSetUpdater updater;
 *  updater.setDescriptorSet(set)
 *         .setArrayElement(0)
 *         .setDescriptorType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
 *              .setBinding(0)
 *              .setArrayElement(0) // start at array
 *              .appendTexture(sampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
 *              .appendTexture(sampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
 *              .update(device)
 *         .setDescriptorType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
 *              .setBinding(1)
 *              .setArrayElement(0)
 *              .appendBuffer(buffer, 0, range)
 *              .update(m_context->getDevice());
 */
struct DescriptorSetUpdater
{
    DescriptorSetUpdater& appendTexture(VkSampler sampler, VkImageView view, VkImageLayout layout)
    {
        imageWrites.push_back( VkDescriptorImageInfo{sampler, view, layout});
        return *this;
    }
    DescriptorSetUpdater& appendBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        bufferWrites.push_back( VkDescriptorBufferInfo{buffer, offset, range});
        return *this;
    }
    DescriptorSetUpdater& clearDescriptors()
    {
        bufferWrites.clear();
        imageWrites.clear();
        return *this;
    }
    DescriptorSetUpdater& setDescriptorSet(VkDescriptorSet b)
    {
        set = b;
        return *this;
    }
    DescriptorSetUpdater& setBinding(uint32_t b)
    {
        binding = b;
        return *this;
    }
    DescriptorSetUpdater& setArrayElement(uint32_t a)
    {
        dstArrayElement = a;
        return *this;
    }
    DescriptorSetUpdater& setDescriptorType(VkDescriptorType a)
    {
        descriptorType = a;
        return *this;
    }
    DescriptorSetUpdater& update(VkDevice device)
    {
        VkWriteDescriptorSet m_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        m_write.dstSet               = set;
        m_write.dstBinding           = binding;
        m_write.dstArrayElement      = dstArrayElement;

        m_write.descriptorType       = descriptorType;
        switch(descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            m_write.descriptorCount      = static_cast<uint32_t>(imageWrites.size());
            m_write.pImageInfo           = imageWrites.data();
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            m_write.descriptorCount      = static_cast<uint32_t>(bufferWrites.size());
            m_write.pBufferInfo          = bufferWrites.data();
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            break;
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            break;
        case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
            break;
        case VK_DESCRIPTOR_TYPE_MAX_ENUM:
            break;
        }

        vkUpdateDescriptorSets(device, 1, &m_write, 0, nullptr);
        clearDescriptors();
        return *this;
    }
protected:
    VkDescriptorSet                           set = VK_NULL_HANDLE;
    uint32_t                                  binding = 0;
    uint32_t                                  dstArrayElement = 0;
    VkDescriptorType                          descriptorType;
    std::vector<VkDescriptorImageInfo>        imageWrites;
    std::vector<VkDescriptorBufferInfo>       bufferWrites;
};

}
#endif
