#ifndef GVU_CACHE_T_H
#define GVU_CACHE_T_H

#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <cassert>

namespace gvu
{

template<typename gvuCreateInfo>
class Cache_t
{
    public:
        using vk_createInfo_type = typename gvuCreateInfo::create_info_type;
        using createInfo_type    = gvuCreateInfo;
        using object_type        = typename gvuCreateInfo::object_type;

        void init(VkDevice newDevice)
        {
            m_device = newDevice;
        }

        void destroy()
        {
            for(auto & l : m_cache)
            {
                createInfo_type::destroy(m_device, l.second);
            }
            m_cache.clear();
        }

        /**
         * @brief createDescriptorSetLayout
         * @param info
         * @return
         *
         * Create a descriptorSetLayout from the custom DescriptorLayoutInfo struct
         */
        object_type create(createInfo_type const & info)
        {
            return _create(info);
        }

        size_t cacheSize() const
        {
            return m_cache.size();
        }
        /**
         * @brief getLayoutInfo
         * @param l
         * @return
         *
         * Return the DescriptorSetLayoutCreateInfo for a particular layout. If
         * the layout doesn't exist, it will throw an error
         */
        createInfo_type const & getCreateInfo(object_type l) const
        {
            for(auto & x : m_cache)
            {
                if(x.second == l)
                {
                    return x.first;
                }
            }
            throw std::out_of_range("This object was not created in this cache");
        }

        VkDevice getDevice() const
        {
            return m_device;
        }
    private:
        object_type _create(createInfo_type const & info)
        {
            object_type obj = VK_NULL_HANDLE;

            auto it = m_cache.find(info);
            if(it != m_cache.end())
            {
                return it->second;
            }
            info.generateVkCreateInfo([&](auto & Ci)
            {
                obj = createInfo_type::create(m_device, Ci);
            });

            if(obj != VK_NULL_HANDLE)
            {
                m_cache[info] = obj;
                return obj;
            }
            throw std::runtime_error("Could not create object");
        }

        struct _hasher
        {
            std::size_t operator()(const createInfo_type& k) const{
                return k.hash();
            }
        };

        std::unordered_map<createInfo_type, object_type, _hasher> m_cache;
        VkDevice m_device;
};

}

#endif
