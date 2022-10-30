#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"
#include <gvu/Core/Cache/TextureCache.h>

#include <gvu/Containers/BufferMap.h>

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

    {
        auto B1 = memoryCache.allocateStorageBuffer(1024, false, true);
        auto S1 = memoryCache.allocateStagingBuffer(2048);

        struct Data_t
        {
            uint32_t x;
            uint32_t y;
            uint32_t z;
            uint32_t w;
        };

        gvu::BufferVector<Data_t> hostVisVec(S1);
        gvu::BufferVector<Data_t> gpuVec(B1);


        REQUIRE(hostVisVec.size() == 0);
        REQUIRE(hostVisVec.capacity() == 2048 / sizeof(Data_t));

        hostVisVec.resize(15);
        hostVisVec.setValue(0 , {});
        hostVisVec.setValue(7 , {});
        hostVisVec.setValue(8 , {});
        hostVisVec.setValue(10, {});
        hostVisVec.setValue(11, {});
        hostVisVec.setValue(12, {});

        // host visible values are updated immediately
        REQUIRE(hostVisVec.dirtyCount() == 0);

        REQUIRE(gpuVec.size() == 0);
        REQUIRE(gpuVec.capacity() == 1024 / sizeof(Data_t));


        gpuVec.resize(15);

        gpuVec.setValue(0 , {});
        gpuVec.setValue(7 , {});
        gpuVec.setValue(8 , {});
        gpuVec.setValue(10, {});
        gpuVec.setValue(11, {});
        gpuVec.setValue(12, {});

        // update 12 again
        gpuVec.setValue(12, {});

        // gpu only vectors are cached and need
        //to be updated manually
        // dirty count will be 1 larger because
        // we updated 12 twice.
        REQUIRE(gpuVec.dirtyCount() == 7);

        gpuVec.printDirty();


        {
            memoryCache.getCommandPool().beginRecording([&](auto cmd)
            {
                gpuVec.pushDirty(cmd, S1);
            }, true);
        }

        REQUIRE( gpuVec.capacity() == 1024/sizeof(Data_t));



    }


    memoryCache.destroy();

    vmaDestroyAllocator(allocator);

    window->destroy();
    window.reset();

    SDL_Quit();
}
#define VMA_IMPLEMENTATION
#include<vk_mem_alloc.h>
