#ifndef GVU_CONTAINERS_BUFFERMAP_H
#define GVU_CONTAINERS_BUFFERMAP_H

#include <cstdint>
#include <limits>
#include <unordered_map>
#include "../Core/Cache/Objects.h"


namespace gvu
{

template<typename T>
struct storage_index
{
    uint32_t index = std::numeric_limits<uint32_t>::max();

    bool operator==(storage_index<T> const & right) const
    {
        return index==right.index;
    }
    bool valid() const
    {
        return index != std::numeric_limits<uint32_t>::max();
    }
};

/**
 * @brief The BufferMap class
 *
 * The BufferMap is similar to a std::map, it stores
 * a key/value pair, but the value is stored
 * in a Vulkan buffer at a particular location
 *
 *
 */
template<typename _Key, typename _Value>
struct BufferMap
{
    using key_type = _Key;
    using value_type = _Value;

    void setBuffer(gvu::BufferHandle h)
    {
        m_buffer = h;
    }

    /**
     * @brief insert
     * @param k
     * @param v
     * @return
     *
     * Inserts a key value into a the buffer-map and returns
     * the index where the value can be found.
     *
     * If the buffer was bound as a storage buffer:
     *
     * layout(set=0, binding = 0) buffer readonly s_Transform_t
     * {
     *     mat4 data[];
     * } s_Matrix;
     *
     * then you can pass the index into the shader as a push-constant
     * or some other method and get the data using the following:
     *
     *    s_Matrix.data[index];
     */
    storage_index<value_type> insert(key_type const & k, value_type const & v)
    {
        auto it = m_keyToIndex.find(k);

        size_t s = 0;

        // key doesn't already exist in the map
        if(it == m_keyToIndex.end())
        {
            if(m_availableIndex.size())
            {
                s = m_availableIndex.back();
                m_availableIndex.pop_back();
            }
            else
            {
                if(_count < maxSize())
                {
                    s = _count;
                    ++_count;
                }
                else
                {
                    return {};
                }
            }

            auto pa = m_keyToIndex.insert( {k, {v,s}});
            setValue(v, s);
        }
        else //
        {
            s = it->second.second;
        }
        setValue(v, s);
        return { uint32_t(s)};
    }

    /**
     * @brief remove
     * @param k
     * @return
     *
     * Removes the value associated with the key.
     * The index will be reused for the next key that
     * gets inserted
     */
    bool remove(key_type const & k)
    {
        auto it = m_keyToIndex.find(k);
        if(it == m_keyToIndex.end())
            return false;
        m_availableIndex.push_back(it->second->second);
        m_keyToIndex.erase(it);
        return true;
    }

    /**
     * @brief find
     * @param k
     * @return
     *
     * Find the index where the value to the key is stored
     */
    storage_index<value_type> find(key_type const & k) const
    {
        auto it = m_keyToIndex.find(k);
        if(it == m_keyToIndex.end())
            return {};
        return { uint32_t(it->second.second) };
    }

    /**
     * @brief size
     * @return
     *
     * Return the total number of
     * items in the map
     */
    size_t size() const
    {
        return m_keyToIndex.size();
    }

protected:
    void setValue(value_type const & v, size_t index)
    {
        m_buffer->setData(v,index);
    }
    size_t maxSize() const
    {
        return m_buffer->getBufferSize() / sizeof(value_type);
    }
    gvu::BufferHandle m_buffer;
    std::unordered_map<_Key, std::pair<value_type,size_t> > m_keyToIndex;
    std::vector<size_t> m_availableIndex;
    size_t _count=0;
};


}

#endif
