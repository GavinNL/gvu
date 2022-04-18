#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Cache/PipelineLayoutCache.h>

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


    using CacheType = gvu::PipelineLayoutCache;
    CacheType cache;
    cache.init(window->getDevice());



    // Test creating the same object twice
    //--------------------------------------------------------------
    CacheType::createInfo_type ci = {};
    ci.setLayouts.push_back(dLayout1);
    ci.pushConstantRanges.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 128});
    //--------------------------------------------------------------

    auto obj1 = cache.create(ci);
    auto obj2 = cache.create(ci);

    REQUIRE(obj1 == obj2);
    //--------------------------------------------------------------


    // Test creating a different object
    //--------------------------------------------------------------
    CacheType::createInfo_type ci2 = ci;
    ci2.setLayouts.push_back(dLayout1);

    auto obj3 = cache.create(ci2);
    REQUIRE(obj1 != obj3);
    //--------------------------------------------------------------


    // destroy the pools
    cache.destroy();
    dlayoutCache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

