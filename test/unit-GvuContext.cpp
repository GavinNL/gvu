#include<catch2/catch.hpp>
#include <fstream>

#include "unit_helpers.h"

#include <gvu/Core/Managers/SubBufferManager.h>

#include <gvu/Core/Cache/TextureCache.h>
#include <gvu/Core/Cache/DescriptorSetLayoutCache.h>
#include <gvu/Core/Cache/SamplerCache.h>
#include <gvu/Core/Cache/RenderPassCache.h>
#include <gvu/Core/Cache/PipelineLayoutCache.h>
#include <gvu/Core/Managers/CommandPoolManager.h>
#include <gvu/Core/Managers/DescriptorPoolManager.h>

/**
 * @brief The GVU_CONTEXT struct
 *
 * The GVU Context is a single class which
 * contains all the caches and managers in the
 * gvu library so that it can be easily initializd
 * and destroyed.
 */
struct GVU_CONTEXT
{
    gvu::DescriptorSetLayoutCache descriptorSetLayoutCache;
    gvu::SamplerCache             samplerCache;
    gvu::RenderPassCache          renderpassCache;
    gvu::PipelineLayoutCache      pipelineLayoutCache;
    gvu::CommandPoolManager2      commandPoolManager;
    gvu::MemoryCache              mem;

    void init(VkInstance i, VkPhysicalDevice pd, VkDevice d, VkQueue gq, VmaAllocator a)
    {
        (void)i;

        if(!a)
        {
            VmaAllocatorCreateInfo _aci = {};
            _aci.flags = {};
            _aci.device = d;
            _aci.physicalDevice = pd;
            _aci.instance = i;
            _aci.vulkanApiVersion = VK_API_VERSION_1_2;
            vmaCreateAllocator(&_aci, &a);
            _selfManagedAllocator = true;
        }

        gvu::MemoryCache::MemoryCacheCreateInfo ci;
        ci.physicalDevice = pd;
        ci.device = d;
        ci.graphicsQueue = gq;
        ci.allocator = a;
        ci.defaultStagingBufferSize = (1024*1024*4)*4;

        pipelineLayoutCache.init(d);
        renderpassCache.init(d);
        samplerCache.init(d);
        descriptorSetLayoutCache.init(d);

        commandPoolManager.init(d,pd, gq);

        mem.init(ci);
    }

    void destroy()
    {
        auto a = mem.getAllocator();
        mem.destroy();
        commandPoolManager.destroy();
        descriptorSetLayoutCache.destroy();
        samplerCache.destroy();
        renderpassCache.destroy();
        pipelineLayoutCache.destroy();

        if(_selfManagedAllocator == true)
        {
            vmaDestroyAllocator(a);
        }
        *this = {};
    }
protected:
    bool _selfManagedAllocator = false;
};

#if 1
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

    GVU_CONTEXT C;
    C.init(window->getInstance(), window->getPhysicalDevice(), window->getDevice(), window->getGraphicsQueue(),nullptr);

    C.destroy();

    window->destroy();
    window.reset();

    SDL_Quit();

}

#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
