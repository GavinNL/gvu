#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/RenderPassCache.h>
#include <gvu/Core/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Core/Cache/PipelineLayoutCache.h>
#include <gvu/Core/GraphicsPipelineCreateInfo.h>

SCENARIO( " Create a graphics pipeline using a renderpass" )
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

    gvu::RenderPassCache          rpCache;
    gvu::DescriptorSetLayoutCache dlCache;
    gvu::PipelineLayoutCache      plCache;

    rpCache.init(window->getDevice());
    dlCache.init(window->getDevice());
    plCache.init(window->getDevice());

    // Test creating the same object twice


    gvu::ShaderModuleCreateInfo s_ci1(CMAKE_SOURCE_DIR "/share/shaders/model_attributes_MVP.vert.spv");
    gvu::ShaderModuleCreateInfo s_ci2(CMAKE_SOURCE_DIR "/share/shaders/model_attributes_MVP.frag.spv");

    auto _createSahder = [dev = window->getDevice()](auto & C)
    {
        VkShaderModule mod = VK_NULL_HANDLE;
        auto res = vkCreateShaderModule(dev, &C, nullptr, &mod);
        assert(res == VK_SUCCESS);
        return mod;
    };

    auto vert_s = s_ci1.create(_createSahder);
    auto frag_s = s_ci2.create(_createSahder);


    //


    //------ Create the pipeline layout
    gvu::PipelineLayoutCreateInfo pci;
    pci.pushConstantRanges.push_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT,0,128});
    auto pipelineLayout = plCache.create(pci);

    //----
    gvu::GraphicsPipelineCreateInfo gci;


    gci.setVertexInputs({VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM});

    // this is required to be set
    gci.setOutputFormat(0, VK_FORMAT_R8G8B8A8_UNORM);
    gci.vertexShader = vert_s;
    gci.fragmentShader = frag_s;
    gci.renderPass = VK_NULL_HANDLE;
    gci.pipelineLayout = pipelineLayout;

    gci.dynamicStates = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};


    auto pipeline = gci.create([dev = window->getDevice()](auto & c)
    {
        VkPipeline pl = VK_NULL_HANDLE;
        vkCreateGraphicsPipelines(dev, nullptr, 1, &c, nullptr, &pl);
        return pl;
    });

    REQUIRE(pipeline != VK_NULL_HANDLE);
    //-----
    vkDestroyShaderModule(window->getDevice(), vert_s, nullptr);
    vkDestroyShaderModule(window->getDevice(), frag_s, nullptr);
    vkDestroyPipeline(window->getDevice(), pipeline, nullptr);



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

