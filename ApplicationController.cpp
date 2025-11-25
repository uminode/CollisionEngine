#include "ApplicationController.h"
#include <iostream>


ApplicationController::ApplicationController() {
	window = nullptr;
	renderer = nullptr;
	camera = new CameraController();
	shapeArray = new DynamicShapeArray();
	inputController = new InputController(camera, shapeArray);
}
ApplicationController::~ApplicationController() {
	delete camera;
	delete shapeArray;
	delete inputController;
	delete renderer;
}

int ApplicationController::start() {
	
	uint32_t one = 1;
	uint32_t zero = 0;
	renderer = new OpenGLRenderer();
	if (renderer->init(1000, 1000) != 1) return -1; // TODO: Update to error codes
	window = renderer->getWindow();
	if (!window) return -1;
	shapeArray->setRenderer(renderer);
	
	glm::mat4 Projection = glm::perspective(glm::radians(40.0f), 1.0f, 0.1f, 1000.0f);

	// Model Matrix
	glm::mat4 cubeModel = glm::mat4{ 1.f };
	glm::mat4 cubeNormalModel = glm::mat4{ 1.f };
	glm::mat4 sphereModel = glm::mat4{ 1.f };
	glm::mat4 sphereNormalModel = glm::mat4{ 1.f };


	//ModelViewProjection Matrix
	glm::mat4 MVP;// Projection * View * Model;

	shapeArray->InitFactoryPrototypes();
	renderer->createUBO(0, MODEL_MATRIX, sizeof(objMatrices));
	renderer->createUBO(1, OBJ_COLOR, sizeof(colorData));
	renderer->createUBO(2, CAM_LIGHT_POSITIONS, sizeof(camLightPositions));
	renderer->createUBO(3, IS_TEXTURE, 1 * sizeof(uint32_t));

	// Create 50 shape buffers to minimize resizing during runtime
	renderer->createSSBO(0, CUBE_MATRICES, 500 * sizeof(objMatrices));  
	renderer->createSSBO(0, SPHERE_MATRICES, 500 * sizeof(objMatrices));  
	renderer->createSSBO(0, CYLINDER_MATRICES, 500 * sizeof(objMatrices));
	renderer->createSSBO(0, RING_MATRICES, 500 * sizeof(objMatrices)); 

	renderer->createSSBO(1, CUBE_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, SPHERE_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, CYLINDER_COLORS, sizeof(glm::vec4));
	renderer->createSSBO(1, RING_COLORS, sizeof(glm::vec4));

	//Create the first 2 shapes
	shapeArray->CreateShape(0.0f, 0.0f, 0.0f, 100.0f, T_CUBE);
	cubeModel = shapeArray->getModel(0);
	cubeNormalModel = shapeArray->getNormalModel(0);
	uint8_t cubeIndex = 0;
	shapeArray->SetRandomColor(0, 0.5f);//give random color to cube
	float* cubeColor = shapeArray->GetColor(0);
	glm::vec4 cubeColorVec = glm::vec4(cubeColor[0], cubeColor[1], cubeColor[2], cubeColor[3]);
	shapeArray->CreateShape(35.0f, 35.0f, 35.0f, 30.0f, T_SPHERE);
	uint8_t sphereIndex = 1;
	shapeArray->SetColor(1, 1.0f, 1.0f, 1.0f, 1.0f);
	float* sphereColor = shapeArray->GetColor(1);
	glm::vec4 sphereColorVec = glm::vec4(sphereColor[0], sphereColor[1], sphereColor[2], sphereColor[3]);
	renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, sphereColor);

	shapeArray->CreateRandomShapes(1000);
	void* ssboPtr;

	//Initialize Shader
	//renderer->initShader("Shader.shader");
	//renderer->initShader("obj_shader.vert", "obj_shader.frag");
	//renderer->initShader("obj_shader_vs.spv", "obj_shader_fs.spv");
	renderer->initShader("obj_shader.slang");
	renderer->initShader("obj_tex_shader.slang");
	renderer->initShader("batch_shader.slang"); // slang doesn't formally support OpenGL yet
	//renderer->initShader("batch_shader.vert", "batch_shader.frag");


	uint32_t shapeArrSize;
	float x = 1.0f;
	float l = 1.0f;
	glm::vec3 lightPos{ 150.f, x, 150.f };
	renderer->loadTexture("textures/texture.jpg");
#ifdef _WIN32
	mciSendString("open \"Elevator Music.mp3\" type mpegvideo alias mp3", NULL, 0, NULL);
	mciSendString("play mp3 repeat", NULL, 0, NULL);
#endif

	while (inputController->parseInputs(window) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
#ifdef _WIN32
		if (soundsEnabled)
			mciSendString("resume mp3 ", NULL, 0, NULL);
		else
			mciSendString("pause mp3 ", NULL, 0, NULL);
#endif
		shapeArrSize = shapeArray->getSize();
		renderer->beginFrame();

		x += l;
		lightPos.y = x;
		if (x >= 150.0f || x <= 0.0f) {
			l = (-1.0f) * l;
		}
		sphereModel = shapeArray->getModel(1);
		renderer->BindShader(TEXTURE_SHADER);
		renderer->uploadUBOData(2, CAM_LIGHT_POSITIONS, sizeof(glm::vec3), 0, &camera->getPosition());
		renderer->uploadUBOData(2, CAM_LIGHT_POSITIONS, sizeof(glm::vec3), sizeof(glm::vec4), &lightPos[0]);

		shapeArray->MoveAll(camera->getView(), Projection);
		//shapeArray->BindShape(sphereIndex);
		renderer->BindShape(T_SPHERE);
		renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, &sphereColorVec[0]);
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), sizeof(glm::mat4), &sphereModel[0]); // swap with glm::valueptr
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat3), 2 * sizeof(glm::mat4), &shapeArray->getNormalModel(1)[0]);
		MVP = Projection * camera->getView() * sphereModel;
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), 0, &MVP[0]);
		renderer->drawElements(shapeArray->GetIndexPointerSize(T_SPHERE)); 
		renderer->unbindShader();

		shapeArray->MoveAll(); // This can be calculated using one really complicated compute shader :/
		ssboPtr = renderer->getMappedSSBOData(CUBE_MATRICES, shapeArray->getShapeTypeArraySize(T_CUBE) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_CUBE, CUBE_MATRICES, ssboPtr);
		renderer->unmapSSBO(CUBE_MATRICES);
		ssboPtr = renderer->getMappedSSBOData(SPHERE_MATRICES, shapeArray->getShapeTypeArraySize(T_SPHERE) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_SPHERE, SPHERE_MATRICES, ssboPtr);
		renderer->unmapSSBO(SPHERE_MATRICES);
		ssboPtr = renderer->getMappedSSBOData(CYLINDER_MATRICES, shapeArray->getShapeTypeArraySize(T_CYLINDER) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_CYLINDER, CYLINDER_MATRICES, ssboPtr);
		renderer->unmapSSBO(CYLINDER_MATRICES);
		ssboPtr = renderer->getMappedSSBOData(RING_MATRICES, shapeArray->getShapeTypeArraySize(T_RING) * sizeof(objMatrices));
		shapeArray->uploadMatricesToPtr(T_RING, RING_MATRICES, ssboPtr);
		renderer->unmapSSBO(RING_MATRICES);

		// TODO: only upload the new colours
		ssboPtr = renderer->getMappedSSBOData(CUBE_COLORS, shapeArray->getShapeTypeArraySize(T_CUBE) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_CUBE, CUBE_COLORS, ssboPtr);
		renderer->unmapSSBO(CUBE_COLORS);
		ssboPtr = renderer->getMappedSSBOData(SPHERE_COLORS, shapeArray->getShapeTypeArraySize(T_SPHERE) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_SPHERE, SPHERE_COLORS, ssboPtr);
		renderer->unmapSSBO(SPHERE_COLORS);
		ssboPtr = renderer->getMappedSSBOData(CYLINDER_COLORS, shapeArray->getShapeTypeArraySize(T_CYLINDER) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_CYLINDER, CYLINDER_COLORS, ssboPtr);
		renderer->unmapSSBO(CYLINDER_COLORS);
		ssboPtr = renderer->getMappedSSBOData(RING_COLORS, shapeArray->getShapeTypeArraySize(T_RING) * sizeof(glm::vec4));
		shapeArray->uploadColorsToPtr(T_RING, RING_COLORS, ssboPtr);
		renderer->unmapSSBO(RING_COLORS);

		renderer->BindShader(BATCH_SHADER);
		renderer->BindShape(T_CUBE);
		renderer->BindSSBO(0, CUBE_MATRICES);
		renderer->BindSSBO(1, CUBE_COLORS);
		renderer->renderBatch(T_CUBE, shapeArray->GetIndexPointerSize(T_CUBE), shapeArray->getShapeTypeArraySize(T_CUBE)-1, 1); // skip first cube as it's drawn separately

		renderer->BindShape(T_SPHERE);
		renderer->BindSSBO(0, SPHERE_MATRICES);
		renderer->BindSSBO(1, SPHERE_COLORS);
		renderer->renderBatch(T_SPHERE, shapeArray->GetIndexPointerSize(T_SPHERE), shapeArray->getShapeTypeArraySize(T_SPHERE) - 1, 1); // skip first sphere as it's drawn separately

		renderer->BindShape(T_CYLINDER);
		renderer->BindSSBO(0, CYLINDER_MATRICES);
		renderer->BindSSBO(1, CYLINDER_COLORS);
		renderer->renderBatch(T_CYLINDER, shapeArray->GetIndexPointerSize(T_CYLINDER), shapeArray->getShapeTypeArraySize(T_CYLINDER), 0);

		renderer->BindShape(T_RING);
		renderer->BindSSBO(0, RING_MATRICES);
		renderer->BindSSBO(1, RING_COLORS);
		renderer->renderBatch(T_RING, shapeArray->GetIndexPointerSize(T_RING), shapeArray->getShapeTypeArraySize(T_RING), 0);
		renderer->unbindShader();
		
		renderer->BindShader(SIMPLE_SHADER);
		renderer->BindShape(T_CUBE);
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), sizeof(glm::mat4), &cubeModel[0]); // swap with glm::valueptr
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat3), 2 * sizeof(glm::mat4), &cubeNormalModel[0]);
		MVP = Projection * camera->getView() * cubeModel;
		renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), 0, &MVP[0]);
		renderer->uploadUBOData(1, OBJ_COLOR, sizeof(glm::vec4), 0, &cubeColorVec[0]);
		renderer->drawElements(shapeArray->GetIndexPointerSize(T_CUBE));
		// Example:
		// moveShader.bind()
		// glDispatchCompute(shapeArrSize, 1, 1);
		//for (int i = shapeArrSize - 1; i >= 2; i--) {
			//float* color = shapeArray->GetColor(i);
			//ib_size = shapeArray->GetIndexPointerSize(i);
			//shapeArray->BindShape(i);
			//if (i > 1) {
				//shapeArray->Move(i); // Change to shapeArray.MoveAll
			//}
			//Model = shapeArray->getModel(i);
			//normalModel = shapeArray->getNormalModel(i);
			//renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), sizeof(glm::mat4), &Model[0]); // swap with glm::valueptr
			//renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat3), 2 * sizeof(glm::mat4), &normalModel[0]);
			//MVP = Projection * camera->getView() * Model;
			//renderer->uploadUBOData(0, MODEL_MATRIX, sizeof(glm::mat4), 0, &MVP[0]);

			// update this to SSBOs
			//renderer->uploadUBOData(1, LIGHT_DATA, sizeof(glm::vec4), sizeof(glm::vec4), &color[0]);

		//	if (i == 1 && tex) {
		//		renderer->uploadUBOData(3, IS_TEXTURE, sizeof(uint32_t), 0, &zero);
		//	}
		//	else {
		//		renderer->uploadUBOData(3, IS_TEXTURE, sizeof(uint32_t), 0, &one);
		//	}
			//renderer->drawElements(ib_size);
		//}

		renderer->endFrame();
	}
	return 0;
}
