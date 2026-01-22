#include "ShaderCompiler.h"

SlangCompiler::SlangCompiler()
{
    // Create a global session that can be reused.
    slang::createGlobalSession(&desc, m_globalSession.writeRef());
}

SlangCompiler::~SlangCompiler()
{
    /*
    if (m_globalSession)
    {
        m_globalSession->release();
    }
    */
}

// Compile to GLSL text - returns all entry points
std::vector<ShaderOutput> SlangCompiler::compileToGLSL(const std::string& source,
    const std::vector<std::string>& entryPoints)
{
    return compile(source, entryPoints, SLANG_GLSL);
}

// Compile to HLSL text - returns all entry points
std::vector<ShaderOutput> SlangCompiler::compileToHLSL(const std::string& source,
    const std::vector<std::string>& entryPoints)
{
    return compile(source, entryPoints, SLANG_HLSL);
}

// Compile to SPIR-V binary - returns all entry points
std::vector<ShaderOutput> SlangCompiler::compileToSPIRV(const std::string& source,
    const std::vector<std::string>& entryPoints)
{
    return compile(source, entryPoints, SLANG_SPIRV);
}

// Convenience overloads for single entry point
std::string SlangCompiler::compileToGLSLSingle(const std::string& source,
    const std::string& entryPoint)
{
    std::vector<ShaderOutput> outputs = compileToGLSL(source, { entryPoint });
    if (outputs.empty()) return "";
    return outputs[0].asText();
}

std::string SlangCompiler::compileToHLSLSingle(const std::string& source,
    const std::string& entryPoint)
{
    std::vector<ShaderOutput> outputs = compileToHLSL(source, { entryPoint });
    if (outputs.empty()) return "";
    return outputs[0].asText();
}

std::vector<uint8_t> SlangCompiler::compileToSPIRVSingle(const std::string& source,
    const std::string& entryPoint)
{
    std::vector<ShaderOutput> outputs = compileToSPIRV(source, { entryPoint });
    if (outputs.empty()) return std::vector<uint8_t>();
    return outputs[0].binaryData;
}

std::vector<ShaderOutput> SlangCompiler::compile(const std::string& source,
    const std::vector<std::string>& entryPoints,
    SlangCompileTarget target)
{
    std::vector<ShaderOutput> outputs;

    if (entryPoints.empty())
    {
        throw std::runtime_error("No entry points specified");
    }

    // Setup session descriptor
    slang::SessionDesc sessionDesc{};
    slang::TargetDesc targetDesc{};
    Slang::ComPtr<slang::ISession> session;

    targetDesc.format = target;
    targetDesc.profile = m_globalSession->findProfile("sm_6_8");

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    const char* searchPaths[] = { "./", "src/shaders/", "./shaders/", "../shaders/", "../../shaders/" };
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 3;

    // Create session
    SlangResult result = m_globalSession->createSession(sessionDesc, session.writeRef());
    if (SLANG_FAILED(result) || !session)
    {
        throw std::runtime_error("Failed to create Slang session");
    }

    // Load module from source string
    Slang::ComPtr<slang::IBlob> diagnostics;

	// TODO: specify module name and file name properly instead of shader and shader.slang
    slang::IModule* loadedModule = session->loadModuleFromSourceString(
        "shader",
        "shader.slang",
        source.c_str(),
        diagnostics.writeRef());

    if (diagnostics && diagnostics->getBufferSize() > 0)
    {
        std::string diagStr((const char*)diagnostics->getBufferPointer(),
            diagnostics->getBufferSize());
        if (!diagStr.empty())
        {
            std::cerr << "Slang diagnostics:\n" << diagStr << "\n";
        }
    }

    if (!loadedModule)
    {
        throw std::runtime_error("Failed to load Slang module from source");
    }

    // Find all entry points
    std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPointObjs;
    for (const auto& entryPointName : entryPoints)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPointObj;
        loadedModule->findEntryPointByName(entryPointName.c_str(), entryPointObj.writeRef());

        if (!entryPointObj)
        {
            throw std::runtime_error("Failed to find entry point: " + entryPointName);
        }

        entryPointObjs.push_back(entryPointObj);
    }

    // Create composite component type (module + all entry points)
    std::vector<slang::IComponentType*> components;
    components.push_back(loadedModule);
    for (auto& ep : entryPointObjs)
    {
        components.push_back(ep.get());
    }

    Slang::ComPtr<slang::IComponentType> program;
    session->createCompositeComponentType(
        components.data(),
        (SlangInt)components.size(),
        program.writeRef(),
        diagnostics.writeRef());

    if (!program)
    {
        throw std::runtime_error("Failed to create composite component type");
    }

    // Link the program once for all entry points
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    program->link(linkedProgram.writeRef(), diagnostics.writeRef());

    if (diagnostics)
    {
        std::string diagStr((const char*)diagnostics->getBufferPointer(),
            diagnostics->getBufferSize());
        if (!diagStr.empty())
        {
            std::cerr << "Slang link diagnostics:\n" << diagStr << "\n";
        }
    }

    if (!linkedProgram)
    {
        throw std::runtime_error("Failed to link program");
    }

    // Get compiled code for each entry point
    int targetIndex = 0;
    for (size_t i = 0; i < entryPoints.size(); ++i)
    {
        Slang::ComPtr<slang::IBlob> codeBlob;

        linkedProgram->getEntryPointCode(
            (int)i,
            targetIndex,
            codeBlob.writeRef(),
            diagnostics.writeRef());

        if (!codeBlob)
        {
            std::cerr << "Failed to get code for entry point: " << entryPoints[i] << "\n";
            continue;
        }

        // Store output
        ShaderOutput output;
        // only store resource bindings on first output
        if ( 0 == i ) {
			output.resourceBindings = extractResourceBindings(linkedProgram.get());
        }
        output.target = target;
        output.entryPointName = entryPoints[i];
        const uint8_t* data = static_cast<const uint8_t*>(codeBlob->getBufferPointer());
        uint64_t size = codeBlob->getBufferSize();
        output.binaryData.assign(data, data + size);

        outputs.push_back(output);
    }

    return outputs;
}


std::vector<ShaderResourceBinding> SlangCompiler::extractResourceBindings(slang::IComponentType* program) {
    std::vector<ShaderResourceBinding> bindings;

    if (!program) {
        throw std::runtime_error("Invalid program for resource binding extraction");
    }
    slang::ProgramLayout* programLayout{ program->getLayout() };
    if (!programLayout) {
        throw std::runtime_error("Failed to get program layout for resource binding extraction");
    }
    slang::TypeLayoutReflection* globalParams{ programLayout->getGlobalParamsTypeLayout() };

    uint32_t fieldCount = globalParams->getFieldCount();

    for (uint32_t i{ 0 }; i < fieldCount; ++i) {
        slang::VariableLayoutReflection* field{ globalParams->getFieldByIndex(i) };
        slang::TypeLayoutReflection* typeLayout{ field->getTypeLayout() };
        slang::TypeReflection* type{ typeLayout->getType() };
        slang::ParameterCategory category{ typeLayout->getParameterCategory() };
        ShaderResourceBinding binding;
        binding.name = field->getName();

        // Get binding information
        slang::TypeReflection::Kind kind = type->getKind();
        //uint32_t bindingSpace = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::RegisterSpace) + field->getOffset(slang::ParameterCategory::SubElementRegisterSpace));
        switch (kind)
        {
        case slang::TypeReflection::Kind::ConstantBuffer:
        {
            binding.resourceType = ShaderResourceBinding::ResourceType::ConstantBuffer;
            binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::ConstantBuffer));
            binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::ConstantBuffer));

            bindings.push_back(binding);
            break;
        }
        case slang::TypeReflection::Kind::Resource:
        {
            SlangResourceShape shape = type->getResourceShape();
            SlangResourceAccess access = type->getResourceAccess();
            if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER) {
                if (access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ_WRITE) {
                    binding.resourceType = ShaderResourceBinding::ResourceType::UAV;
                    binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::UnorderedAccess));
                    binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::UnorderedAccess));
                }
                else {
                    binding.resourceType = ShaderResourceBinding::ResourceType::StructuredBuffer;
                    binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::ShaderResource));
                    binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::ShaderResource)); 
                }
                bindings.push_back(binding);
            }
            else if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_TEXTURE_2D ||
                (shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_TEXTURE_3D ||
                (shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_TEXTURE_CUBE) {
                if (access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ_WRITE) {
                    binding.resourceType = ShaderResourceBinding::ResourceType::UAV;
                    binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::UnorderedAccess));
                    binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::UnorderedAccess));
                }
                else {
                    binding.resourceType = ShaderResourceBinding::ResourceType::Texture;
                    binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::ShaderResource));
                    binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::ShaderResource));
                }
                bindings.push_back(binding);
                auto samplerOffset = field->getOffset(slang::ParameterCategory::SamplerState);
                if (samplerOffset >= 0) {
                    ShaderResourceBinding samplerBinding;
                    samplerBinding.name = binding.name + "_sampler";
                    samplerBinding.resourceType = ShaderResourceBinding::ResourceType::Sampler;
                    samplerBinding.binding = static_cast<uint32_t>(samplerOffset);
                    samplerBinding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::SamplerState));
                    bindings.push_back(samplerBinding);
                }
            }
            break;
        }
        case slang::TypeReflection::Kind::SamplerState:
        {
            binding.resourceType = ShaderResourceBinding::ResourceType::Sampler;
            binding.binding = static_cast<uint32_t>(field->getOffset(slang::ParameterCategory::SamplerState));
            binding.set = static_cast<uint32_t>(field->getBindingSpace(slang::ParameterCategory::SamplerState));
            bindings.push_back(binding);
            break;
        }
        default:
            break;
        }
    }
    return bindings;
}
