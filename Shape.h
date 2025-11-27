#pragma once
#include <glm/glm.hpp>

enum ShapeType {
	T_CUBE = 0,
	T_SPHERE,
	T_CYLINDER,
	T_RING
};

struct objMatrices {
	glm::mat4 mvp{ 1.f }; // 4x4 floats x4 bytes = 64 bytes
	glm::mat4 model{ 1.f }; // 128 bytes
	glm::mat3x4 normalModel{ 1.f }; // 3x4floats x 4 bytes = 48 + 128 = 176 bytes
};

struct Shape {
	int size = 0;
	int shapeType = -1;
	float speed[3] = { 0.0f ,0.0f ,0.0f };
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float center[3] = { 0.f, 0.f, 0.f };
	glm::vec3 scale = { 1.f, 1.f, 1.f };
	objMatrices matrices;
	float d = 0.f;
	float d2 = 0.f;
};
