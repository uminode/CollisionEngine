#ifdef _WIN32
#include "D3D12Renderer.h"
#include "PathUtils.h"
#include <algorithm>
#include "ImageLoader.h"

D3D12Renderer::D3D12Renderer() : m_cbvSrvDescriptorSize{ 0 }, m_frameIndex { 0 }, m_fenceEvent{ nullptr } {
}
D3D12Renderer::~D3D12Renderer() {
	// TODO: Cleanup code here once we initialize stuff
    WaitForPreviousFrame();
    if (m_fenceEvent != nullptr) {
        CloseHandle(m_fenceEvent);
    }
	for (uint32_t i = 0; i < shaders.size(); ++i) {
        delete shaders[i];
    }
}

void D3D12Renderer::init(uint16_t windowWidth, uint16_t windowHeight) {
    m_width = windowWidth;
    m_height = windowHeight;
    CD3DX12_VIEWPORT viewPort{ 0.f, 0.f, static_cast<float>(windowWidth), static_cast<float>(windowHeight) };
    m_viewport = viewPort;
    CD3DX12_RECT scissorRect{ 0, 0, static_cast<int32_t>(windowWidth), static_cast<int32_t>(windowHeight) };
    m_scissorRect = scissorRect;
    m_frameIndex = 0;
    m_rtvDescriptorSize = 0;

    if (!glfwInit())
    {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(windowWidth, windowHeight, "Shape Rammer 3000", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Window creation failed");
    }
	glfwMakeContextCurrent(window);
    windowHandle = glfwGetWin32Window(window);

	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSwapInterval(0);
    LoadPipeline();
}

void D3D12Renderer::finalizeInit() {
	createDescriptors();

    // upload the texture initialized by loadTexture
    m_commandList->Close();
	ID3D12CommandList* lists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    const uint64_t initFenceValue = 1;
	m_commandQueue->Signal(m_fence.Get(), initFenceValue);
	if (m_fence->GetCompletedValue() < initFenceValue) {
        m_fence->SetEventOnCompletion(initFenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_uploadBuffers.clear();
}

void D3D12Renderer::LoadPipeline() {
    uint32_t dxgiFactoryFlags = 0;
#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif
    ComPtr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);

    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        windowHandle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    ThrowIfFailed(factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create render target view descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ m_rtvHeap->GetCPUDescriptorHandleForHeapStart() };

        for (uint32_t n = 0; n < FrameCount; ++n) {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    for (uint32_t i = 0; i < FrameCount; ++i) {
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        m_fenceValues[i] = 0;
    }

	m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    //m_commandList->Close();

	m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void D3D12Renderer::GetHardwareAdapter(IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;
    *ppAdapter = nullptr;
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
                ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }
            // Check to see if the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                __uuidof(ID3D12Device),
                nullptr)))
            {
                break;
            }
        }
    }
    if (adapter.Get() == nullptr) {
        for (uint32_t adapterIndex = 0;
            SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select Basic Render Driver adapter
                continue;
            }


            // Check to see whether the adapter supports D3D12
            // Don't create the device yet
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    *ppAdapter = adapter.Detach();
}
void D3D12Renderer::initShader(const std::string& path) {

    HLSLShader* aShader = new HLSLShader{ path };

    shaders.push_back(aShader);
	createPSO(*aShader);
}
void D3D12Renderer::initShader(const std::string& vertPath, const std::string& fragPath) {
    HLSLShader* aShader = new HLSLShader{ vertPath, fragPath };

    shaders.push_back(aShader);
	createPSO(*aShader);
    //shaders.back()->Bind(); Create PSO here:
}
void D3D12Renderer::setShader(HLSLShader& shader, int shaderType) {
    //shaders.at(shaderType) = shader;
    shaders.at(shaderType) = &shader;
	createPSO(shader); // TODO: REPLACE NOT CREATE
    //shader.Bind(); // Replace PSO
}
void D3D12Renderer::setViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
	m_viewport = CD3DX12_VIEWPORT{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) };
	m_scissorRect = CD3DX12_RECT{ static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(x + width), static_cast<int32_t>(y + height) };

}
void D3D12Renderer::BindShader(int shaderType) {
    //shaders.at(shaderType)->Bind();
    const auto& pipeline = pipelines[shaderType];
    m_commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
    m_commandList->SetPipelineState(pipeline.pipelineState.Get());

    D3DBuffer& cameraBuffer = m_uboBuffers[CAM_LIGHT_POSITIONS]; // 2
    m_commandList->SetGraphicsRootConstantBufferView(2, cameraBuffer.bufferID->GetGPUVirtualAddress());
}

void D3D12Renderer::unbindShader() {
}

void D3D12Renderer::BindShape(uint16_t shapeType) {
    const auto& shapeBuffer = m_shapeTypeToShapeBuffer[shapeType];
    m_commandList->IASetVertexBuffers(0, 1, &shapeBuffer.vertexBufferView);
    m_commandList->IASetIndexBuffer(&shapeBuffer.indexBufferView);
}

void D3D12Renderer::createDescriptorHeaps() {
	if (m_descriptorHeapCreated) return;
	m_descriptorHeapCreated = true;

    auto roundUpToPowerOf2 = [](uint32_t value) -> uint32_t {
        if (value == 0) return 1;
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return ++value;
		};

    uint32_t totalDescriptors =
        m_descriptorCounts.cbvCount +
        m_descriptorCounts.srvCount +
        m_descriptorCounts.uavCount +
		m_descriptorCounts.samplerCount;
	//uint32_t padding = std::max(4u, totalDescriptors / 10); // 10% padding or at least 4 descriptors
	uint32_t padding = 4u >= totalDescriptors / 10 ? 4u: totalDescriptors / 10; // 10% padding or at least 4 descriptors

	totalDescriptors += padding;
	totalDescriptors = roundUpToPowerOf2(totalDescriptors); // align to 8

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = totalDescriptors;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvHeap)));

	m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

#ifdef _DEBUG_
    std::cout << "Created descriptor heap with " << totalDescriptors << " slots(CBVs: " << m_descriptorCount.cbvCount << ", SRVs: " << m_descriptorCount.srvCount << ")\n";
#endif
}

void D3D12Renderer::createDescriptors() {
    if (!m_descriptorHeapCreated) {
        createDescriptorHeaps();
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle{ m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart() };
    uint32_t currentSlot = 0;
#ifdef _DEBUG
	std::cout << "CBV/SRV/UAV descriptor heap layout:\n";
	std::cout << "CBVs: slots " << currentSlot << "-" << currentSlot + m_descriptorCounts.cbvCount-1 << std::endl;
#endif
	// It's not double buffered since it's UBOs
		for (const auto& [id, buffer] : m_uboBuffers) {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = buffer.bufferID->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<UINT>(buffer.size + 255) & ~255; // CB size is required to be 256-byte aligned.
            m_device->CreateConstantBufferView(&cbvDesc, handle);
#ifdef _DEBUG
			std::cout << "  UBO Type " << id << " at slot " << currentSlot << std::endl;
#endif
            handle.Offset(1, m_cbvSrvDescriptorSize);
            ++currentSlot;
        }
#ifdef _DEBUG
	std::cout << "SRVs: slots " << currentSlot << "-" << currentSlot + m_descriptorCounts.srvCount-1 << std::endl;
#endif
    std::vector<uint16_t> ssboTypes;
    for (const auto& [id, buffer] : m_ssboBuffers[0]) {
        ssboTypes.push_back(id);
    }
    std::sort(ssboTypes.begin(), ssboTypes.end());
    for (uint16_t id : ssboTypes) {
		for (uint32_t frameIdx = 0; frameIdx < FrameCount; ++frameIdx) {
			D3DBuffer& buffer = m_ssboBuffers[frameIdx][id];
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			if (id == CUBE_MATRICES || id == SPHERE_MATRICES || id == CYLINDER_MATRICES || id == RING_MATRICES) {
                srvDesc.Buffer.NumElements = static_cast<UINT>(buffer.size / sizeof(objMatrices));
                srvDesc.Buffer.StructureByteStride = sizeof(objMatrices);
            }
            else if (id == CUBE_COLORS || id == SPHERE_COLORS || id == CYLINDER_COLORS || id == RING_COLORS) {
                srvDesc.Buffer.NumElements = static_cast<UINT>(buffer.size / sizeof(glm::vec4));
                srvDesc.Buffer.StructureByteStride = sizeof(glm::vec4);
            }
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            m_device->CreateShaderResourceView(buffer.bufferID.Get(), &srvDesc, handle);
#ifdef _DEBUG
			std::cout << " SSBO Type" << id << " at slot " << currentSlot << std::endl;
#endif
			handle.Offset(1, m_cbvSrvDescriptorSize);
			++currentSlot;
        }
    }
    // TODO: support multiple textures
    if (m_cubemapTexture) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = m_cubemapTexture->GetDesc().MipLevels;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		m_device->CreateShaderResourceView(m_cubemapTexture.Get(), &srvDesc, handle);
        m_cubemapDescriptorIndex = currentSlot;
    }
}

void D3D12Renderer::createPSO(HLSLShader& shader) {
	Pipeline pipeline;
    pipeline.rootSignature = CreateD3D12RootSignature(shader.getBindings(), "Placeholder");
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = pipeline.rootSignature.Get();
	psoDesc.VS.pShaderBytecode = shader.getVertexShaderBlob()->GetBufferPointer();
    psoDesc.VS.BytecodeLength = shader.getVertexShaderBlob()->GetBufferSize();
	psoDesc.PS.pShaderBytecode = shader.getFragmentShaderBlob()->GetBufferPointer();
    psoDesc.PS.BytecodeLength = shader.getFragmentShaderBlob()->GetBufferSize();
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline.pipelineState)));
    pipelines.push_back(pipeline);
}

void D3D12Renderer::recreateSSBODescriptor(uint16_t type, uint64_t newSize) {
    uint32_t stride{0}, numElements{0};

    if (type == CUBE_MATRICES || type == SPHERE_MATRICES || type == CYLINDER_MATRICES || type == RING_MATRICES) {
        stride = sizeof(objMatrices);
        numElements = newSize / stride;
    }
    else if (type == CUBE_COLORS || type == SPHERE_COLORS || type == CYLINDER_COLORS || type == RING_COLORS) {
        stride = sizeof(glm::vec4);
        numElements = newSize / sizeof(glm::vec4);
    }
    else {
        throw std::runtime_error("Unknown SSBO type");
    }

    for (uint32_t i{ 0 }; i < FrameCount; ++i) {
        const D3DBuffer& buffer = m_ssboBuffers[i][type];

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = numElements;
        srvDesc.Buffer.StructureByteStride = stride;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle{
            m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(buffer.descriptorIndex),
            m_cbvSrvDescriptorSize
        };
        m_device->CreateShaderResourceView(buffer.bufferID.Get(), &srvDesc, srvHandle);
    }
}


ID3D12RootSignature* D3D12Renderer::CreateD3D12RootSignature(const std::vector<ShaderResourceBinding>& bindings, const std::string& shaderName) {

    std::vector<CD3DX12_ROOT_PARAMETER1> rootParams;
	std::vector<CD3DX12_DESCRIPTOR_RANGE1> descriptorRanges;
	std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers;

    auto findRange = [&](uint32_t binding, uint32_t space, D3D12_DESCRIPTOR_RANGE_TYPE type) ->
	CD3DX12_DESCRIPTOR_RANGE1* {
        for (auto& range : descriptorRanges) {
            if (range.BaseShaderRegister == binding && range.RegisterSpace == space && range.RangeType == type) {
                return &range;
            }

		}
        return nullptr;
	};

    std::vector<uint8_t> bindingTypes( bindings.size() ); // 0 = root descriptor, 1 = table, 2 = sampler
    
	// Group bindings by type and push descriptors
	//for (const auto& binding : bindings) {
    for (uint64_t i = 0; i < bindings.size(); ++i ) {
        const auto& binding = bindings[i];
        switch (binding.resourceType) {
        case ShaderResourceBinding::ResourceType::ConstantBuffer: {
            bindingTypes[i] = 0;
            break;
        }
        case ShaderResourceBinding::ResourceType::Texture: {
			// Descriptor table
            bindingTypes[i] = 1;
			auto* existingRange = findRange(binding.binding, binding.set, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
			if (existingRange) {
				uint32_t maxBinding = std::max<uint32_t>(
					existingRange->BaseShaderRegister + existingRange->NumDescriptors - 1,
					binding.binding
				);
				existingRange->NumDescriptors = maxBinding - existingRange->BaseShaderRegister + 1;
                bindingTypes[i] = 3;
			}
			else {
				CD3DX12_DESCRIPTOR_RANGE1 range;
				range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, binding.binding, binding.set);
				descriptorRanges.push_back(range);
			}
            break;
        }
        case ShaderResourceBinding::ResourceType::StructuredBuffer: {
			bindingTypes[i] = 0;
			// Descriptor table
		//	auto* existingRange = findRange(binding.binding, binding.set, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
		//	if (existingRange) {
		//		uint32_t maxBinding = std::max<uint32_t>(
		//			existingRange->BaseShaderRegister + existingRange->NumDescriptors - 1,
		//			binding.binding
		//		);
		//		existingRange->NumDescriptors = maxBinding - existingRange->BaseShaderRegister + 1;
        //        bindingTypes[i] = 3;
		//	}
		//	else {
		//		CD3DX12_DESCRIPTOR_RANGE1 range;
		//		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, binding.binding, binding.set);
		//		descriptorRanges.push_back(range);
		//	}
            break;
        }
        case ShaderResourceBinding::ResourceType::Sampler: {
            bindingTypes[i] = 2; // static sampler
            CD3DX12_STATIC_SAMPLER_DESC sampler(
                binding.binding,
                D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                0.0f, 16,
                D3D12_COMPARISON_FUNC_LESS_EQUAL,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
                0.0f, D3D12_FLOAT32_MAX,
                D3D12_SHADER_VISIBILITY_ALL,
                binding.set
            );
            staticSamplers.push_back(sampler);
            break;
        }
        case ShaderResourceBinding::ResourceType::UAV: {
            bindingTypes[i] = 0; // root UAV
            break;
		}
        default: 
			throw std::runtime_error("Unsupported resource type in root signature creation");
        }
    }
    uint32_t rootParamIndex = 0;
    uint32_t descriptorTableIndex = 0;
    // Root signatures first
    for (uint64_t i = 0; i < bindings.size(); ++i) {
        if (bindingTypes[i] != 0) continue; // skip non root parameters

        const auto& binding = bindings[i];
        CD3DX12_ROOT_PARAMETER1 param;

        uint32_t set = 0;
        switch (binding.resourceType) {
        case ShaderResourceBinding::ResourceType::ConstantBuffer:
            param.InitAsConstantBufferView(binding.binding, binding.set);
            break;
        case ShaderResourceBinding::ResourceType::StructuredBuffer:
            set = 1;
            param.InitAsShaderResourceView(binding.binding, binding.set);
            break;
        case ShaderResourceBinding::ResourceType::UAV:
            set = 1;
            param.InitAsUnorderedAccessView(binding.binding, binding.set);
            break;
        default:
            throw std::runtime_error("Unsupported shader resource type");
        }
        rootParams.push_back(param);
        BindingKey key{ binding.binding, set };
        m_bindingToRootParam[key] = rootParamIndex++;
    }

    // Descriptor tables:
    for (uint64_t i{ 0 }; i < bindings.size(); ++i) {
		const auto& binding = bindings[i];
		uint32_t set = 0;
		// This is a hackjob because slang doesn't want to preserve the bindings as they are.
		if (binding.resourceType == ShaderResourceBinding::ResourceType::StructuredBuffer || binding.resourceType == ShaderResourceBinding::ResourceType::UAV ) {
			set = 1;
		}
        if (bindingTypes[i] == 1) {
            CD3DX12_ROOT_PARAMETER1 param;
            // Find the range
            param.InitAsDescriptorTable(1, &descriptorRanges[descriptorTableIndex++]);
            rootParams.push_back(param);
            BindingKey key{ binding.binding, set };
            m_bindingToRootParam[key] = rootParamIndex++;
        }
        else if (bindingTypes[i] == 3) {
#ifdef _DEBUG
			if (rootParamIndex == 0) throw std::runtime_error("A descriptor table parameter is passed without a descriptor table root parameter."); 
#endif
            BindingKey key{ binding.binding, set };
            m_bindingToRootParam[key] = rootParamIndex - 1;
        }
    }
    // Then descriptor tables
#ifdef _DEBUG
    for (const auto& range : descriptorRanges) {
        std::cout << "Descriptor Range :\n"
            << "  Type: " << (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? "SRV" : "other") << "\n"
            << "  NumDescriptors: " << range.NumDescriptors << "\n"
            << "  BaseShaderRegister: " << range.BaseShaderRegister << "\n"
            << "  RegisterSpace: " << range.RegisterSpace << "\n";
    }
#endif

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(
        static_cast<uint32_t>(rootParams.size()),
        rootParams.data(),
        static_cast<uint32_t>(staticSamplers.size()),
        staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

	ID3DBlob* serialized = nullptr;
	ID3DBlob* error = nullptr;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &serialized,
        &error
	);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
            error->Release();
            throw std::runtime_error("Failed to serialize root signature for shader " + shaderName);
        }
    }
    ID3D12RootSignature* rootSignature = nullptr;
    m_device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    serialized->Release();
    //m_rootSignatures.push_back(rootSignature);
    return rootSignature;


}

void D3D12Renderer::createObjectBuffer(Shape& shape, uint32_t index_pointer_size, uint32_t normal_pointer_size, float* normals, uint32_t* index_array, std::vector<float> objDataVector) {

    ShapeBuffer buffer;
    // Create vertex buffer
    const uint32_t vertexDataSize = shape.size * sizeof(float);
    const uint32_t normalDataSize = normal_pointer_size * sizeof(float);
	const uint32_t totalVertexBufferSize = vertexDataSize + normalDataSize;
	const uint32_t indexBufferSize = index_pointer_size * sizeof(uint32_t);

    buffer.vertexCount = shape.size / 3;
	buffer.indexCount = index_pointer_size;


    CD3DX12_HEAP_PROPERTIES defaultHeap{ D3D12_HEAP_TYPE_DEFAULT };
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalVertexBufferSize);
    m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&buffer.vertexBuffer)
    );

    // To upload the buffer
	CD3DX12_HEAP_PROPERTIES uploadHeap{ D3D12_HEAP_TYPE_UPLOAD };
	CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalVertexBufferSize);

	ComPtr<ID3D12Resource> vertexUploadBuffer;
    m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexUploadBuffer)
    );
	buffer.vertexBuffer->SetName(L"Vertex Buffer (Shape)");

    // To map and copy.
    uint8_t* vertexDataBegin = nullptr;
	CD3DX12_RANGE readRange{ 0, 0 };
	vertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));

    std::memcpy(vertexDataBegin, objDataVector.data(), vertexDataSize);

    std::memcpy(vertexDataBegin + vertexDataSize, normals, normalDataSize);
    
	vertexUploadBuffer->Unmap(0, nullptr);

	m_commandList->CopyBufferRegion(buffer.vertexBuffer.Get(), 0, 
                                    vertexUploadBuffer.Get(), 0,
                                    totalVertexBufferSize);

    CD3DX12_RESOURCE_BARRIER vbBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        buffer.vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );
	m_commandList->ResourceBarrier(1, &vbBarrier);

    // Create index buffer
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&buffer.indexBuffer)
    );
	buffer.indexBuffer->SetName(L"Index Buffer (Shape)");

	uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    ComPtr<ID3D12Resource> indexUploadBuffer;
    m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexUploadBuffer)
    );
    // Map and copy index data
	uint8_t* indexDataBegin = nullptr;
	indexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin));
	std::memcpy(indexDataBegin, index_array, indexBufferSize);
	indexUploadBuffer->Unmap(0, nullptr);

    // Record copy from upload to main buffer
    m_commandList->CopyBufferRegion(
        buffer.indexBuffer.Get(), 0,
        indexUploadBuffer.Get(), 0,
        indexBufferSize
    );

    CD3DX12_RESOURCE_BARRIER ibBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        buffer.indexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER
	);
	m_commandList->ResourceBarrier(1, &ibBarrier);

    // Create views
	buffer.vertexBufferView.BufferLocation = buffer.vertexBuffer->GetGPUVirtualAddress();
	buffer.vertexBufferView.SizeInBytes = totalVertexBufferSize;
	buffer.vertexBufferView.StrideInBytes = sizeof(float) * 3; // we don't have any stride since vertices and normals are in adjacent blocks

    buffer.indexBufferView.BufferLocation = buffer.indexBuffer->GetGPUVirtualAddress();
    buffer.indexBufferView.SizeInBytes = indexBufferSize;
    buffer.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // Keep them alive until copy to final buffer is finalized
    m_uploadBuffers.push_back(vertexUploadBuffer);
    m_uploadBuffers.push_back(indexUploadBuffer);
#ifdef _DEBUG
	std::cout << "Created D3D12 buffers for shape type" << shape.shapeType << "with"  << buffer.vertexCount << " vertices and " << buffer.indexCount << " indices.\n";
#endif
    m_shapeTypeToShapeBuffer[shape.shapeType] = buffer;

}

void D3D12Renderer::createUBO(uint32_t binding, uint16_t type, uint32_t size) {
	uint64_t alignedSize = (size + 255) & ~255; // 256-byte alignment for CBVs
    // store descriptorIndex if we want to bind different UBOs to each root signature

    for (uint32_t i{ 0 }; i < FrameCount; ++i) {
        D3DBuffer uboData{ nullptr, nullptr, alignedSize };

		CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_UPLOAD};
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uboData.bufferID)
		);

        CD3DX12_RANGE readRange{ 0, 0 };
		uboData.bufferID->Map(0, &readRange, &uboData.mappedPtr);
		m_uboBuffers[type] = uboData;
    }
    ++m_descriptorCounts.cbvCount;
}
void D3D12Renderer::createSSBO(uint32_t binding, uint16_t type, uint32_t size) {
    uint32_t startDescriptorIndex = m_descriptorCounts.cbvCount + m_descriptorCounts.srvCount;
    for (uint32_t i{ 0 }; i < FrameCount; ++i) {
        D3DBuffer ssboData{ nullptr, startDescriptorIndex + i, nullptr, size };
        CD3DX12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_UPLOAD };
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ssboData.bufferID)
        );
        CD3DX12_RANGE readRange{ 0, 0 };
        ssboData.bufferID->Map(0, &readRange, &ssboData.mappedPtr);
        m_ssboBuffers[i][type] = ssboData;
    }
    m_descriptorCounts.srvCount += FrameCount;
}

void D3D12Renderer::resizeSSBO(uint16_t type, uint32_t newSize) {

    for (uint32_t i{ 0 }; i < FrameCount; ++i) {
        D3DBuffer& oldBuffer = m_ssboBuffers[i][type];
        uint64_t oldSize = oldBuffer.size;
        // Bigger buffer
        D3DBuffer newSSBO{ nullptr, oldBuffer.descriptorIndex, nullptr, newSize };
        CD3DX12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_UPLOAD };
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(newSize);
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&newSSBO.bufferID)
        );
        CD3DX12_RANGE readRange{ 0, 0 };
        newSSBO.bufferID->Map(0, &readRange, &newSSBO.mappedPtr);

        if (oldBuffer.mappedPtr != nullptr) {
            //uint64_t copySize = oldSize < newSize ? oldSize : newSize;
			std::memcpy(newSSBO.mappedPtr, oldBuffer.mappedPtr, oldSize);

            oldBuffer.bufferID->Unmap(0, nullptr);
            oldBuffer.bufferID.Reset();
        }
        m_ssboBuffers[i][type] = newSSBO;
    }
	recreateSSBODescriptor(type, newSize);
}

void D3D12Renderer::unmapSSBO(uint16_t type) {
    for (uint32_t i{ 0 }; i < FrameCount; ++i) {
        D3DBuffer& buffer = m_ssboBuffers[i][type];
        if (buffer.mappedPtr != nullptr) {
            buffer.bufferID->Unmap(0, nullptr);
            buffer.mappedPtr = nullptr;
        }
    }
}

void D3D12Renderer::BindSSBO(uint32_t binding, uint16_t type) {
	const D3DBuffer& ssbo = m_ssboBuffers[m_frameIndex][type];

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle{
        m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(ssbo.descriptorIndex),
        m_cbvSrvDescriptorSize
    };

	BindingKey key{ binding, 1 };
#ifdef _DEBUG
	auto it = m_bindingToRootParam.find(key);
    if (it == m_bindingToRootParam.end()) {
        throw std::runtime_error("SSBO binding not found in root signature.");
	}
    uint32_t heapIndex = it->second;
    //std::cout << "BindSSBO binding = " << binding << " type = " << (int)type << "heapIndex = " << heapIndex << std::endl;
    m_commandList->SetGraphicsRootShaderResourceView(it->second, ssbo.bufferID->GetGPUVirtualAddress());
    //m_commandList->SetGraphicsRootDescriptorTable(it->second, srvHandle);
#else
    m_commandList->SetGraphicsRootShaderResourceView(m_bindingToRootParam[key], ssbo.bufferID->GetGPUVirtualAddress());
    //m_commandList->SetGraphicsRootDescriptorTable(m_bindingToRootParam[key], srvHandle);
#endif
}

void D3D12Renderer::uploadUBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void* data) {
	D3DBuffer& ubo = m_uboBuffers[type];
	std::memcpy(static_cast<uint8_t*>(ubo.mappedPtr) + offset, data, size);
}

void* D3D12Renderer::getMappedSSBOData(uint16_t type, uint64_t maxSize) {
    D3DBuffer& ssbo = m_ssboBuffers[m_frameIndex][type];
    if (ssbo.size < maxSize) {
        resizeSSBO(type, maxSize);
    }
    return ssbo.mappedPtr;
}

void D3D12Renderer::loadTexture(const std::string& fileName) {

	ImageData image = ImageLoader::load(fileName, 4);

    // Load image data using stbi
    int width = image.width, height = image.height, nrChannels = image.channels;
    uint8_t* data = image.pixels;
   // unsigned char* data = stbi_load(resolvedPath.string().c_str(), &width, &height,
   //     &nrChannels, STBI_rgb_alpha);
    if (!data) {
        throw std::runtime_error("Failed to load texture: " + fileName);
    }

	D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 6;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES defaultHeap{D3D12_HEAP_TYPE_DEFAULT};
    m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_cubemapTexture));

    // Calculate footprint for upload buffer
    const uint32_t numSubresources = 6;
    uint64_t uploadBufferSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[6];
    uint32_t numRows[6];
    uint64_t rowSizesInBytes[6];

    m_device->GetCopyableFootprints(&textureDesc, 0, numSubresources, 0, layouts, numRows, rowSizesInBytes, &uploadBufferSize);

    // Upload buffer
    CD3DX12_HEAP_PROPERTIES uploadHeap{D3D12_HEAP_TYPE_UPLOAD};
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ComPtr<ID3D12Resource> uploadBuffer;
    m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    uint8_t* uploadBufferData = nullptr;
    uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadBufferData));

    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[faceIndex];
        const uint32_t rowCount = numRows[faceIndex];
        const uint64_t rowSizeInBytes = rowSizesInBytes[faceIndex];
        
        // Destination pointer
        uint8_t* destSubresource = uploadBufferData + layout.Offset;

        const uint8_t* srcData = data;
        const uint32_t srcRowPitch = width * 4;
        // Copy row by row
        for (uint32_t row = 0; row < rowCount; ++row) {
            std::memcpy(destSubresource + row * layout.Footprint.RowPitch,
                srcData + row * srcRowPitch,
                rowSizeInBytes);
        }
    }

    uploadBuffer->Unmap(0, nullptr);

    // Record copy commands needs a command list.
	for (uint32_t face = 0; face < 6; ++face) {
        D3D12_TEXTURE_COPY_LOCATION dest = {};
        dest.pResource = m_cubemapTexture.Get();
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = face;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[face];
        m_commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
    }


    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_cubemapTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);

	//stbi_image_free(data);

    // Keep it alive until we upload the texture
	m_uploadBuffers.push_back(uploadBuffer);
}

void D3D12Renderer::waitIdle() {
	// TODO: delete WaitForPreviousFrame 
    WaitForPreviousFrame();
}

void D3D12Renderer::WaitForPreviousFrame() {
   // const uint64_t fence = m_fenceValue;
   // ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
   // m_fenceValue++;
   // if (m_fence->GetCompletedValue() < fence)
   // {
   //     ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
   //     WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
   // }
   // m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
void D3D12Renderer::clear() {
    // Clear render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(m_frameIndex),
        m_rtvDescriptorSize
    };
    const float clearColor[] = { .1f, .1f, .1f, 1.f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    // Set render target
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    // Set viewport and scissor
    m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
}

void D3D12Renderer::clear(CD3DX12_CPU_DESCRIPTOR_HANDLE& rtvHandle) {
    const float clearColor[] = { .1f, .1f, .1f, 1.f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    // Set viewport and scissor
    m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

}

void D3D12Renderer::beginFrame() {
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    const uint64_t fence = m_fenceValues[m_frameIndex];
    if (m_fenceValues[m_frameIndex] != 0) {
        if (m_fence->GetCompletedValue() < fence) {
            m_fence->SetEventOnCompletion(fence, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
    // Reset for rendering
    m_commandAllocators[m_frameIndex]->Reset();
	m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

    // Set descriptor heaps
	ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    // Transition to RENDER_TARGET from PRESENT
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_commandList->ResourceBarrier(1, &barrier);

    clear();

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D12Renderer::renderBatch(int16_t shapeType, uint32_t ib_size, uint32_t amount, uint32_t baseInstance) {
    // Optional, bind shapes here using 
    // BindShape(shapeType);
	m_commandList->DrawIndexedInstanced(ib_size, amount, 0, 0, baseInstance);

}
void D3D12Renderer::drawElements(uint32_t ib_size) {
    // Set CBVs. This should be done using a bindUBO method.
    D3DBuffer& modelBuffer = m_uboBuffers[MODEL_MATRIX]; // 0 
    D3DBuffer& colorBuffer = m_uboBuffers[OBJ_COLOR]; // 1

    m_commandList->SetGraphicsRootConstantBufferView(0, modelBuffer.bufferID->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootConstantBufferView(1, colorBuffer.bufferID->GetGPUVirtualAddress());

	m_commandList->DrawIndexedInstanced(ib_size, 1, 0, 0, 0);
}
void D3D12Renderer::render() {}

void D3D12Renderer::endFrame() {
    // Transition to RENDER_TARGET from PRESENT
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    // Signal and increment the fence value.
    const uint64_t currentFenceValue = m_fenceValues[m_frameIndex] + 1;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));
    m_fenceValues[m_frameIndex] = currentFenceValue;
	//glfwSwapBuffers(window);
	glfwPollEvents();
}
#endif