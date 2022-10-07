#ifndef GVU_DESCRIPTOR_POOL_MANAGER_H
#define GVU_DESCRIPTOR_POOL_MANAGER_H

#include <memory>
#include <iostream>
#include <map>
#include <cassert>
#include <vector>
#include <vulkan/vulkan.h>
#include <unordered_set>
#include "../Cache/DescriptorSetLayoutCache.h"

namespace gvu
{

/**
 * @brief The DescriptorPoolManager class
 *
 * This clas manages descriptor pools for a specific layout.
 *
 * It requires access to a DescriptorLayoutCache
 */
class DescriptorPoolManager
{
public:
    #pragma message "We should rename this to SingleLayoutDescriptorSetAllocator"

    /**
     * @brief init
     * @param device
     * @param layout - the layout that this pool will use
     * @param cache
     *
     * Initialize the descriptor pool queue.
     */
    void init(VkDevice device,
              DescriptorSetLayoutCache *cache,
              VkDescriptorSetLayout layout,
              uint32_t maxSetsPerPool = 10
              )
    {
        m_device                = device;
        m_layout                = layout;

        m_createInfo.maxSets = maxSetsPerPool;

        auto & layoutInfo = cache->getCreateInfo(layout);
        std::map<VkDescriptorType, uint32_t> sizeMap;
        for(auto x : layoutInfo.bindings)
        {
            sizeMap[x.descriptorType] += x.descriptorCount;
        }
        m_poolSizes.clear();
        for(auto & x : sizeMap)
        {
            auto &s           = m_poolSizes.emplace_back();
            s.descriptorCount = x.second * maxSetsPerPool;
            s.type            = x.first;
        }

        createNewPool();
    }

    /**
     * @brief destroy
     *
     * Destroy the DescriptorPool queue.
     */
    void destroy()
    {
        freePools();
        for(auto & [p, i] : m_poolInfos)
        {
            vkDestroyDescriptorPool(m_device, p, nullptr);
        }
        m_poolInfos.clear();
    }

    /**
     * @brief releaseToPool
     * @param set
     *
     * Release a descriptor set back to its pool. After using this
     * the descriptor set is no longer usable!
     */
    void releaseToPool(VkDescriptorSet set)
    {
        auto p = m_setToPool.at(set);
        auto & i = m_poolInfos.at(p);
        ++i.returnedSets;
        if( i.returnedSets == m_createInfo.maxSets )
        {
            #pragma message "Rather than reset the pool. Set a flag saying that a pool needs to be reset. On next allocation, reset the pool"
            resetPool(p);
        }
    }

    /**
     * @brief allocateDescriptorSet
     * @return
     *
     * Allocate a descriptorSet from this manager. The descriptor set has the
     */
    VkDescriptorSet allocateDescriptorSet()
    {
        VkDescriptorSet set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

        for(auto & [p, i] : m_poolInfos)
        {
            allocInfo.descriptorPool     = p;
            allocInfo.pSetLayouts        = &m_layout;
            allocInfo.descriptorSetCount = 1;

            if( i.allocatedSets < i.maxSets)
            {
                auto result = vkAllocateDescriptorSets(m_device, &allocInfo, &set);

                switch (result)
                {
                    case VK_SUCCESS:
                        //all good, return
                        ++i.allocatedSets;
                        m_setToPool[set] = p;
                        i.sets.insert(set);
                        return set;
                    case VK_ERROR_FRAGMENTED_POOL:
                    case VK_ERROR_OUT_OF_POOL_MEMORY:
                        //reallocate pool
                    default:
                        //unrecoverable error
                        throw std::runtime_error("Unrecoverable error");
                }
            }
        }

        createNewPool();
        return allocateDescriptorSet();
    }

    /**
     * @brief getPool
     * @param s
     * @return
     *
     * Get the pool the descriptor set was allocated from
     */
    VkDescriptorPool getPool(VkDescriptorSet s) const
    {
        auto it = m_setToPool.find(s);
        if(it==m_setToPool.end())
            return VK_NULL_HANDLE;
        return it->second;
    }

    /**
     * @brief resetAllAvailablePools
     *
     * Resets all the pools that have had their descriptor sets returned to them
     */
    void resetAllAvailablePools(bool forceResetAll=false)
    {
        if(forceResetAll)
        {
            for(auto & [p,i] : m_poolInfos)
            {
                resetPool(p);
            }

            m_setToPool.clear();
        }

        for(auto & [p,i] : m_poolInfos)
        {
            if(i.returnedSets == m_createInfo.maxSets &&
               i.allocatedSets == m_createInfo.maxSets)
            {
                resetPool(p);
            }
        }
    }
    bool isResetable(VkDescriptorPool p) const
    {
        return m_poolInfos.at(p).returnedSets == m_poolInfos.at(p).maxSets;
    }
    size_t allocatedSetsCount(VkDescriptorPool p) const
    {
        return m_poolInfos.at(p).maxSets-m_poolInfos.at(p).returnedSets;
    }
    size_t allocatedPoolCount() const
    {
        return m_poolInfos.size();
    }

protected:
    void freePools()
    {
        for(auto & [p,i] : m_poolInfos)
        {
            resetPool(p);
        }
    }

    void resetPool(VkDescriptorPool p)
    {
        auto & I = m_poolInfos.at(p);
        for(auto s : I.sets)
        {
            m_setToPool.erase(s);
        }

        vkResetDescriptorPool(m_device, p, {});
        m_poolInfos.at(p).allocatedSets = 0;
        m_poolInfos.at(p).returnedSets = 0;
        m_poolInfos.at(p).sets.clear();
    }

    VkDescriptorPool createNewPool()
    {
        m_createInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        m_createInfo.pPoolSizes    = m_poolSizes.data();
        m_createInfo.poolSizeCount = static_cast<uint32_t>(m_poolSizes.size());

        VkDescriptorPool pool;
        auto result = vkCreateDescriptorPool(m_device, &m_createInfo, nullptr, &pool);
        if( result != VK_SUCCESS)
        {
            throw std::runtime_error("Error creating Descriptor Pool");
        }

        auto & i = m_poolInfos[pool];
        i.pool = pool;
        i.maxSets = m_createInfo.maxSets;
        i.allocatedSets = 0;

        return pool;
    }


    struct PoolInfo
    {
        VkDescriptorPool pool;
        uint32_t allocatedSets=0;
        uint32_t returnedSets=0;
        uint32_t maxSets=0;
        std::unordered_set<VkDescriptorSet> sets;
    };

    std::unordered_map<VkDescriptorPool, PoolInfo> m_poolInfos;

    VkDevice                          m_device;
    VkDescriptorSetLayout             m_layout;
    std::vector<VkDescriptorPoolSize> m_poolSizes;
    std::unordered_map<VkDescriptorSet, VkDescriptorPool> m_setToPool;
    VkDescriptorPoolCreateInfo        m_createInfo = {};
};

class DescriptorSetAllocator
{
public:
    void init(DescriptorSetLayoutCache * cache)
    {
        m_cache = cache;
    }
    void destroy()
    {
        for(auto & [l, p] : m_pools)
        {
            p->destroy();
        }
        m_pools.clear();
    }
    VkDescriptorSet allocate(DescriptorSetLayoutCreateInfo const &ci)
    {
        auto l = m_cache->create(ci);
        return allocate(l);
    }

    VkDescriptorSet allocate(VkDescriptorSetLayout layout)
    {
        auto i = m_pools.find(layout);
        if(i == m_pools.end())
        {
            auto p = std::make_shared<DescriptorPoolManager>();
            p->init(m_cache->getDevice(), m_cache, layout, 10);
            m_pools[layout] = p;
            return allocate(layout);
        }
        else
        {
            auto set =  i->second->allocateDescriptorSet();
            m_setToLayout[set] = layout;
            return set;
        }
    }

    void releaseToPool(VkDescriptorSet set)
    {
        auto l = m_setToLayout.at(set);
        m_pools.at(l)->releaseToPool(set);
    }

protected:

    DescriptorSetLayoutCache * m_cache;
    std::unordered_map<VkDescriptorSetLayout, std::shared_ptr<DescriptorPoolManager> > m_pools;
    std::unordered_map<VkDescriptorSet, VkDescriptorSetLayout> m_setToLayout;
};

}

#endif
