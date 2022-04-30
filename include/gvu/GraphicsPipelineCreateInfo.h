#ifndef GVU_GRAPHICS_PIPELINE_CREATE_INFO_H
#define GVU_GRAPHICS_PIPELINE_CREATE_INFO_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <map>
#include <streambuf>
#include <fstream>
#include "FormatInfo.h"

// C++17 includes the <filesystem> library, but
// unfortunately gcc7 does not have a finalized version of it
// it is in the <experimental/filesystem lib
// this section includes the proper header
// depending on whether the header exists and
// includes that. It also sets the
// nfcbn::nf namespace
#if __has_include(<filesystem>)

    #include <filesystem>
    namespace guv
    {
        namespace fs = std::filesystem;
    }

#elif __has_include(<experimental/filesystem>)

    #include <experimental/filesystem>
    namespace guv
    {
        namespace fs = std::experimental::filesystem;
    }

#else
    #error There is no <filesystem> or <experimental/filesystem>
#endif


namespace gvu
{

#if defined(__ANDROID__)
#define VK_CHECK_RESULT(f)																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        LOGE("Fatal : VkResult is \" %d \" in %s at line %d", res, __FILE__, __LINE__);					\
        assert(res == VK_SUCCESS);																		\
    }																									\
}
#else
#define VK_CHECK_RESULT(f)																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        std::cout << "Fatal : VkResult is \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
        assert(res == VK_SUCCESS);																		\
    }																									\
}
#endif

struct ShaderModuleCreateInfo
{
    std::vector<uint32_t> code;

    ShaderModuleCreateInfo() = default;
    ShaderModuleCreateInfo(guv::fs::path const &f)
    {
        loadCode(f);
    }
    void loadCode(guv::fs::path const & f)
    {
        std::ifstream t(f);
        std::string str((std::istreambuf_iterator<char>(t)),
                         std::istreambuf_iterator<char>());
        code.resize(str.size() / sizeof(uint32_t));
        std::memcpy(code.data(), str.data(), str.size());
    }

    template<typename callable_t>
    auto create(callable_t && C) const
    {
        VkShaderModuleCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.pCode = code.data();
        ci.codeSize = static_cast<uint32_t>(code.size() * 4);
        return C(ci);
    }
};

/**
 * @brief The GraphicsPipelineCreateInfo struct
 *
 * A simplified GraphicsPipelineCreateInfo struct that
 * you can use to create a graphics pipeline with some default
 * states. T
 *
 * GraphicsPipelineCreateInfo CI;
 *
 * auto pipeline = CI.create([](VkGraphicsPipelineCreateInfo & info)
 * {
 *     // modify info.XXX if you need to do any additional fine-tuning
 *    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS);
 *
 *    return pipeline;
 *  });
 *
 *  This structure can be hashed in an unordered map so that you can
 */
struct GraphicsPipelineCreateInfo
{
    using vertexAttributeLocationIndex_type = uint32_t;
    using vertexAttributeBindingIndex_type = uint32_t;

    std::vector<VkVertexInputBindingDescription>   inputBindings;
    std::vector<VkVertexInputAttributeDescription> inputVertexAttributes;


    VkViewport                  viewport          = {0, 0, 256, 256, 0, 1.f};
    VkRect2D                    scissor           = {{0, 0}, {256, 256}};
    VkPrimitiveTopology         topology          = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode               polygonMode       = VK_POLYGON_MODE_FILL;
    VkCullModeFlags             cullMode          = VK_CULL_MODE_NONE;
    VkFrontFace                 frontFace         = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool                        enableDepthTest   = false;
    bool                        enableDepthWrite  = false;
    uint32_t                    tesselationPatchControlPoints = 3;

    // the number of output framebuffers
    // this would normally be 1 if you are rendering to the swapchain.
    // if you are rendering to deferred framebuffer, change this value.
    // This value sets the blend values to some default.
    uint32_t                    outputColorTargets= 1;
    bool                        enableBlending    = true; // enables blending for all targets, if you want
                                                          // specific control, you will need to modify CreateInfo
                                                          // after the call to .create( VkGraphicsPipelineCreateInfo & info )


    // These must not be null
    VkShaderModule   vertexShader      = VK_NULL_HANDLE;
    VkShaderModule   tessEvalShader    = VK_NULL_HANDLE; // can be left null
    VkShaderModule   tessControlShader = VK_NULL_HANDLE; // can be left null
    VkShaderModule   fragmentShader    = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout    = VK_NULL_HANDLE;
    VkRenderPass     renderPass        = VK_NULL_HANDLE;

    std::vector<VkDynamicState> dynamicStates;

    size_t hash() const
    {
        std::hash<size_t> Hs;
        auto h = Hs(inputBindings.size());

        auto hash_combine = [](std::size_t& seed, const auto& v)
        {
            std::hash<std::decay_t<decltype(v)> > hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        };

        for(auto & a : inputBindings)
        {
            hash_combine(h, a.stride);
            hash_combine(h, a.binding);
            hash_combine(h, a.inputRate);
        }

        hash_combine(h, inputVertexAttributes.size());
        for(auto & a : inputVertexAttributes)
        {
            hash_combine(h, a.format);
            hash_combine(h, a.offset);
            hash_combine(h, a.binding);
            hash_combine(h, a.location);
        }

        hash_combine(h, viewport.x);
        hash_combine(h, viewport.y);
        hash_combine(h, viewport.width);
        hash_combine(h, viewport.height);
        hash_combine(h, scissor.extent.width);
        hash_combine(h, scissor.extent.height);
        hash_combine(h, scissor.offset.x);
        hash_combine(h, scissor.offset.y);
        hash_combine(h, topology                     );
        hash_combine(h, polygonMode                  );
        hash_combine(h, cullMode                     );
        hash_combine(h, frontFace                    );
        hash_combine(h, enableDepthTest              );
        hash_combine(h, enableDepthWrite             );
        hash_combine(h, tesselationPatchControlPoints);

        hash_combine(h, vertexShader      );
        hash_combine(h, tessEvalShader    );
        hash_combine(h, tessControlShader );
        hash_combine(h, fragmentShader    );
        hash_combine(h, pipelineLayout    );
        hash_combine(h, renderPass        );

        hash_combine(h, outputColorTargets);
        hash_combine(h, enableBlending);
        for(auto & s : dynamicStates)
        {
            hash_combine(h, s);
        }
        return h;
    }


    /**
     * @brief setVertexInputs
     * @param formats
     *
     * Sets all the vertex input attributes, each attribute will be
     * on bound to a different buffer binding starting t
     *
     * GraphicsPipelineCreateInfo CI;
     * CI.setVertexInputs({VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32_SFLOAT} );
     *
     * In the shader thsi would look like this:
     *
     * layout(location = 0) in vec3 in_Position;
     * layout(location = 1) in vec3 in_Normal;
     * layout(location = 2) in vec2 in_TexCoord;
     *
     *
     * you would only need to to bind a single buffer where each vertex attribute is
     * a densely packed triple of vec3,vec3,vec2: eg: [p0,n0,u0,p1,n1,u1,p2,n2,u2]
     *
     * VkBuffer buffers[] = {b1,b2,b3};
     * VkDeviceSize offsets[] = {o1,o2,o3};
     * vkCmdBindVertexBuffers(cmd, 0, 3, buffers, offsets);
     */
    void setVertexInputs(std::vector<VkFormat> const & formats)
    {
        uint32_t i = 0;
        constexpr uint32_t bindingBaseIndex = 0;
        constexpr uint32_t locationBaseIndex = 0;
        inputVertexAttributes.clear();
        inputBindings.clear();
        for(auto const & f : formats)
        {
            auto & a = inputVertexAttributes.emplace_back();
            a.format   = f;
            a.offset   = 0;
            a.binding  = bindingBaseIndex + i;
            a.location = locationBaseIndex + i;

            auto & b    = inputBindings.emplace_back();
            b.stride    = getFormatInfo(f).blockSizeInBits / 8;
            b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            b.binding   = a.binding;
            ++i;
        }
    }
    /**
     * @brief setVertexInputs
     * @param locationIndex
     * @param bindingIndex
     * @param formats
     * @param inputRate
     *
     * Sets the vertex inputs for a particular buffer binding.
     *
     * Eg: Setting the position/normal/UV attributes for binding 0
     *
     * GraphicsPipelineCreateInfo CI;
     * CI.setVertexInputs(0, 0, {VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32_SFLOAT} );
     *
     * In the shader thsi would look like this:
     *
     * layout(location = 0) in vec3 in_Position;
     * layout(location = 1) in vec3 in_Normal;
     * layout(location = 2) in vec2 in_TexCoord;
     *
     *
     * you would only need to to bind a single buffer where each vertex attribute is
     * a densely packed triple of vec3,vec3,vec2: eg: [p0,n0,u0,p1,n1,u1,p2,n2,u2]
     *
     * vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
     */
    void setVertexInputs(vertexAttributeBindingIndex_type  bindingIndex,
                         vertexAttributeLocationIndex_type locationBaseIndex,
                         std::vector<VkFormat> const & formats,
                         VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX)
    {
        uint32_t offset = 0;
        uint32_t i = 0;
        for(auto & f : formats)
        {
            VkVertexInputAttributeDescription a;
            a.format   = f;
            a.offset   = offset;
            a.binding  = bindingIndex;
            a.location = locationBaseIndex + i;
            offset    += getFormatInfo(f).blockSizeInBits / 8;
            inputVertexAttributes.push_back(a);
            i++;
        }
        auto & b = inputBindings.emplace_back();
        b.stride = offset;
        b.inputRate = inputRate;
    }

    /**
     * @brief create
     * @param C
     * @return
     *
     * This function is used to generate an actual VkGraphicsPipelineCreateInfo structure
     * which you can use to generate your pipeline
     *
     * GraphicsPipelineCreateInfo CI;
     *
     * auto pipeline = CI.create([](VkGraphicsPipelineCreateInfo & info)
     * {
     *     // modify info.XXX if you need to do any additional fine-tuning
     *
     *    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS);
     *
     *    return pipeline;
     *  });
     */
    template<typename callable_t>
    auto create(callable_t && C) const
    {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        {
            auto & vertShaderStageInfo = shaderStages.emplace_back();
            vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertexShader;
            vertShaderStageInfo.pName  = "main";
        }

        {
            auto & fragShaderStageInfo = shaderStages.emplace_back();
            fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragmentShader;
            fragShaderStageInfo.pName  = "main";
        }

        if( tessControlShader != VK_NULL_HANDLE &&  tessEvalShader != VK_NULL_HANDLE )
        {
            {
                auto & stageInfo = shaderStages.emplace_back();
                stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageInfo.stage  = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                stageInfo.module = tessControlShader;
                stageInfo.pName  = "main";
            }
            {
                auto & stageInfo = shaderStages.emplace_back();
                stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageInfo.stage  = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                stageInfo.module = tessEvalShader;
                stageInfo.pName  = "main";
            }
        }

#if 0
        std::vector<VkVertexInputBindingDescription>   inputBindings;
        std::vector<VkVertexInputAttributeDescription> inputVertexAttributes;

        {
            for(auto & [binding, bindingInfo] : vertexBindings)
            {
                auto & info = inputBindings.emplace_back();
                info.binding   = binding;
                info.inputRate = bindingInfo.inputRate;
                info.stride    = bindingInfo.stride;
            }
            for(auto & [location, attributes] : vertexAttributes)
            {
                uint32_t offset = 0;
                for(auto & a : attributes)
                {
                    auto &  va = inputVertexAttributes.emplace_back();
                    va.location = location;
                    va.binding  = a.binding;
                    va.format   = a.format;
                    va.offset   = offset;
                    offset += getFormatInfo(va.format).blockSizeInBits / 8;
                    if( vertexBindings.count(a.binding) == 0)
                    {
                        throw std::runtime_error("Binding" + std::to_string(a.binding) + " is not in the vertexBinding map");
                    }
                }
            }
        }
#endif


        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(inputVertexAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions    = inputVertexAttributes.data();

        vertexInputInfo.pVertexBindingDescriptions = inputBindings.data();
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(inputBindings.size());

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = polygonMode;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = cullMode;
        rasterizer.frontFace               = frontFace;
        rasterizer.depthBiasEnable         = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable         = enableBlending;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;// vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;// vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; //vk::BlendFactor::eOneMinusDstAlpha;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;//vk::BlendFactor::eOneMinusDstAlpha;
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments( outputColorTargets, colorBlendAttachment);

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.logicOp           = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = colorBlendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;


        VkPipelineDepthStencilStateCreateInfo dsInfo = {};
        dsInfo.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthTestEnable                       = enableDepthTest;
        dsInfo.depthWriteEnable                      = enableDepthWrite;
        dsInfo.depthCompareOp                        = VK_COMPARE_OP_LESS;
        dsInfo.minDepthBounds                        = 0.0f;
        dsInfo.maxDepthBounds                        = 1.0f;
        dsInfo.stencilTestEnable                     = false;

        VkPipelineDynamicStateCreateInfo dynamicStatesCI = {};
        dynamicStatesCI.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStatesCI.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
        dynamicStatesCI.pDynamicStates                   = dynamicStates.data();

        VkPipelineTessellationStateCreateInfo tessellationState;
        tessellationState.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessellationState.patchControlPoints = tesselationPatchControlPoints;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages             = shaderStages.data();
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDepthStencilState  = &dsInfo;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = renderPass;
        pipelineInfo.pDynamicState       = &dynamicStatesCI;
        pipelineInfo.pTessellationState  = &tessellationState;
        if( tessControlShader == VK_NULL_HANDLE)
            pipelineInfo.pTessellationState  = nullptr;
        pipelineInfo.subpass             = 0;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

        return C(pipelineInfo);
    }
};

}

#endif

