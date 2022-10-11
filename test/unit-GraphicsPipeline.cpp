#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/RenderPassCache.h>
#include <gvu/Core/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Core/Cache/PipelineLayoutCache.h>
#include <gvu/Core/GraphicsPipelineCreateInfo.h>
#include <gvu/Extension/spirvPipelineReflector.h>
#include <gvu/Advanced/Pipeline.h>
#include <gvu/Advanced/VulkanApplicationContext.h>

SCENARIO( " Scenario 1: Create a Sampler" )
{
    glslang::InitializeProcess();

    // create a default window and initialize all vulkan
    // objects.
    auto window = createWindow(1024,768);

    // resize the framegraph to the size of the
    // swapchain. This will allocate any internal
    // images which depend on the size of the swapchain (eg: gBuffers)
    auto e = window->getSwapchainExtent();
    (void)e;

    auto context = std::make_shared<gvu::VulkanApplicationContext>();

    context->init(
                window->getInstance(),
                window->getPhysicalDevice(),
                window->getDevice(),
                window->getGraphicsQueue()
                );


   // THEN("We can create pipelines")
    {
        auto p = context->makeGraphicsPipeline();

        p->getVertexStage()
                .loadGLSL(CMAKE_SOURCE_DIR "/share/shaders/model_attributes_MVP.vert")
                .addCompileTimeDefinition("VERTEX_SHADER", "")
                .compile();
        p->getFragmentStage()
                .loadGLSL(CMAKE_SOURCE_DIR "/share/shaders/model_attributes_MVP.frag")
                .addCompileTimeDefinition("FRAGMENT_SHADER", "")
                .compile();

        p->setOutputFormat(0, VK_FORMAT_R8G8B8A8_UNORM);
        p->setDepthFormat(VK_FORMAT_D32_SFLOAT);
        p->getCreateInfo().setVertexInputs({ VK_FORMAT_R32G32B32_SFLOAT, // position
                                             VK_FORMAT_R32G32B32_SFLOAT, // normal
                                             VK_FORMAT_R32G32B32A32_SFLOAT, // tangent

                                             VK_FORMAT_R32G32_SFLOAT, // tex0
                                             VK_FORMAT_R32G32_SFLOAT, // tex1

                                             VK_FORMAT_R8G8B8A8_UNORM, // color 0
                                             VK_FORMAT_R16G16B16A16_UINT,
                                             VK_FORMAT_R32G32B32A32_SFLOAT,
                                           });
        p->getCreateInfo().cullMode = VK_CULL_MODE_BACK_BIT;
        p->build();


        p->destroy();
    }

    std::cout << "Destroying context" << std::endl;
    context->destroy();

    window->destroy();
    window.reset();

    SDL_Quit();
    glslang::FinalizeProcess();
}
#define VMA_IMPLEMENTATION
#include<vk_mem_alloc.h>
