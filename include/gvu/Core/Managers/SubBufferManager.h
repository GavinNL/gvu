#ifndef GVU_SUBBUFFER_MANAGER_H
#define GVU_SUBBUFFER_MANAGER_H

#include "../Cache/Objects.h"
#include <set>
#include <list>
namespace gvu
{

/**
 * @brief The SubBuffer struct
 *
 * A subbuffer is a section within a larger buffer that has been allocated
 * with a specific offset so that it can hold array data. For example:
 *
 * For example, given a parent buffer B and we want to allocate 2
 * sub buffers to hold two different types, T1 and T2
 *
 * If B only stored T1 or T2, this is how the array would look
 * in the buffer
 *   B: | T1 | T1 | T1 | T1 | T1 | T1 | T1 | T1 |
 *   B: |    T2   |    T2   |    T2   |    T2   |
 *
 *
 * now if we want to allocate sub buffer A to hold 3x T1 and
 * sub buffer B to hold 2x T2. To properly allocate them
 * so they can be looked up correctly, buffer B would need to
 * look like this:
 *
 *       <---a size---> <-----b.allocatedSize()>
 *       <---a size--->      <--b.size()------->
 *   B: | T1 | T1 | T1 |    |    T2   |    T2   |
 *       ^ a.offset()   ^    ^ b.offset()
 *       |              |
 *       |              b.allocatedOffset()
 *       a.allocatedOffset()
 *
 * If B was a shader storage buffer, you can gain access
 * to both a and b using a single binding
 *
 *
 *     layout(std430, binding = 3) buffer  T1_name
 *     {
 *         T1 data[];
 *     }  T1_data;
 *     layout(std430, binding = 3) buffer  T2_name
 *     {
 *         T1 data[];
 *     }  T1_data;
 *
 *
 *     // these two values have to be passed into the shader
 *     T1_startIndex = a.offset() / a.alignment();
 *     T2_startIndex = b.offset() / b.alignment();
 *
 *     // and values in a and b can be accessed like so
 *     T1_data.data[ T1_startIndex + i]
 *     T2_data.data[ T2_startIndex + i]
 *
 */
struct SubBuffer : public BufferBase
{
    /**
     * @brief offset
     * @return
     *
     * The aligned offset of the buffer. this should be used for binding
     */
    VkDeviceSize offset() const
    {
        return m_offset;
    }

    /**
     * @brief size
     * @return
     *
     * The size of the usable buffer requested by the
     * user
     */
    VkDeviceSize size() const
    {
        return m_size;
    }

    /**
     * @brief allocationSize
     * @return
     *
     * The actual size of the subbuffer that was allocated
     */
    VkDeviceSize allocationSize() const
    {
        return m_allocationSize;
    }
    /**
     * @brief allocationOffset
     * @return
     *
     * The actual offset of the subbuffer
     */
    VkDeviceSize allocationOffset() const
    {
        return m_allocationOffset;
    }

    /**
     * @brief alignment
     * @return
     *
     * The alignment of the buffer.
     *
     * offset() % alignment() == 0
     */
    VkDeviceSize alignment() const
    {
        return m_alignment;
    }

    /**
     * @brief shaderStorageArrayStartIndex
     * @return
     *
     */
    uint32_t shaderStorageArrayStartIndex() const
    {
        return offset() / alignment();
    }

protected:
    // the actual buffer size and the offset
    // from the host buffer
    VkDeviceSize m_allocationOffset = 0;
    VkDeviceSize m_allocationSize = 0;

    // the usable location
    VkDeviceSize m_offset    = 0;
    VkDeviceSize m_size      = 0;
    VkDeviceSize m_alignment = 1;
    friend class SubBufferManager;
};
using SubBufferHandle   = std::shared_ptr<SubBuffer>;

/**
 * @brief The SubBufferManager class
 *
 * The SubBufferManager is essentially a memory pool for buffers
 * that exist within another buffer. This works by allocating
 * a single buffer (ie: 100M) and then allocating sections
 * of that to use for various purposes.
 */
class SubBufferManager : public BufferBase
{
public:
    ~SubBufferManager()
    {
        for(auto & m : m_allocations)
        {
            m = {};
        }
        m_allocations.clear();
    }
    /**
     * @brief setBuffer
     * @param h
     * @param allocationChunkSize
     *
     * Sets the buffer for this manager. This will essentially reset
     * all allocations.
     *
     * The allocationChunkSize will ensure that all allocations are
     * multiples of that value
     */
    void setBuffer(BufferHandle h, VkDeviceSize allocationChunkSize=256)
    {
        m_handle = h;
        m_allocationChunkSize = allocationChunkSize;
        auto m_emptyMemory = std::make_shared<SubBuffer>();
        m_emptyMemory->m_allocationOffset = 0;
        m_emptyMemory->m_allocationSize   = h->getBufferSize();

        m_emptyMemory->m_offset    = 0;
        m_emptyMemory->m_size      = m_emptyMemory->m_allocationSize;
        m_emptyMemory->m_alignment = 1;

        m_allocations.push_back(m_emptyMemory);
    }

    void bindIndex(VkCommandBuffer cmd, VkIndexType _indexType)
    {
        vkCmdBindIndexBuffer(cmd, m_handle->getBuffer(), 0, _indexType);
    }

    /**
     * @brief allocate
     * @param s
     * @param alignment
     * @return
     *
     * Allocate a subbuffer from this buffer manager
     * and return the SubBuffer handle.
     *
     * The returned SubBuffer size will always be
     * a multiple of the alignment you chose.
     *
     * alignment ensures the the offset of the subbuffer you
     * get will be aligned to this value
     *
     * auto b = M.allocate(1545, 1024);
     * assert(b->size() >= 1545)
     * assert(b->offset() % 1024 == 0)
     */
    SubBufferHandle allocate(VkDeviceSize s, VkDeviceSize alignment )
    {
        // bump the size requirements to be multiples of the alignment
        //s = BufferInfo::_roundUp(s, alignment);
        //s = s % alignment == 0 ? s : (s/alignment+1)*alignment;

        assert(m_allocations.size() > 0);
        for(auto it=std::prev(m_allocations.end()); ;)
        {
            auto & E = *it;
            // use_count() == 1 means this chunk was allocate
            // but no longer used
            if(E.use_count() == 1)
            {
                // check if any of the next allocations are
                // able to be merged into this empty allocation
                _mergeForward(it);

                auto firstByte    = E->allocationOffset();
                auto alignedBegin = BufferInfo::_roundUp(firstByte, alignment);
                auto alignedEnd   = BufferInfo::_roundUp(alignedBegin + s, alignment);
                auto lastByte     = BufferInfo::_roundUp(alignedEnd,m_allocationChunkSize);

                auto allocationSize = lastByte-firstByte;
                s = alignedEnd - alignedBegin;

                if(E->allocationSize() >= allocationSize)
                {
                    auto B = std::make_shared<SubBuffer>(*E);
                    B->m_handle           = m_handle;
                    B->m_allocationSize   = allocationSize;
                    B->m_allocationOffset = E->allocationOffset();
                    B->m_size             = s;
                    B->m_offset           = alignedBegin;
                    B->m_alignment        = alignment;

                    E->m_allocationOffset = B->m_allocationOffset + B->m_allocationSize;
                    E->m_allocationSize   -= allocationSize;
                    E->m_offset           = E->m_allocationOffset;
                    E->m_size             = E->m_allocationSize;
                    E->m_alignment        = 1;

                    // insert the newly allocated buffer
                    // into the allocation list right before E
                    m_allocations.insert(it, B);

                    // no need to keep E around if the allocation size is zero
                    if(E->allocationSize() == 0)
                    {
                        m_allocations.erase(it);
                    }
                    return B;
                }
            }
            if( it == m_allocations.begin())
                break;
            --it;
        }


        return {};
    }


    /**
     * @brief mergeFreeAllocations
     *
     * If there are any consequtive allocations which are not
     * being used, combine them into a single allocation
     */
    void mergeFreeAllocations()
    {
        // bump the size requirements to be multiples of the alignment
        //s = BufferInfo::_roundUp(s, alignment);
        //s = s % alignment == 0 ? s : (s/alignment+1)*alignment;

        assert(m_allocations.size() > 0);
        for(auto it=std::prev(m_allocations.end()); ;)
        {
            auto & E = *it;
            // use_count() == 1 means this chunk was allocate
            // but no longer used
            if(E.use_count() == 1)
            {
                // check if any of the next allocations are
                // able to be merged into this empty allocation
                _mergeForward(it);

            }
            if( it == m_allocations.begin())
                break;
            --it;
        }
    }
    /**
     *
     * Allocate a buffer using a specifc type. This is
     * mostly just a helper function to use if you know you
     * are going to use the buffer to only hold a certain types (eg: vec3 for positions)
     *
     */
    template<typename T>
    SubBufferHandle allocateTyped(uint32_t count)
    {
        auto d = allocate( sizeof(T) * count, sizeof(T));
        assert(d->offset() % sizeof(T) == 0);
        return d;
    }

    std::list<SubBufferHandle> const & allocations() const
    {
        return m_allocations;
    }

    void print(VkDeviceSize chunkSize = 256)
    {
        assert( std::is_sorted(m_allocations.begin(), m_allocations.end(),[](auto & a, auto & b)
        {
            return a->allocationOffset() < b->allocationOffset();
        }));
        std::string _sym = "#X@";
        std::string _emp = "_.";
        std::string out;
        size_t i=0;

        for(auto & a : m_allocations)
        {
            auto s = a->allocationSize() / chunkSize;
            out += std::string(s, a.use_count() == 1 ? _emp[i%_emp.size()] : _sym[i%_sym.size()] );
            ++i;
        }
        std::cout << out << std::endl;
    }

    static SubBufferManager createTestCase(VkDeviceSize size, VkDeviceSize offset,
                                    VkDeviceSize allocationSize, VkDeviceSize allocationOffset,
                                    VkDeviceSize alignment)
    {
        auto a = std::make_shared<gvu::SubBuffer>();

        a->m_size = size;
        a->m_offset = offset;
        a->m_allocationSize = allocationSize;
        a->m_allocationOffset = allocationOffset;
        a->m_alignment = alignment;

        SubBufferManager M;
        M.m_allocations.push_back(a);
        return M;
    }
protected:
    // given an iterator to a subbuffer handle
    // attempt to merge with next allocation if
    // the next handle is empty
    bool _mergeForward(std::list<SubBufferHandle>::iterator a)
    {
        if( a == m_allocations.end())
            return false;
        auto b = std::next(a);
        if( b == m_allocations.end())
            return false;
        auto & A = *a;
        auto & B = *b;

        // if both of the allocations are empty
        // then we can merge them
        if(B.use_count() == 1)
        {
            assert(A->allocationOffset() + A->allocationSize() == B->allocationOffset() );

            if(A->allocationOffset() + A->allocationSize() == B->allocationOffset() )
            {
                A->m_allocationSize += B->allocationSize();
                A->m_size            = A->m_allocationSize;

                m_allocations.erase(b);
                return _mergeForward(a);
            }
        }
        return false;
    }

    std::list<SubBufferHandle> m_allocations;
    VkDeviceSize m_allocationChunkSize = 256;
};



}

#endif

