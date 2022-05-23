#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/RenderPassCache.h>

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
    cache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

