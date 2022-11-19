#ifndef GVU_VULKAN_APPLICATION_CONTEXT_H
#define GVU_VULKAN_APPLICATION_CONTEXT_H

#include <iostream>
#include <cassert>
#include "../Core/Managers/CommandPoolManager.h"
#include "../Core/Managers/DescriptorPoolManager.h"
#include "../Core/Cache/TextureCache.h"
#include "../Core/Cache/SamplerCache.h"
#include "../Core/Cache/DescriptorSetLayoutCache.h"
#include "../Core/Cache/RenderPassCache.h"
#include "../Core/Cache/PipelineLayoutCache.h"
#include "../Core/GraphicsPipelineCreateInfo.h"
//#include "../Advanced/GLSLCompiler.h"
//#include "../Extension/spirvPipelineReflector.h"

#include "Pipeline.h"


#if __has_include(<spdlog/spdlog.h>)
#define GVU_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace gvu
{
struct VulkanApplicationContext;


/**
 * @brief The VulkanApplicationContext struct
 *
 * The VulkanApplicationContext is a struct which
 * holds all the managers/caches which can be used
 * to create vulkan objects.
 *
 * It also provides additional functions/objects for
 * Pipelines and ComputePipelines
 *
 */
struct VulkanApplicationContext : std::enable_shared_from_this<VulkanApplicationContext>
{
    VmaAllocator                  allocator = nullptr;
    gvu::MemoryCache              memoryCache;// allocate textures from here
    gvu::RenderPassCache          renderPassCache;
    gvu::SamplerCache             samplerCache;
    gvu::DescriptorSetLayoutCache descriptorSetLayoutCache;
    gvu::PipelineLayoutCache      pipelineLayoutCache;
    gvu::DescriptorSetAllocator   descriptorSetAllocator;
    gvu::CommandPoolManager       commandPool;
    gvu::CommandPoolManager2      commandPoolManager;

    void init(VkInstance instance,
              VkPhysicalDevice physicalDevice,
              VkDevice device,
              VkQueue graphicsQueue)
    {
        VmaAllocatorCreateInfo outInfo = {};
        outInfo.physicalDevice   = physicalDevice;
        outInfo.device           = device;
        outInfo.instance         = instance;
        outInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        VkResult res = vmaCreateAllocator(&outInfo, &allocator);

        memoryCache.init(physicalDevice, device, graphicsQueue, allocator);
        renderPassCache.init(device);
        samplerCache.init(device);
        descriptorSetLayoutCache.init(device);
        pipelineLayoutCache.init(device);
        descriptorSetAllocator.init(&descriptorSetLayoutCache);
        commandPool.init(device,physicalDevice,graphicsQueue);
        commandPoolManager.init(device, physicalDevice, graphicsQueue);

        for(int i=0;i<5;i++)
            commandPoolManager.getCommandPool();
    }

    void destroy()
    {
        pipelineLayoutCache.destroy();
        descriptorSetLayoutCache.destroy();
        samplerCache.destroy();
        renderPassCache.destroy();
        memoryCache.destroy();
        commandPool.destroy();
        commandPoolManager.destroy();
        descriptorSetAllocator.destroy();
        vmaDestroyAllocator(allocator);
    }

    VkDevice getDevice() const
    {
        return memoryCache.getDevice();
    }



    /**
     * @brief createBuffer
     * @param bytes
     * @param usage
     * @param memUsage
     * @return
     *
     * Create a buffer
     */
    std::shared_ptr<gvu::BufferInfo> createBuffer(size_t bytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
    {
        return memoryCache.allocateBuffer(bytes, usage, memUsage, {});
    }


    /**
     * @brief makePipeline
     * @return
     *
     * Returns a shared pipeline object with the appropriate
     * pointers created.
     */
    std::shared_ptr<GraphicsPipeline> makeGraphicsPipeline()
    {
        return std::make_shared<GraphicsPipeline>(shared_from_this());
    }

    /**
     * @brief makeComputePipeline
     * @return
     *
     * Returns a shared pointer to a compute pipeline
     */
    std::shared_ptr<ComputePipeline> makeComputePipeline()
    {
        return std::make_shared<ComputePipeline>(shared_from_this());
    }

    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout)
    {
        return descriptorSetAllocator.allocate(layout);
    }

    void releaseDescriptorSet(VkDescriptorSet set)
    {
        descriptorSetAllocator.releaseToPool(set);
    }
};


inline VkShaderModule ShaderStage::getModule()
{
    if(module == VK_NULL_HANDLE)
    {
        compile();

        VkShaderModuleCreateInfo ci = {};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = static_cast<uint32_t>(spirvCode.size() * 4);
        ci.pCode    = spirvCode.data();

        VkShaderModule sh = {};
        auto result = vkCreateShaderModule(context->getDevice(), &ci, nullptr, &sh);
        if(result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed at compiling shader");
        }
        module = sh;
    }
    return module;
}

inline void GraphicsPipeline::destroy()
{
    if(pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(context->getDevice(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        vertexStage.destroy();
        fragmentStage.destroy();
    }

}

inline void GraphicsPipeline::build()
{
    {

        createInfo.vertexShader = vertexStage.getModule();// _createShader(vertexStage.spirvCode);
        createInfo.fragmentShader = fragmentStage.getModule();//_createShader(fragmentStage.spirvCode);
    }

    reflector.addSPIRVCode(vertexStage.spirvCode, VK_SHADER_STAGE_VERTEX_BIT);
    reflector.addSPIRVCode(fragmentStage.spirvCode, VK_SHADER_STAGE_FRAGMENT_BIT);

    auto pipelineLayoutCreateInfo = reflector.generateCombinedPipelineLayoutCreateInfo();

    size_t s=0;
#if defined GVU_HAS_SPDLOG
    for(auto & S : pipelineLayoutCreateInfo.setLayoutInfos)
    {
        spdlog::debug("Set: {}", s);
        for(auto & b : S.bindings)
        {
            spdlog::debug("Binding: {}  Count: {}  Stage: {:x}  type: {}", b.binding, b.descriptorCount, static_cast<int>(b.stageFlags), static_cast<int>(b.descriptorType));
        }
        s++;
    }
#endif
    createInfo.pipelineLayout = pipelineLayoutCreateInfo.create(context->pipelineLayoutCache, context->descriptorSetLayoutCache);

    uint32_t set=0;
    for(auto & c : pipelineLayoutCreateInfo.setLayoutInfos)
    {
        auto l = context->descriptorSetLayoutCache.create(c);
        setLayouts[set] = l;
        set++;
    }


    //createInfo.enableDepthTest = true;
    //createInfo.enableDepthWrite = true;
    createInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    createInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    //createInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
    //createInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_FRONT_FACE);
    //createInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);

    //createInfo.setVertexInputs({
    //                       VK_FORMAT_R32G32B32_SFLOAT, // position
    //                       VK_FORMAT_R32G32B32_SFLOAT, // normal
    //                       VK_FORMAT_R32G32B32A32_SFLOAT, // tangent
    //
    //                       VK_FORMAT_R32G32_SFLOAT, // tex0
    //                       VK_FORMAT_R32G32_SFLOAT, // tex1
    //
    //                       VK_FORMAT_R8G8B8A8_UNORM, // color 0
    //                       VK_FORMAT_R16G16B16A16_UINT,
    //                       VK_FORMAT_R32G32B32A32_SFLOAT,
    //                   });


    pipeline = createInfo.create([&](auto & C)
    {
        VkPipeline p2 = VK_NULL_HANDLE;
        auto result = vkCreateGraphicsPipelines(context->getDevice(), nullptr, 1, &C, nullptr, &p2);
        if(result != VK_SUCCESS)
        {

            //throw std::runtime_error("Failed at compiling pipeline");
        }
        return p2;
    });
}

inline void ComputePipeline::destroy()
{
    vkDestroyPipeline(context->getDevice(), pipeline, nullptr);
}

inline void ComputePipeline::build()
{

    {
        createInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        createInfo.stage.module = computeStage.getModule();//_createShader(computeStage.spirvCode);
        createInfo.stage.pName  = "main";
        createInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    reflector.addSPIRVCode(computeStage.spirvCode, VK_SHADER_STAGE_COMPUTE_BIT);

    auto pipelineLayoutCreateInfo = reflector.generateCombinedPipelineLayoutCreateInfo();
    createInfo.layout = pipelineLayoutCreateInfo.create(context->pipelineLayoutCache, context->descriptorSetLayoutCache);

    uint32_t set=0;
    for(auto & c : pipelineLayoutCreateInfo.setLayoutInfos)
    {
        auto l = context->descriptorSetLayoutCache.create(c);
        setLayouts[set] = l;
    }

    auto result = vkCreateComputePipelines(context->getDevice(), nullptr, 1, &createInfo, nullptr, &pipeline);
    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed at compiling pipeline");
    }
}

inline VkDescriptorSet PipelineBase::allocateDescriptorSet(uint32_t setNumber) const
{
    return context->allocateDescriptorSet(getDescriptorSetLayout(setNumber));
}

inline VkShaderModule PipelineBase::_createShader(std::vector<uint32_t> code)
{
    VkShaderModuleCreateInfo ci = {};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = static_cast<uint32_t>(code.size() * 4);
    ci.pCode    = code.data();

    VkShaderModule sh = {};
    auto result = vkCreateShaderModule(context->getDevice(), &ci, nullptr, &sh);
    if(result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed at compiling shader");
    }
    return sh;
}

inline void ShaderStage::destroy()
{
    if(module)
    {
        vkDestroyShaderModule(context->getDevice(), module, nullptr);
        module = VK_NULL_HANDLE;
    }
}

inline ShaderStage::~ShaderStage()
{
    destroy();
}


}

#endif
