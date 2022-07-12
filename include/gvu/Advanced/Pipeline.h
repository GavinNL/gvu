#ifndef GVU_VULKAN_APPLICATION_CONTEXT_PIPELINE_H
#define GVU_VULKAN_APPLICATION_CONTEXT_PIPELINE_H

#include <iostream>
#include <cassert>
#include <gvu/Extension/spirvPipelineReflector.h>
#include <gvu/Core/GraphicsPipelineCreateInfo.h>
#include <GLSLCompiler.h>

namespace gvu
{
struct VulkanApplicationContext;

struct PipelineBase
{
    VkPipeline getPipeline() const
    {
        return pipeline;
    }
    gvu::spirvPipelineReflector const & getReflector() const
    {
        return reflector;
    }
    VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setNumber) const
    {
        return setLayouts.at(setNumber);
    }
    /**
     * @brief allocateDescriptorSet
     * @param setNumber
     * @return
     *
     * Allocate a descriptor set for a particular set layout
     * within this pipeline
     */
    VkDescriptorSet allocateDescriptorSet(uint32_t setNumber) const;
protected:
    VkShaderModule _createShader(std::vector<uint32_t> code);

    VkPipeline                                          pipeline = VK_NULL_HANDLE;
    std::unordered_map<uint32_t, VkDescriptorSetLayout> setLayouts;
    gvu::spirvPipelineReflector                         reflector;
    std::shared_ptr<VulkanApplicationContext>           context;

    friend struct VulkanApplicationContext;
};


struct GraphicsPipeline;

struct ShaderStage
{
    VkShaderStageFlagBits                        stage;
    std::string                                  glslCode;
    std::vector<uint32_t>                        spirvCode;
    std::vector<std::filesystem::path>           includePaths;
    std::map<std::string, std::string>           compileTimeDefinitions;

    /**
     * @brief loadVertexGLSL
     * @param p
     * @param includeFilePathAsIncludeDir
     * @return
     *
     * Load GLSL code from a file path.
     */
    ShaderStage& loadGLSL(std::filesystem::path const & p, bool includeFilePathAsIncludeDir=true)
    {
        std::ifstream t(p);
        std::string srcString((std::istreambuf_iterator<char>(t)),
                         std::istreambuf_iterator<char>());

        glslCode = std::move(srcString);
        if(includeFilePathAsIncludeDir)
        {
            appendIncludePath(p.parent_path());
        }
        return *this;
    }
    ShaderStage& setGLSL(std::string const & srcString)
    {
        glslCode = std::move(srcString);
        return *this;
    }


    ShaderStage& appendIncludePath(std::filesystem::path const & p)
    {
        includePaths.push_back(p);
        return *this;
    }


    ShaderStage& addCompileTimeDefinition(std::string const & var, std::string const &value)
    {
        compileTimeDefinitions[var] = value;
        return *this;
    }

    void compile()
    {
        gnl::GLSLCompiler compiler;
        for(auto & [key, value] : compileTimeDefinitions)
        {
            compiler.addCompleTimeDefinition(key, value);
        }
        for(auto & p : includePaths)
        {
            compiler.addIncludePath(p.native());
        }

        EShLanguage _type;
        switch(stage)
        {
            case VK_SHADER_STAGE_VERTEX_BIT: _type = EShLangVertex; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:  _type = EShLangTessControl; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:  _type = EShLangTessEvaluation; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT:  _type = EShLangGeometry; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:  _type = EShLangFragment; break;
            case VK_SHADER_STAGE_COMPUTE_BIT:  _type = EShLangCompute; break;
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  _type = EShLangRayGen; break;
            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  _type = EShLangAnyHit; break;
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  _type = EShLangClosestHit; break;
            case VK_SHADER_STAGE_MISS_BIT_KHR:  _type = EShLangMiss; break;
            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  _type = EShLangIntersect; break;
            case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  _type = EShLangCallable; break;
            case VK_SHADER_STAGE_TASK_BIT_NV:  _type = EShLangTaskNV; break;
            case VK_SHADER_STAGE_MESH_BIT_NV: _type = EShLangMeshNV; break;
            case VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI:
            default:
    #if 0
            case VK_SHADER_STAGE_ALL_GRAPHICS:
            case VK_SHADER_STAGE_ALL:
            case VK_SHADER_STAGE_RAYGEN_BIT_NV:
            case VK_SHADER_STAGE_ANY_HIT_BIT_NV:  _type = EShLangAnyHitNV; break;
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV:  _type = EShLangClosestHitNV; break;
            case VK_SHADER_STAGE_MISS_BIT_NV:  _type = EShLangMissNV; break;
            case VK_SHADER_STAGE_INTERSECTION_BIT_NV:  _type = EShLangIntersectNV; break;
            case VK_SHADER_STAGE_CALLABLE_BIT_NV:  _type = EShLangCallableNV; break;
            case VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM:  _type = EShLangVertex; break;
    #endif
                break;
        }

        spirvCode = compiler.compile(glslCode, _type);
    }


    std::shared_ptr<VulkanApplicationContext> context;
      friend struct VulkanApplicationContext;
};


struct ComputePipeline : public PipelineBase
{
    VkPipelineLayout getPipelineLayout() const
    {
        return createInfo.layout;
    }

    /**
     * @brief build
     *
     * Build the pipeline
     */
    void build();

    /**
     * @brief destroy
     *
     * Destroy the pipeline
     */
    void destroy();


    ShaderStage & getComputeStage()
    {
        return computeStage;
    }
protected:
    VkComputePipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ShaderStage computeStage = {VK_SHADER_STAGE_COMPUTE_BIT};
    friend class VulkanApplicationContext;
};


/**
 * @brief The Pipeline struct
 *
 * This pipeline struct is a help object and wrapper around
 * a VkPipeline.
 */
struct GraphicsPipeline : public PipelineBase
{

public:
    std::shared_ptr<GraphicsPipeline> clone() const
    {
        auto p = std::make_shared<GraphicsPipeline>(*this);
        p->pipeline = VK_NULL_HANDLE;
        return p;
    }
    VkPipelineLayout getPipelineLayout() const
    {
        return createInfo.pipelineLayout;
    }
    VkRenderPass getRenderPass() const
    {
        return createInfo.renderPass;
    }

    void setRenderPass(VkRenderPass rp)
    {
        createInfo.renderPass = rp;
    }

    /**
     * @brief build
     *
     * Build the pipeline
     */
    void build();

    /**
     * @brief destroy
     *
     * Destroy the pipeline
     */
    void destroy();


    ShaderStage& getVertexStage()
    {
        return vertexStage;
    }

    ShaderStage& getFragmentStage()
    {
        return fragmentStage;
    }

    void setPrimitiveTopology(VkPrimitiveTopology t)
    {
        createInfo.topology = t;
    }
    /**
     * @brief setOutputFormat
     * @param index
     * @param format
     *
     * Sets the output color format
     */
    void setOutputFormat(uint32_t index, VkFormat format)
    {
        createInfo.setOutputFormat(index, format);
    }
    void setDepthFormat(VkFormat format)
    {
        createInfo.setDepthFormat(format);
    }
    auto & getCreateInfo()
    {
        return createInfo;
    }
protected:
    gvu::GraphicsPipelineCreateInfo createInfo;
    ShaderStage vertexStage = {VK_SHADER_STAGE_VERTEX_BIT};
    ShaderStage fragmentStage  = {VK_SHADER_STAGE_FRAGMENT_BIT};

    friend struct VulkanApplicationContext;
};

using GraphicsPipelineHandle = std::shared_ptr<GraphicsPipeline>;
using ComputePipelineHandle  = std::shared_ptr<ComputePipeline>;
}

#endif
