#include "ApplicationController.h"
#include <iostream>
#ifdef _DEBUG
#include "OpenGLProfiler.h"
#endif
#define GLM_FORCE_INLINE
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_AVX2
#define GLM_FORCE_ALIGNED_GENTYPES
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

ApplicationController::ApplicationController() {
	window = nullptr;
	renderer = nullptr;
	camera = new CameraController();
	shapeArray = new DynamicShapeArray();
	inputController = new InputController(camera, shapeArray);
	//renderer = new OpenGLRenderer();
	renderer = new D3D12Renderer();
}
ApplicationController::~ApplicationController() {
	delete camera;
	delete shapeArray;
	delete inputController;
	delete renderer;
}

int ApplicationController::start() {
	
	renderer->init(1000, 1000);
	window = renderer->getWindow();
	if (!window) return APP_GENERIC_ERROR;
	shapeArray->setRenderer(renderer);
	
	glm::mat4 Projection = glm::perspective(glm::radians(40.0f), 1.0f, 0.1f, 1000.0f);

	// Model Matrix for objects that don't move via collisions
	glm::mat4 cubeModel = glm::mat4{ 1.f };
	glm::mat4 cubeNormalModel = glm::mat4{ 1.f };
	glm::mat4 sphereModel = glm::mat4{ 1.f };
	glm::mat4 sphereNormalModel = glm::mat4{ 1.f };


	//ModelViewProjection Matrix
	glm::mat4 MVP;// Projection * View * Model;

	// Prototype objects are created so that new objects can be derived from them
	shapeArray->InitFactoryPrototypes();

	// Quick uploading buffer for single object data
	renderer->createUBO(0, MODEL_MATRIX, sizeof(objMatrices));
	renderer->createUBO(1, OBJ_COLOR, sizeof(colorData));
	renderer->createUBO(2, CAM_LIGHT_POSITIONS, sizeof(camLightPositions));
	renderer->createUBO(3, IS_TEXTURE, 1 * sizeof(uint32_t));

	// Create 500 shape buffers to minimize resizing during runtime
	// This is done for for batch rendering
	renderer->createSSBO(0, CUBE_MATRICES, 500 * sizeof(objMatrices));  
	renderer->createSSBO(0, SPHERE_MATRICES, 500 * sizeof(objMatrices));  
	renderer->createSSBO(0, CYLINDER_MATRICES, 500 * sizeof(objMatrices));
	renderer->createSSBO(0, RING_MATRICES, 500 * sizeof(objMatrices)); 

	renderer->createSSBO(1, CUBE_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, SPHERE_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, CYLINDER_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, RING_COLORS, sizeof(glm::vec4));
	void* ssboPtr;

	// Create cube enclosure and sphere in the middle
	shapeArray->CreateShape(0.0f, 0.0f, 0.0f, 100.0f, T_CUBE);
	cubeModel = shapeArray->getModel(0);
	cubeNormalModel = shapeArray->getNormalModel(0);
	//uint8_t cubeIndex = 0;
	shapeArray->SetRandomColor(0, 0.5f);//give random color to cube
	float* cubeColor = shapeArray->GetColor(0);
	glm::vec4 cubeColorVec = glm::vec4(cubeColor[0], cubeColor[1], cubeColor[2], cubeColor[3]);
	shapeArray->CreateShape(35.0f, 35.0f, 35.0f, 30.0f, T_SPHERE);
	//uint8_t sphereIndex = 1;
	shapeArray->SetColor(1, 1.0f, 1.0f, 1.0f, 1.0f);
	float* sphereColor = shapeArray->GetColor(1);
	glm::vec4 sphereColorVec = glm::vec4(sphereColor[0], sphereColor[1], sphereColor[2], sphereColor[3]);
	renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, sphereColor);

	// Stress test
	shapeArray->CreateRandomShapes(1000);

	// Shader initialization examples
	// GLSL combined shader:
	//renderer->initShader("Shader.shader");
	// GLSL split shaders:
	//renderer->initShader("obj_shader.vert", "obj_shader.frag");
	//renderer->initShader("batch_shader.vert", "batch_shader.frag");
	// SpirV binary shader:
	//renderer->initShader("obj_shader_vs.spv", "obj_shader_fs.spv");
	// Slang shaders:
	renderer->initShader("shaders/obj_shader.slang");
	renderer->initShader("shaders/obj_tex_shader.slang");
	renderer->initShader("shaders/batch_shader.slang"); 
	
	renderer->finalizeInit();

	// Helpers to profile the code
#ifdef _DEBUG
	// Not renderer agnostic.`
	//OpenGLProfiler bufferUpdateProfiler("Buffer Updates");
	//OpenGLProfiler cubeDrawProfiler("Draw Cubes");
	//OpenGLProfiler sphereDrawProfiler("Draw Spheres");
	//OpenGLProfiler cylinderDrawProfiler("Draw Cylinders");
	//OpenGLProfiler ringDrawProfiler("Draw Rings");
#endif
	uint32_t frameCount = 0;
	uint32_t shapeArrSize;
	float x = 1.0f;
	float l = 1.0f;
	glm::vec3 lightPos{ 150.f, x, 150.f };
	renderer->loadTexture("textures/texture.jpg");
#ifdef _WIN32
	mciSendString("open \"Elevator Music.mp3\" type mpegvideo alias mp3", NULL, 0, NULL);
	mciSendString("play mp3 repeat", NULL, 0, NULL);
#endif

	// do trick with lastFrameTime so that physics don't go bonkers at start
	float lastFrameTime = static_cast<float>(glfwGetTime()), currentFrameTime = 0.f, deltaTime = .016f;
	while (inputController->parseInputs(window, deltaTime) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
#ifdef _WIN32
		if (soundsEnabled)
			mciSendString("resume mp3 ", NULL, 0, NULL);
		else
			mciSendString("pause mp3 ", NULL, 0, NULL);
#endif
		currentFrameTime = static_cast<float>(glfwGetTime());
		deltaTime = currentFrameTime - lastFrameTime;
		if (deltaTime > 0.1f) {
			deltaTime = 0.1f;  
		}
		lastFrameTime = currentFrameTime;
		shapeArrSize = shapeArray->getSize();
		renderer->beginFrame();

		x += l * 50.f * deltaTime;
		lightPos.y = x;
		if (x >= 150.0f || x <= 0.0f) {
			l = (-1.0f) * l;
		}

		shapeArray->UpdatePhysics(deltaTime);
		shapeArray->UpdateMatrices(camera->getView(), Projection);

		// Sphere drawing process: Use shader with texture support -> upload camera position and light position to VRAM -> 
		// Bind shape -> upload color and model matrix -> draw call -> unbind shader
		sphereModel = shapeArray->getModel(1);
		renderer->BindShader(TEXTURE_SHADER);
		// TODO: remove uploadUBOData, use persistent mapping on UBOs as well.
		renderer->uploadUBOData(2, CAM_LIGHT_POSITIONS, sizeof(glm::vec3), 0, &camera->getPosition());
		renderer->uploadUBOData(2, CAM_LIGHT_POSITIONS, sizeof(glm::vec3), sizeof(glm::vec4), &lightPos[0]);

		renderer->BindShape(T_SPHERE);
		renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, &sphereColorVec[0]);
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), sizeof(glm::mat4), &sphereModel[0]); // swap with glm::valueptr
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat3), 2 * sizeof(glm::mat4), &shapeArray->getNormalModel(1)[0]);
		MVP = Projection * camera->getView() * sphereModel;
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), 0, &MVP[0]);
		renderer->drawElements(shapeArray->GetIndexPointerSize(T_SPHERE)); 
		renderer->unbindShader();

#ifdef _DEBUG
		//bufferUpdateProfiler.begin();
#endif
		// This is the core of the batch rendering process
		// Uploads for all objects of each shape type their matrices and colors to the mapped SSBO pointers
		ssboPtr = renderer->getMappedSSBOData(CUBE_MATRICES, shapeArray->getShapeTypeArraySize(T_CUBE) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_CUBE, CUBE_MATRICES, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(SPHERE_MATRICES, shapeArray->getShapeTypeArraySize(T_SPHERE) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_SPHERE, SPHERE_MATRICES, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(CYLINDER_MATRICES, shapeArray->getShapeTypeArraySize(T_CYLINDER) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_CYLINDER, CYLINDER_MATRICES, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(RING_MATRICES, shapeArray->getShapeTypeArraySize(T_RING) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_RING, RING_MATRICES, ssboPtr);

		// TODO: only upload the new colours (super micro optimization since whole upload takes 0.000032ms)
		ssboPtr = renderer->getMappedSSBOData(CUBE_COLORS, shapeArray->getShapeTypeArraySize(T_CUBE) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_CUBE, CUBE_COLORS, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(SPHERE_COLORS, shapeArray->getShapeTypeArraySize(T_SPHERE) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_SPHERE, SPHERE_COLORS, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(CYLINDER_COLORS, shapeArray->getShapeTypeArraySize(T_CYLINDER) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_CYLINDER, CYLINDER_COLORS, ssboPtr);
		ssboPtr = renderer->getMappedSSBOData(RING_COLORS, shapeArray->getShapeTypeArraySize(T_RING) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_RING, RING_COLORS, ssboPtr);
#ifdef _DEBUG
		//bufferUpdateProfiler.end();
#endif

		// After upload draw all objects in batches per shape type
		renderer->BindShader(BATCH_SHADER);
#ifdef _DEBUG
		//cubeDrawProfiler.begin();
#endif
		renderer->BindShape(T_CUBE);
		renderer->BindSSBO(0, CUBE_MATRICES);
		renderer->BindSSBO(1, CUBE_COLORS);
		renderer->renderBatch(T_CUBE, shapeArray->GetIndexPointerSize(T_CUBE), shapeArray->getShapeTypeArraySize(T_CUBE)-1, 1); // skip first cube as it's drawn separately
		//renderer->renderBatch(T_CUBE, shapeArray->GetIndexPointerSize(T_CUBE), 2, 1); // skip first cube as it's drawn separately
#ifdef _DEBUG
		//cubeDrawProfiler.end();
		//sphereDrawProfiler.begin();
#endif

		renderer->BindShape(T_SPHERE);
		renderer->BindSSBO(0, SPHERE_MATRICES);
		renderer->BindSSBO(1, SPHERE_COLORS);
		renderer->renderBatch(T_SPHERE, shapeArray->GetIndexPointerSize(T_SPHERE), shapeArray->getShapeTypeArraySize(T_SPHERE) - 1, 1); // skip first sphere as it's drawn separately
#ifdef _DEBUG
		//sphereDrawProfiler.end();
		//cylinderDrawProfiler.begin();
#endif

		renderer->BindShape(T_CYLINDER);
		renderer->BindSSBO(0, CYLINDER_MATRICES);
		renderer->BindSSBO(1, CYLINDER_COLORS);
		renderer->renderBatch(T_CYLINDER, shapeArray->GetIndexPointerSize(T_CYLINDER), shapeArray->getShapeTypeArraySize(T_CYLINDER), 0);

#ifdef _DEBUG
		//cylinderDrawProfiler.end();
		//ringDrawProfiler.begin();
#endif
		renderer->BindShape(T_RING);
		renderer->BindSSBO(0, RING_MATRICES);
		renderer->BindSSBO(1, RING_COLORS);
		renderer->renderBatch(T_RING, shapeArray->GetIndexPointerSize(T_RING), shapeArray->getShapeTypeArraySize(T_RING), 0);
		renderer->unbindShader();
#ifdef _DEBUG
		//ringDrawProfiler.end();
#endif
		
		// Cube drawing follows sphere without textures.
		renderer->BindShader(SIMPLE_SHADER);
		renderer->BindShape(T_CUBE);
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), sizeof(glm::mat4), &cubeModel[0]); // swap with glm::valueptr
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat3), 2 * sizeof(glm::mat4), &cubeNormalModel[0]);
		MVP = Projection * camera->getView() * cubeModel;
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), 0, &MVP[0]);
		renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, &cubeColorVec[0]);
		//renderer->drawElements(shapeArray->GetIndexPointerSize(T_CUBE));

#ifdef _DEBUG
		// Also that's not best practice
		if (++frameCount % 1000 == 0) {
			//bufferUpdateProfiler.printResult();
			//cubeDrawProfiler.printResult();
			//sphereDrawProfiler.printResult();
			//cylinderDrawProfiler.printResult();
			//ringDrawProfiler.printResult();
		}
#endif
		renderer->endFrame();
	}
	return APP_SUCCESS;
}
