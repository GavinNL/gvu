#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/SamplerCache.h>

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

    using CacheType = gvu::SamplerCache;

    CacheType cache;
    cache.init(window->getDevice());

    // Test creating the same object twice
    //--------------------------------------------------------------
    CacheType::createInfo_type ci = {};
    //--------------------------------------------------------------

    auto obj1 = cache.create(ci);
    auto obj2 = cache.create(ci);

    REQUIRE(obj1 == obj2);
    //--------------------------------------------------------------


    // Test creating a different object
    //--------------------------------------------------------------
    CacheType::createInfo_type ci2;
    ci2.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    auto obj3 = cache.create(ci2);
    REQUIRE(obj1 != obj3);
    //--------------------------------------------------------------

    // Destroy the cache
    cache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

