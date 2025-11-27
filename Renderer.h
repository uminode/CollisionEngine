#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "Shape.h"
#include <GLFW/glfw3.h>

/// Abstract Renderer Interface
class Renderer
{
private:
	GLFWwindow *window = nullptr;
public:
    virtual ~Renderer() = default;

    virtual void init(uint16_t windowWidth, uint16_t windowHeight) = 0;
	virtual inline GLFWwindow* getWindow() = 0;

    virtual void beginFrame() = 0;   // start recording/clearing
    virtual void clear() = 0;        // clear default framebuffer
    virtual void render() = 0;       // issue draw commands
    virtual void endFrame() = 0;     // present/swap buffers

	virtual void setViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;

    // extend with loadShader(), createPipeline(), etc.
    virtual void loadTexture(const std::string& filePath) = 0;

	virtual void createObjectBuffer(Shape &shape, int32_t index_pointer_size, int32_t normal_pointer_size, float* normals, uint32_t* index_array, std::vector<float> objDataVector) = 0;

    // Window resizing
	//virtual void resize(uint16_t newWidth, uint16_t newHeight) = 0; // optional for now

    virtual void waitIdle() = 0; // useful for Vulkan (gpu sync before cleanup)

    //virtual void setDebugName(uint32_t resourceID, const std::string& name) = 0;
};

// TODO: relocate these to a seperate file correlated to gpu storage
// These structs include stride bits for the std140 layout. We should really change to std430

struct PersistentBuffer {
	uint32_t bufferID;
	void* mappedPtr;
	uint64_t size;
};
struct colorData {
	glm::vec4 color;
};
struct camLightPositions {
    glm::vec4 camPos;
    glm::vec4 lightPos;
};

enum BufferUsage {
    MODEL_MATRIX,
	OBJ_COLOR,
    CAM_LIGHT_POSITIONS,
	IS_TEXTURE,
    CUBE_MATRICES,
    SPHERE_MATRICES,
    CYLINDER_MATRICES,
    RING_MATRICES,
    CUBE_COLORS,
    SPHERE_COLORS,
    CYLINDER_COLORS,
    RING_COLORS 
};

enum ShaderTypes {
    SIMPLE_SHADER = 0,
    TEXTURE_SHADER,
    BATCH_SHADER
};
