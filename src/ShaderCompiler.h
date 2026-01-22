#pragma once
// SlangCompiler.h
// A helper class that uses the Slang C API directly to compile shaders
// into GLSL, SPIR-V, or HLSL.
#include <slang.h>
#include <slang-com-ptr.h> 
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct ShaderResourceBinding
{
    enum class ResourceType { ConstantBuffer, StructuredBuffer, Texture, Sampler, UAV };

    ResourceType resourceType;
    // Set to something invalid
    uint32_t binding{ 999999 };   // Vulkan binding or D3D register 
    uint32_t set{ 999999 };       // Vulkan set or D3D space
    uint32_t count{ 9999999 };

    std::string name{ "" };
    // Then have separate converters:
    // - ToD3D12RootSignature()
    // - ToVulkanDescriptorSetLayout()
};

struct ShaderOutput
{
    SlangCompileTarget target = SLANG_TARGET_UNKNOWN;
    std::string entryPointName;
    std::vector<uint8_t> binaryData; // For SPIR-V and text formats
	std::vector<ShaderResourceBinding> resourceBindings;

    std::string asText() const
    {
        return std::string(binaryData.begin(), binaryData.end());
    }
};

class SlangCompiler
{
public:
    SlangCompiler();
    ~SlangCompiler();

    // Compile multiple entry points to GLSL in one pass
    std::vector<ShaderOutput> compileToGLSL(const std::string& source,
        const std::vector<std::string>& entryPoints);

    // Compile multiple entry points to HLSL in one pass
    std::vector<ShaderOutput> compileToHLSL(const std::string& source,
        const std::vector<std::string>& entryPoints);

    // Compile multiple entry points to SPIR-V in one pass
    std::vector<ShaderOutput> compileToSPIRV(const std::string& source,
        const std::vector<std::string>& entryPoints);

    // Convenience methods for single entry point (returns just the text/data)
    std::string compileToGLSLSingle(const std::string& source,
        const std::string& entryPoint);

    std::string compileToHLSLSingle(const std::string& source,
        const std::string& entryPoint);

    std::vector<uint8_t> compileToSPIRVSingle(const std::string& source,
        const std::string& entryPoint);

private:
    Slang::ComPtr<slang::IGlobalSession> m_globalSession = nullptr;
    SlangGlobalSessionDesc desc = {};

    std::vector<ShaderOutput> compile(const std::string& source,
        const std::vector<std::string>& entryPoints,
        SlangCompileTarget target);

    std::vector<ShaderResourceBinding> extractResourceBindings(slang::IComponentType* program);
};