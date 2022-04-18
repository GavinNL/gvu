#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Cache/PipelineLayoutCache.h>
#include <gvu/Managers/DescriptorPoolManager.h>

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

    gvu::DescriptorSetLayoutCache dlayoutCache;
    dlayoutCache.init(window->getDevice());

    gvu::DescriptorSetLayoutCreateInfo dci;
    dci.bindings.emplace_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
    dci.bindings.emplace_back(VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
    auto dLayout1 = dlayoutCache.create(dci);

    gvu::DescriptorPoolManager poolManager;

    poolManager.init(window->getDevice(), &dlayoutCache, dLayout1, 3);

    auto s1 = poolManager.allocateDescriptorSet();
    auto s2 = poolManager.allocateDescriptorSet();
    auto s3 = poolManager.allocateDescriptorSet();

    REQUIRE( poolManager.allocatedSetsCount(poolManager.getPool(s1)) == 3);

    REQUIRE( poolManager.getPool(s1) == poolManager.getPool(s2));
    REQUIRE( poolManager.getPool(s1) == poolManager.getPool(s3));

    // allocate a 4th set, this will generate a new pool
    auto s4 = poolManager.allocateDescriptorSet();
    (void)s4;
    REQUIRE( poolManager.allocatedPoolCount() == 2);

    // release allthe initial sets back to the pools
    poolManager.releaseToPool(s1);
    poolManager.releaseToPool(s2);
    poolManager.releaseToPool(s3);

    // resets all the pools
    poolManager.resetAllAvailablePools();

    // destroy the pools
    poolManager.destroy();
    dlayoutCache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

