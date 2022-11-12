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
 * @brief The BufferVector class
 *
 * A BufferVector is similar to a std::vector, but the data
 * is stored in a vulkan storage buffer
 *
 * You must call setBuffer( ) and provide it a handle to the buffer
 * you want to use.
 *
 * The buffer can be host visible or GPU only. If the bufer is
 * GPU only, the vector will allocate a std::vector internally
 * in host memory
 */
template<typename _Value, typename _bufferHandleType=BufferHandle>
struct BufferVector
{
    using value_type  = _Value;
    using buffer_type = _bufferHandleType;

    bool _isMappable = false;

    BufferVector()
    {
    }

    BufferVector(buffer_type h)
    {
        setBuffer(h);
    }

    void setBuffer(buffer_type h)
    {
        m_buffer = h;
        _isMappable = h->isMappable();
    }

    /**
     * @brief capacity
     * @return
     *
     * Returns the maximum number of elements this buffer can hold.
     *
     */
    size_t capacity() const
    {
        return m_buffer->getBufferSize() / sizeof(value_type);
    }

    /**
     * @brief size
     * @return
     *
     * Returns the current size of the vector. This will return
     * the maxium size (capacity()) if the buffer is host mapable.
     * If the buffer is GPU only, i will return the size of the
     * internal host vector
     */
    size_t size() const
    {
        return m_hostData.size();

    }
    /**
     * @brief clear
     *
     * Clears the host vector and sets its size to zero. Data
     * in the GPU buffer will remain unchanged
     */
    void clear()
    {
        m_hostData.clear();
    }

    /**
     * @brief push_back
     * @param v
     *
     * Pushes data onto the host vector and sets
     * that element as dirty. The GPU buffer will
     * not be updated until updateDirty( )
     * is called.
     *
     * This will fail if the
     */
    void push_back(value_type && v)
    {
        m_hostData.push_back(v);
        m_pushDirty.push_back( m_hostData.size() - 1);
    }

    /**
     * @brief setDirty
     * @param index
     *
     * Sets a specific index to be flagged as dirty
     * so that it will be updated the next time updateDirty()
     * is called.
     *
     * If the underlying buffer is host mappable, this value
     * is updated immediately
     */
    void setDirty(size_t index)
    {
        if(!_isMappable)
        {
            m_pushDirty.push_back(index);
        }
        else
        {
            m_buffer->setData( m_hostData[index], index);
        }
    }

    /**
     * @brief resize
     * @param s
     *
     * Resizes the host data vector. This does not
     * change any data on the GPU
     */
    void resize(size_t s)
    {
        if(s >= capacity())
            throw std::runtime_error("Base buffer is not large enough");
        m_hostData.resize(s);
    }

    /**
     * @brief setValue
     * @param v
     * @param index
     *
     * Sets the value of a specific index. If the buffer is
     * host mappable, the value will be updated immediately.
     * If the buffer is GPU only, the data will be cached
     * and updated when updateDirty( ) is called
     */
    void setValue(size_t index, value_type const & v)
    {
        m_hostData.at(index) = v;
        setDirty(index);
    }

    /**
     * @brief at
     * @param index
     * @return
     *
     * Returns the host value at the index specified.
     * If you modify this value, you must call setDirty(index) after
     * to update it in the GPU.
     *
     */
    value_type& at(size_t index)
    {
        return m_hostData.at(index);
    }
    value_type const & at(size_t index) const
    {
        return m_hostData.at(index);
    }
    /**
     * @brief requiredStagingBufferSize
     * @return
     *
     * Returns the size of the staging buffer required
     * to update the dirty buffer values. This
     * value will return 0 if no data needs to be updated
     * or if the buffer is host mappable
     */
    VkDeviceSize requiredStagingBufferSize() const
    {
        return m_pushDirty.size() * sizeof(value_type);
    }

    /**
     * @brief pushDirty
     * @param cmd
     * @param h
     *
     * Copies all the dirty values from the host into the
     * gpu buffer. You must provide a staging buffer which
     * is at least requiredStagingBufferSize()
     *
     * If the buffer is host mappable, this function does nothing
     */
    template<typename _stagingBufferHandle>
    void pushDirty(VkCommandBuffer cmd, _stagingBufferHandle h)
    {
        if(_isMappable)
            return;

        std::vector<VkBufferCopy> regions;
        //#define mockCopy
        #if defined mockCopy
        #else
        auto _mapped = static_cast<uint8_t*>(h->mapData());
        #endif

        uint32_t _srcOffset = h->offset();
        std::sort(m_pushDirty.begin(), m_pushDirty.end());
        auto _end = std::unique(m_pushDirty.begin(), m_pushDirty.end());

        chunk_by(m_pushDirty.begin(), _end, [](auto &a, auto &b)
        {
            return !(a+1 == b);
        },
        [&](auto &&i, auto &&j)
        {
            auto objectsToCopy = static_cast<size_t>(std::distance(i,j));

            #if defined mockCopy
            #else
            std::memcpy( _mapped, &m_hostData[*i], sizeof(value_type)*objectsToCopy);
            #endif

            auto & r = regions.emplace_back();

            r.srcOffset = _srcOffset;
            r.dstOffset = *i * sizeof(value_type);
            r.size      = sizeof(value_type) * objectsToCopy;

            #if defined mockCopy
                std::cout << "srcByte: " << _srcOffset << "   dstByte: " << r.dstOffset << "   byteCount: " << r.size << std::endl;
            #endif
            _mapped    += r.size;
            _srcOffset += r.size;
        });

        #if defined mockCopy
        #else
        if(regions.size())
        {
            vkCmdCopyBuffer(cmd, h->getBuffer(), m_buffer->getBuffer(), static_cast<uint32_t>(regions.size()), regions.data());
        }
        #endif
        m_pushDirty.clear();
    }


    /**
     * @brief dirtyCount
     * @return
     *
     * Returns the number of elements which are flagged as dirty.
     * This value may be larger than the actual number as the same
     * index can be flagged multiple times. Calling updateDirty()
     * will only update each index once though.
     */
    size_t dirtyCount() const
    {
        return m_pushDirty.size();
    }
    void printDirty()
    {
        for(auto & i : m_pushDirty)
        {
            std::cout << i << ", ";
        }
        std::cout << std::endl;
    }

    auto getHandle()
    {
        return m_buffer;
    }
protected:
    _bufferHandleType m_buffer;
    std::vector<value_type> m_hostData;
    std::vector<size_t> m_pushDirty;

    template<typename it, typename _comp, typename _eva>
    void chunk_by(it _begin, it _end, _comp && cmp, _eva && eva)
    {
        auto i = _begin;
        while( i != _end )
        {
            auto j = std::adjacent_find(i, _end, cmp);
            if(j != _end)
            {
                ++j;
                eva(i,j);
                i = j;
            }
            else
            {
                eva(i,_end);
                break;
            }

        }
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
struct BufferMap : protected BufferVector<_Value, BufferHandle>
{
    using key_type = _Key;
    using value_type = _Value;
    using buffer_vector_type = BufferVector<_Value, BufferHandle>;

    BufferMap()
    {
    }

    BufferMap(BufferHandle h)
    {
        buffer_vector_type::setBuffer(h);
    }

    using buffer_vector_type::setBuffer;
    using buffer_vector_type::getHandle;
    using buffer_vector_type::requiredStagingBufferSize;
    using buffer_vector_type::setDirty;

    template<typename _stagingBufferHandle>
    void pushDirty(VkCommandBuffer cmd, _stagingBufferHandle h)
    {
        buffer_vector_type::pushDirty(cmd, h);
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
                buffer_vector_type::resize( buffer_vector_type::size() + 1);
                s = buffer_vector_type::size()-1;
            }

            m_keyToIndex.insert( {k, s});
        }
        else //
        {
            s = it->second;
        }
        buffer_vector_type::setValue(s, v);
        return { uint32_t(s)};
    }

    /**
     * @brief find
     * @param k
     * @return
     *
     * Find the index where the value to the key is stored.
     *  YOu can then use this index using the atIndex( )
     *  function
     */
    storage_index<value_type> find(key_type const & k) const
    {
        auto it = m_keyToIndex.find(k);
        if(it == m_keyToIndex.end())
            return {};
        return { uint32_t(it->second) };
    }

    /**
     * @brief at
     * @param k
     * @return
     *
     * Returns a reference to the value associated with
     * the specific key
     */
    value_type& atKey(key_type const & k)
    {
        auto id = buffer_vector_type::find(k);
        buffer_vector_type::at(id.index);
    }
    value_type const& atKey(key_type const & k) const
    {
        auto id = buffer_vector_type::find(k);
        buffer_vector_type::at(id.index);
    }

    value_type& atIndex(storage_index<value_type> index)
    {
        return buffer_vector_type::at(index.index);
    }
    value_type const & atIndex(storage_index<value_type> index) const
    {
        return buffer_vector_type::at(index.index);
    }
    value_type& atIndex(size_t index)
    {
        return buffer_vector_type::at(index);
    }
    value_type const & atIndex(size_t index) const
    {
        return buffer_vector_type::at(index);
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

    /**
     * @brief capacity
     * @return
     *
     * Returns the maximum number of items this buffermap can hold
     */
    size_t capacity() const
    {
        return maxSize();
    }

    auto begin()
    {
        return m_keyToIndex.begin();
    }
    auto begin() const
    {
        return m_keyToIndex.begin();
    }
    auto end()
    {
        return m_keyToIndex.end();
    }
    auto end() const
    {
        return m_keyToIndex.end();
    }
protected:
    size_t maxSize() const
    {
        return this->m_buffer->getBufferSize()  / sizeof(value_type);
    }
    std::unordered_map<_Key, size_t > m_keyToIndex;
    std::vector<size_t> m_availableIndex;
    size_t _count=0;
};


}

#endif
