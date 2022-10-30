#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/TextureCache.h>
#include <gvu/Containers/TextureMap.h>
#include <gvu/Advanced/VulkanApplicationContext.h>
#include <gvu/Advanced/ImageArrayManager2.h>

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

    auto context = std::make_shared<gvu::VulkanApplicationContext>();

    context->init(
                window->getInstance(),
                window->getPhysicalDevice(),
                window->getDevice(),
                window->getGraphicsQueue()
                );


    auto nullTexture = context->memoryCache.allocateTexture2D(8,8, VK_FORMAT_R8G8B8A8_UNORM,1);
    auto nullCube = context->memoryCache.allocateTextureCube(8,VK_FORMAT_R8G8B8A8_UNORM,1);


    gvu::TextureMap T;
    T.init(1024, nullTexture);


    REQUIRE( T.dirtyCount() == 1024);
    REQUIRE( T.getIndex(nullTexture) == 0);

    {
        gvu::DescriptorSetLayoutCreateInfo ci = {};
        auto & tab = ci.bindings.emplace_back();
        tab.descriptorCount = T.arraySize();
        tab.binding  = 0;
        tab.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        tab.pImmutableSamplers = nullptr;
        tab.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        auto layout = context->descriptorSetLayoutCache.create(ci);

        auto set = context->allocateDescriptorSet(layout);

        T.update(set, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    }


    T.destroy();

    {
        gvu::TextureArrayManager2 G;
        G.init(context, 4,
               1024, nullTexture,
               64, nullCube,
               VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;
        G.nextSet();
        std::cout << G.updateDirty() << std::endl;

        G.destroy();
    }

    context->destroy();
    window->destroy();
    window.reset();

    SDL_Quit();
}
#define VMA_IMPLEMENTATION
#include<vk_mem_alloc.h>
