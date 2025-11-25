#pragma once
#include "opengl.h"
#include "OpenGLRenderer.h"
#include "Shape.h"
#include <cstdlib>
#include <chrono>
#include <random>
#include <iostream>
#include <vector>
#include <array>

#define PI 3.14159265f
#define SQRT_2 1.41421356237f

#define CIRCLE_VERTEX_NUM (CIRCLE_TRIANGLE_NUM+2)

#define SPHERE_SECTOR_NUM 36
#define SPHERE_STACK_NUM 18
#define CIRCLE_TRIANGLE_NUM 34



class ShapeFactory {
private:
	std::vector<Shape> Prototypes;
	OpenGLRenderer* renderer = nullptr; // Deprecated: moved to renderer class
	
	//Normals
/*
	Indices for cube triangle points have been numbered in the following way on the 2 faces back and front(+4)
	1 - 3 5 - 7
	| X | | X |
	0 - 2 4 - 6
	back: 0123, front: 4567, left: 0145, right: 2367, bottom: 4062, top: 5173
	The only easy enough to do by hand
*/
	std::array<float, 72> cube_normals; // to be used instead of C-style array
	std::array<float, 2109> sphere_normals; // to be used instead of C-style array
	std::array<float, 216> cylinder_normals; // to be used instead of C-style array
	std::array<float, 8 * (CIRCLE_VERTEX_NUM - 1) * 3> ring_normals; // to be used instead of C-style array

	//index arrays
	std::array<uint32_t, 36> cube_indices;
	std::array<uint32_t, CIRCLE_TRIANGLE_NUM * 3 * 4> cylinder_indices;
	std::array<uint32_t, 2 * 3 * (SPHERE_STACK_NUM - 1) * SPHERE_SECTOR_NUM> sphere_indices;
	std::array<uint32_t, 2 * 8 * CIRCLE_TRIANGLE_NUM * 3> ring_indices;

	//flags
	bool firstCube = true;
	bool firstCylinder = true;
	bool firstRing = true;
	bool firstSphere = true;

	unsigned int* GetIndexPointer(int shapeType);

	void InitSphereIndices();
	void InitCylinderIndices();
	void AddCircleIndices(std::array<uint32_t, 3 * 4 * CIRCLE_TRIANGLE_NUM> &indices, int index, int offset = 0);

	Shape& CreateShapeObject(float* element, int elementSize, int shapeType, float x0, float y0, float z0, float d);
	
	//functions that create shapes
	float* CreateCircle(float x, float y, float z, float radius);
	Shape& CreateCube(float x0, float y0, float z0, float size);
	Shape& CreateSphere(float x0, float y0, float z0, float radius);
	Shape& CreateCylinder(float x, float y, float z, float radius, float height);
	Shape& CreateRing(float x0, float y0, float z0, float r1, float r2);

	int RandomInt(int min, int max); // DEBUG: Move to another class
	float RandomFloat(float min, float max); // and this

public:
	ShapeFactory();
	void setRenderer(OpenGLRenderer* rend);
	void InitPrototypes();

	uint32_t GetIndexPointerSize(uint32_t shapeType);
	int32_t GetNormalPointerSize(int32_t shapeType);
	float* GetNormals(int shapeType);

	void BindShape(const Shape& shape); // Move to Renderer Class

	Shape& CreateRandomShape(float x = 0.f, float y = 0.f, float z = 0.f);
	Shape& CreateShape(float x, float y, float z, int size, int ShapeType);

	// Color handlers, DEBUG: Move to another class
	void SetRandomColor(Shape& shape, float alpha_value = 1.0f);
	float* GetColor(Shape& shape);
	void SetColor(Shape& shape, float r_value, float g_value, float b_value, float alpha_value);
	
};