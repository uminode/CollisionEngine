#pragma once
#include "InputController.h"
#include "GLSLShader.h"
#include "Renderer.h"
#include "OpenGLRenderer.h"
#include "D3D12Renderer.h"
#include "ErrorCodes.h"

#ifdef _WIN32
#include <Windows.h>
#endif

class ApplicationController {
private:
	GLFWwindow* window;
	CameraController* camera;
	DynamicShapeArray* shapeArray;
	InputController* inputController;
	//OpenGLRenderer* renderer; // TODO: change this to Renderer* when other renderers are implemented
	D3D12Renderer* renderer;
public:
	ApplicationController();
	~ApplicationController();
	int start();
};