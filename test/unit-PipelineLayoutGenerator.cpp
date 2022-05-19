#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Cache/RenderPassCache.h>
#include <gvu/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Cache/PipelineLayoutCache.h>
#include <gvu/GraphicsPipelineCreateInfo.h>
#include <gvu/spirvPipelineReflector.h>


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

    gvu::RenderPassCache          rpCache;
    gvu::DescriptorSetLayoutCache dlCache;
    gvu::PipelineLayoutCache      plCache;

    rpCache.init(window->getDevice());
    dlCache.init(window->getDevice());
    plCache.init(window->getDevice());

    // Test creating the same object twice


    gvu::ShaderModuleCreateInfo s_ci1(CMAKE_SOURCE_DIR "/share/shaders/pbr.vert.spv");
    gvu::ShaderModuleCreateInfo s_ci2(CMAKE_SOURCE_DIR "/share/shaders/pbr.frag.spv");


    gvu::spirvPipelineReflector generator;
    generator.addSPIRVCode(s_ci1.code, VK_SHADER_STAGE_VERTEX_BIT);
    generator.addSPIRVCode(s_ci2.code, VK_SHADER_STAGE_FRAGMENT_BIT);


    REQUIRE(generator.vertex.inputAttributes.size() == 8);


    // generate a combinedPipelineLayoutCreateInfo
    auto C = generator.generateCombinedPipelineLayoutCreateInfo();

    REQUIRE(C.pushConstantRanges.size() != 0);

    // three discriptor sets
    REQUIRE(C.setLayoutInfos.size() == 3);

    // create the layout using pipelineLayout and descriptorset layout cachje
    auto layout = C.create(plCache,dlCache);

    REQUIRE(dlCache.cacheSize() == 3);
    REQUIRE(plCache.cacheSize() == 1);

    REQUIRE(layout != VK_NULL_HANDLE);
    // Test creating a different object
    //--------------------------------------------------------------


    //--------------------------------------------------------------

    // Destroy the cache
    plCache.destroy();
    dlCache.destroy();
    rpCache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}



SCENARIO( "Compute Shader" )
{
    // create a default window and initialize all vulkan
    // objects.
    auto window = createWindow(1024,768);

    // resize the framegraph to the size of the
    // swapchain. This will allocate any internal
    // images which depend on the size of the swapchain (eg: gBuffers)
    auto e = window->getSwapchainExtent();
    (void)e;

    gvu::RenderPassCache          rpCache;
    gvu::DescriptorSetLayoutCache dlCache;
    gvu::PipelineLayoutCache      plCache;

    rpCache.init(window->getDevice());
    dlCache.init(window->getDevice());
    plCache.init(window->getDevice());

    // Test creating the same object twice


    gvu::ShaderModuleCreateInfo s_ci1(CMAKE_SOURCE_DIR "/share/shaders/comp.spv");


    gvu::spirvPipelineReflector generator;
    generator.addSPIRVCode(s_ci1.code, VK_SHADER_STAGE_COMPUTE_BIT);



    auto C = generator.generateCombinedPipelineLayoutCreateInfo();

    // create the layout using pipelineLayout and descriptorset layout cachje
    auto layout = C.create(plCache,dlCache);

    REQUIRE(layout != VK_NULL_HANDLE);

    // Destroy the cache
    plCache.destroy();
    dlCache.destroy();
    rpCache.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

