#pragma once
#include "opengl.h"
#include "Renderer.h"
#include "GLSLShader.h"
#include "Shape.h"
#include <vector>
#include <map>
#include <string>

struct OpenGLBuffer : PersistentBuffer {
	uint32_t bufferID; 
	OpenGLBuffer() : PersistentBuffer{}, bufferID{ 0 } {}
	OpenGLBuffer(uint32_t id, void* ptr, uint64_t size) {
		bufferID = id;
		this->mappedPtr = ptr;
		this->size = size;
	}
};

class OpenGLRenderer : public Renderer
{
private:
	GLFWwindow *window;
	std::vector<GLSLShader*> shaders;
	std::unordered_map<uint32_t, uint32_t> typeToUBOMap;
	std::unordered_map<uint32_t, uint32_t> typeToUBOSize;
	std::unordered_map<uint32_t, uint32_t> typeToSSBOMap;
	std::unordered_map<uint32_t, uint32_t> typeToSSBOSize;
	std::map<uint32_t, int> shapeVAOIDmap;
	std::map<uint32_t, int> shapeVBOIDmap;
	std::map<uint32_t, int> shapeIBOIDmap;
	std::vector<uint32_t> textures;
	GLbitfield ssboUsageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	//std::map<uint32_t, PersistentBuffer> typeToPersistentSSBOMap;
	std::map<uint32_t, OpenGLBuffer> typeToPersistentSSBOMap;
	//std::vector<GLuint> framebuffers;
public:
	OpenGLRenderer();
	~OpenGLRenderer();

	void init(uint16_t windowWidth, uint16_t windowHeight) override;
	void inline finalizeInit() override {};

	inline GLFWwindow* getWindow() { return window; };
	void setShader(GLSLShader& shader, int shaderType);
	void initShader(const std::string& path);
	void initShader(const std::string& vertPath, const std::string& fragPath);
	void loadTexture(const std::string &fileName) override;

	void BindShader(int shaderType = 0) override;
	void unbindShader();
	void BindShape(uint16_t shapeType) override;
	void setViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override;

	void createUBO(uint32_t binding, uint16_t type, uint32_t size);
	void createSSBO(uint32_t binding, uint16_t type, uint32_t size);
	void uploadUBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void *data);
	// Is not needed as mapped pointer is used for SSBOs
	//void uploadSSBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void *data);
	// TODO: check with vulkan/dx12 to see if unmap is needed there. Add it to renderer?
	void* getMappedSSBOData(uint16_t type, uint64_t maxSize);
	void resizeSSBO(uint16_t type, uint32_t newSize);
	void unmapSSBO(uint16_t type);
	void BindSSBO(uint32_t binding, uint16_t type);

	void waitIdle() override;

	void createObjectBuffer(Shape &shape, uint32_t index_pointer_size, uint32_t normal_pointer_size, float* normals, uint32_t* index_array, std::vector<float> objDataVector) override;

	void clear() override;
	void clear(GLuint framebufferID);
	
	void beginFrame() override;
	void endFrame() override;
	void render() override;
	void renderBatch(int16_t shapeType, uint32_t ib_size, uint32_t amount, uint32_t baseInstance);
	void drawElements(uint32_t ib_size); // temporary to accelerate integration

};
