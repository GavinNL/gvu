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

    gvu::GLSLCompiler compiler;

    std::string glslCode =
R"foo(
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3  in_POSITION  ;

out gl_PerVertex
{
    vec4 gl_Position;
};
layout(location = 0)      out vec3 f_POSITION;
void main()
{
    gl_Position = vec4(in_POSITION,1);
    f_POSITION  = in_POSITION.xyz;
}

)foo";
    auto spvCode = compiler.compile(glslCode, EShLangVertex);

    REQUIRE( spvCode.size() > 0);

    glslang::FinalizeProcess();

}
// #define VMA_IMPLEMENTATION
// #include<vk_mem_alloc.h>
