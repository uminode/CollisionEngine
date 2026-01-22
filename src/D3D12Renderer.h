#pragma once
#ifdef _WIN32

#include <unordered_map>
#include "Renderer.h"
#include "DxHelper.h"
#include "HLSLShader.h"
#include <glm/glm.hpp>
#include <Windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

using Microsoft::WRL::ComPtr;

constexpr uint32_t UBO_ALIGNED_SIZE = 256;

struct D3DBuffer : PersistentBuffer {
	ComPtr<ID3D12Resource> bufferID;
	uint32_t descriptorIndex = 0;
	D3DBuffer() : PersistentBuffer{}, bufferID{ nullptr } {}
	D3DBuffer(ComPtr<ID3D12Resource> buf, void* ptr, uint64_t size) : PersistentBuffer{ ptr, size }, bufferID{ buf } {}
	D3DBuffer(ComPtr<ID3D12Resource> buf, uint32_t descriptorIndex_, void* ptr, uint64_t size) : PersistentBuffer{ ptr, size }, descriptorIndex{descriptorIndex_}, bufferID { buf } {}
};
struct ShapeBuffer {
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	uint32_t indexCount;
	uint32_t vertexCount;
};

struct DescriptorCounts {
	uint32_t cbvCount = 0;
	uint32_t srvCount = 0;
	uint32_t uavCount = 0;
	uint32_t samplerCount = 0;
};

static uint16_t constexpr FrameCount = 2;
static uint16_t m_width = 0;
static uint16_t m_height = 0;
struct Pipeline {
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12RootSignature> rootSignature;
};
struct BindingKey {
	uint32_t binding;
	// NOTE: BECAUSE OF SLANG OBFUSCATING THE SET TO SOMETHING ELSE,
	// I'M EXPLICITLY BINDING SSBOS AND UAVS TO SET 1, UBOS TO SET 0
	// That's because I can't differentiate between them otherwise.
	uint32_t set;
	bool operator ==(const BindingKey& other) const {
		return binding == other.binding && set == other.set;
	}
};
struct BindingKeyHash {
	std::size_t operator()(const BindingKey& k) const {
		return (k.set << 16) | k.binding;
	}
};

class D3D12Renderer : public Renderer {
private:
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	std::vector<Pipeline> pipelines;
	//std::vector<ComPtr<ID3D12RootSignature>> m_rootSignatures; // look below for details
	//ComPtr<ID3D12RootSignature> m_rootSignature_Single; // b0, b1, b2 for UbOs (CBV), t0, s0 for texture
	//ComPtr<ID3D12RootSignature> m_rootSignature_Batch; // t0, t1 for SSBOs (SRV), b2 for UBO

	// Buffer stuff
	// TODO: change shape to ShapeType or to uint16_t
	std::unordered_map<uint32_t, ShapeBuffer> m_shapeTypeToShapeBuffer;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	uint32_t m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
	uint32_t m_cbvSrvDescriptorSize;
	//std::unordered_map <uint16_t, D3DBuffer> m_uboBuffers[FrameCount]; 
	std::unordered_map <uint16_t, D3DBuffer> m_uboBuffers; 
	std::unordered_map <uint16_t, D3DBuffer> m_ssboBuffers[FrameCount];
	std::unordered_map <BindingKey, uint32_t, BindingKeyHash> m_bindingToRootParam;
	ComPtr<ID3D12Resource> m_cubemapTexture;
	std::vector<ComPtr<ID3D12Resource>> m_uploadBuffers;
	uint32_t m_cubemapDescriptorIndex = 0;
	DescriptorCounts m_descriptorCounts;
	bool m_descriptorHeapCreated = false;
	bool m_cubemapTextureLoaded = false;

	//ComPtr<ID3D12PipelineState> m_pipelineState;

	// App resources
	std::unordered_map<ShapeType, ShapeBuffer> shapeBufferMap;

	// Sync objects
	int32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValues[FrameCount];


	GLFWwindow* window = nullptr;
	std::vector<HLSLShader*> shaders;
	HWND windowHandle;
	void LoadPipeline();
	void GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = true);
	ID3D12RootSignature* CreateD3D12RootSignature(const std::vector<ShaderResourceBinding>& bindings, const std::string& shaderName);
	void createPSO(HLSLShader& shader);

	void WaitForPreviousFrame();
public:
	D3D12Renderer();
	~D3D12Renderer();

	void init(uint16_t windowWidth, uint16_t windowHeight) override;
	void finalizeInit() override;
	void createDescriptorHeaps();
	void createDescriptors();
	void recreateSSBODescriptor(uint16_t type, uint64_t newSize);

	inline GLFWwindow* getWindow() { return window; };
	void setShader(HLSLShader& shader, int shaderType);
	void initShader(const std::string& path);
	void initShader(const std::string& vertPath, const std::string& fragPath);
	void loadTexture(const std::string& fileName) override;

	void BindShader(int shaderType = 0) override;
	void unbindShader();
	void BindShape(uint16_t shapeType) override;
	void setViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override;

	void createUBO(uint32_t binding, uint16_t type, uint32_t size);
	void createSSBO(uint32_t binding, uint16_t type, uint32_t size);
	void uploadUBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void* data);
	// Is not needed as mapped pointer is used for SSBOs
	//void uploadSSBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void *data);
	// TODO: check with vulkan/dx12 to see if unmap is needed there. Add it to renderer?
	void* getMappedSSBOData(uint16_t type, uint64_t maxSize);
	void resizeSSBO(uint16_t type, uint32_t newSize);
	void unmapSSBO(uint16_t type);
	void BindSSBO(uint32_t binding, uint16_t type);

	void waitIdle() override;

	void createObjectBuffer(Shape& shape, uint32_t index_pointer_size, uint32_t normal_pointer_size, float* normals, uint32_t* index_array, std::vector<float> objDataVector) override;

	void clear() override;
	void clear(CD3DX12_CPU_DESCRIPTOR_HANDLE& rtvHandle);

	void beginFrame() override;
	void endFrame() override;
	void render() override;
	void renderBatch(int16_t shapeType, uint32_t ib_size, uint32_t amount, uint32_t baseInstance);
	void drawElements(uint32_t ib_size); // temporary to accelerate integration

};
#endif
