#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/TextureCache.h>


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

    VmaAllocator allocator = nullptr;
    {
        VmaAllocatorCreateInfo _aci = {};
        _aci.flags = {};
        _aci.device = window->getDevice();
        _aci.physicalDevice = window->getPhysicalDevice();
        _aci.instance = window->getInstance();
        _aci.vulkanApiVersion = VK_API_VERSION_1_3;
        vmaCreateAllocator(&_aci, &allocator);
    }


    gvu::MemoryCache memoryCache;
    memoryCache.init(window->getPhysicalDevice(), window->getDevice(), window->getGraphicsQueue(), allocator);



    auto B1 = memoryCache.allocateBuffer(513, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    REQUIRE(B1->getBufferSize() == 3*256);

    auto raw = B1.get();

    // reset the shared pointer to reduce it's usecount
    // its now considered unused
    B1.reset();

    // allocate another buffer with a similar size and usage
    auto B2 = memoryCache.allocateBuffer(599, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // will return the old one
    REQUIRE(B2.get() == raw);

    memoryCache.destroy();

    vmaDestroyAllocator(allocator);

    window->destroy();
    window.reset();

    SDL_Quit();
}
#define VMA_IMPLEMENTATION
#include<vk_mem_alloc.h>
