#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/RenderPassCache.h>
#include <gvu/Core/Managers/SubBufferManager.h>


SCENARIO("Aligned allocations")
{
    gvu::SubBufferManager M = gvu::SubBufferManager::createTestCase(1024, 0, 1024,0,1);


    WHEN("We allocate 15 bytes with an alignment of 32")
    {
        auto b = M.allocate(15, 32);

        THEN("The size and offset is 0,32")
        {
            REQUIRE( b->offset() == 0);
            REQUIRE( b->size()   == 32);
        }
        THEN("The allocated size is 32")
        {
            REQUIRE( b->allocationSize()   == 256);
            REQUIRE( b->allocationOffset() == 0);
        }

        WHEN("We allocate a buffer with 24 byte alignment")
        {
            auto c = M.allocate(468,24);

            THEN("The offset and size is a multiple of the alignment")
            {
                REQUIRE( c->offset() % 24 == 0);
                REQUIRE( c->size()   % 24 == 0);
                REQUIRE( c->size() >= 468 );
                REQUIRE( c->alignment() == 24);
            }
            THEN("The allocated size is greater than the size")
            {
                REQUIRE( c->allocationSize() > c->size() );
                REQUIRE( c->allocationOffset() == 256);
            }
        }
    }
}

SCENARIO("Multiple allocations")
{
    gvu::SubBufferManager M = gvu::SubBufferManager::createTestCase(1024, 0, 1024,0,1);


    WHEN("We allocate 15 bytes with an alignment of 32")
    {
        auto a = M.allocate(256, 16);
        M.print();
        auto b = M.allocate(256, 16);
        M.print();
        auto c = M.allocate(256, 16);
        M.print();
        auto d = M.allocate(256, 16);


        // |  a  |  b  |  c  |  d  |

        THEN("The size and offset is 0,32")
        {
            REQUIRE( a->offset() == 0);
            REQUIRE( b->offset() == 256);
            REQUIRE( c->offset() == 512);
            REQUIRE( d->offset() == 768);

            M.print();
            WHEN("We try to allocate again we get a nullptr")
            {
                auto e = M.allocate(256,16);
                REQUIRE( e.get() == nullptr);
            }

            WHEN("We release c")
            {
                c.reset();
                M.print();
                THEN("We can allocate the same size again")
                {
                    auto e = M.allocate(256,8);
                    REQUIRE(e->offset() == 512);
                }
            }
        }
    }
}

SCENARIO("Condense allocations")
{
    gvu::SubBufferManager M = gvu::SubBufferManager::createTestCase(1024, 0, 1024,0,1);


    WHEN("Multiple buffers")
    {
        auto a = M.allocate(256, 16);
        auto b = M.allocate(256, 16);
        auto c = M.allocate(256, 16);
        auto d = M.allocate(256, 16);


        // |  a  |  b  |  c  |  d  |

        THEN("We can check the offsets")
        {
            REQUIRE( a->offset() == 0);
            REQUIRE( b->offset() == 256);
            REQUIRE( c->offset() == 512);
            REQUIRE( d->offset() == 768);

            M.print();
            WHEN("We reset all of them and condense them")
            {
                a.reset();
                b.reset();
                c.reset();
                d.reset();


                M.print();
                M.condense();

                THEN("There will only be one allocation")
                {
                    REQUIRE(M.allocations().size() == 1);
                }
                M.print();
            }
        }
    }
}


SCENARIO("Allocting multiple")
{
    // allocate 200Mb of mesh data
    gvu::SubBufferManager M = gvu::SubBufferManager::createTestCase(200*1024*1024, 0,
                                                                    200*1024*1024,0,1);


    std::vector<VkDeviceSize> alignments = {12,16,4,8};
    std::vector<gvu::SubBufferHandle> allocs;
    while(true)
    {
        auto b = M.allocate( (rand()%5+1) * 1024*1024, alignments[rand()%4]);
        allocs.push_back(b);
        if(!b)
            break;
    }

    std::cout << "Total Allocations: " << allocs.size() << std::endl;
}

SCENARIO("Allocting typed buffers")
{
    // allocate 200Mb of mesh data
    gvu::SubBufferManager M = gvu::SubBufferManager::createTestCase(512*1024, 0,
                                                                    512*1024,0,1);


    auto b = M.allocateTyped<float>(300);
    {
        REQUIRE(b->size() >= 300 * sizeof(float));
        REQUIRE(b->offset() % sizeof(float) == 0);
        REQUIRE(b->allocationSize() % 256 == 0);
        REQUIRE(b->allocationOffset() % 256 == 0);
    }

    auto c = M.allocateTyped< std::array<float, 3> >(500);
    {
        REQUIRE(c->size() >= 500 * sizeof(float));
        REQUIRE(c->offset() % sizeof(float) == 0);
        REQUIRE(c->alignment() == sizeof( std::array<float,3>));
        REQUIRE(c->allocationSize() % 256 == 0);
        REQUIRE(c->allocationOffset() % 256 == 0);
    }


    auto d = M.allocateTyped< std::array<float, 3> >(500);
    c.reset();
    {
        REQUIRE(d->size() >= 500 * sizeof(float));
        REQUIRE(d->offset() % sizeof(float) == 0);
        REQUIRE(d->alignment() == sizeof( std::array<float,3>));
        REQUIRE(d->allocationSize() % 256 == 0);
        REQUIRE(d->allocationOffset() % 256 == 0);
    }
    // bind the vertex buffers like so:
    // VkBuffer     buffers[] = { b->buffer(), c->buffer()};
    // VkDeviceSize offsets[] = { b->offset()   , c->offset()};
    // vkCmdBindVertexBuffers({}, 0, 2, buffers, offsets);

    // we want to be able to do something like this:



    M.print();
}


#if 0
SCENARIO( " Scenario 1: Create a Sampler" )
{
    // create a default window and initialize all vulkan
    // objects.
    auto window = createWindow(1024,768);

    // resize the framegraph to the size of the
    // swapchain. This will allocate any internal
    // images which depend on the size of the swapchain (eg: gBuffers)
    auto e = window->getSwapchainExtent();
    (void)e;

    using CacheType = gvu::RenderPassCache;

    CacheType cache;
    cache.init(window->getDevice());

    // Test creating the same object twice
    //--------------------------------------------------------------
    CacheType::createInfo_type ci = CacheType::createInfo_type::createSimpleRenderPass( {{VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}});
    //--------------------------------------------------------------

    auto obj1 = cache.create(ci);
    auto obj2 = cache.create(ci);

    REQUIRE(obj1 == obj2);
    //--------------------------------------------------------------


    // Test creating a different object
    //--------------------------------------------------------------
    CacheType::createInfo_type ci2 = CacheType::createInfo_type::createSimpleRenderPass( {{VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}},
                                                                                        {VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
    auto obj3 = cache.create(ci2);
    REQUIRE(obj1 != obj3);
    //--------------------------------------------------------------

    // Destroy the cache
    cache.destroy(); B->m_alloca

    window->destroy();
    window.reset();

    SDL_Quit();

}

#endif

