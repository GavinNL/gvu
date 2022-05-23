#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/DescriptorSetLayoutCache.h>

SCENARIO( " Scenario 1: Create a DescriptorSetLayout" )
{
    // create a default window and initialize all vulkan
    // objects.
    auto window = createWindow(1024,768);

    // resize the framegraph to the size of the
    // swapchain. This will allocate any internal
    // images which depend on the size of the swapchain (eg: gBuffers)
    auto e = window->getSwapchainExtent();
    (void)e;

    using CacheType = gvu::DescriptorSetLayoutCache;

    CacheType  cache;
    cache.init(window->getDevice());

    CacheType::createInfo_type ci;
    ci.bindings.emplace_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});


    auto layout = cache.create(ci);
    auto layout2 = cache.create(ci);

    REQUIRE(layout == layout2);


    auto ci2 = ci;
    ci2.bindings.emplace_back(VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
    auto layout3 = cache.create(ci2);

    REQUIRE(layout != layout3);

    // destroy the pools
    cache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

