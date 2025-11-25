#pragma once
#include "Renderer.h"
#include "GLSLShader.h"
#include "Shape.h"
#include "opengl.h"
#include <vector>
#include <map>
#include <string>
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
	//std::vector<GLuint> framebuffers;
public:
	OpenGLRenderer();
	~OpenGLRenderer();

	int16_t init(uint16_t windowWidth, uint16_t windowHeight) override;

	inline GLFWwindow* getWindow() { return window; };
	void setShader(GLSLShader& shader, int shaderType);
	void initShader(const std::string& path);
	void initShader(const std::string& vertPath, const std::string& fragPath);
	void loadTexture(const std::string &fileName) override;

	void BindShader(int shaderType = 0);
	void unbindShader();
	void BindShape(int shapeType);
	void setViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override;

	void createUBO(uint32_t binding, uint16_t type, uint32_t size);
	void createSSBO(uint32_t binding, uint16_t type, uint32_t size);
	void uploadUBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void *data);
	// Is not needed as mapped pointer is used for SSBOs
	//void uploadSSBOData(uint32_t binding, uint16_t type, uint32_t size, uint32_t offset, void *data);
	// TODO: check with vulkan/dx12 to see if unmap is needed there. Add it to renderer?
	void* getMappedSSBOData(uint16_t type, uint32_t maxSize);
	void resizeSSBO(uint16_t type, uint32_t newSize);
	void unmapSSBO(uint16_t type);
	void BindSSBO(uint32_t binding, uint16_t type);

	void waitIdle() override;

	void createObjectBuffer(Shape &shape, int32_t index_pointer_size, int32_t normal_pointer_size, float* normals, uint32_t* index_array, std::vector<float> objDataVector) override;

	void clear() override;
	void clear(GLuint framebufferID);
	
	void beginFrame() override;
	void endFrame() override;
	void render() override;
	void renderBatch(int16_t shapeType, uint32_t ib_size, uint32_t amount, uint32_t baseInstance);
	void drawElements(uint32_t ib_size); // temporary to accelerate integration

};
enum ShaderTypes {
	SIMPLE_SHADER = 0,
	TEXTURE_SHADER,
	BATCH_SHADER
};

