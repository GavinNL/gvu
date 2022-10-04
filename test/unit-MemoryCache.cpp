#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/RenderPassCache.h>
#include <gvu/Core/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Core/Cache/PipelineLayoutCache.h>
#include <gvu/Core/GraphicsPipelineCreateInfo.h>
#include <gvu/Core/Cache/TextureCache.h>

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

    VmaAllocatorCreateInfo ci;
    ci.flags = {};
    ci.device = window->getDevice();
    ci.physicalDevice = window->getPhysicalDevice();
    ci.instance = window->getInstance();
    ci.vulkanApiVersion = VK_API_VERSION_1_2;

    VmaAllocator allocator;
    auto res = vmaCreateAllocator(&ci, &allocator);
    REQUIRE(res == VK_SUCCESS);


    gvu::MemoryCache M;
    M.init(window->getPhysicalDevice(), window->getDevice(), window->getGraphicsQueue(), allocator,1024*1024*4);



    size_t siz = 1024*1024*100;
    auto buffer = M.allocateBuffer(siz,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    // Test creating a different object
    //--------------------------------------------------------------


    std::vector<uint8_t> raw(siz);

    auto t0 = std::chrono::system_clock::now();
    buffer->beginUpdate(raw.data(), raw.size(), 0);
    auto t1 = std::chrono::system_clock::now();

    std::cout << "Time to update: " << std::chrono::duration<double>(t1-t0).count() << std::endl;
    //--------------------------------------------------------------
    buffer->destroy();

    M.destroy();

    vmaDestroyAllocator(allocator);

    window->destroy();
    window.reset();

    SDL_Quit();

}

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
