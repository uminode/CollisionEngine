#pragma once
#include "ShapeFactory.h"

#define GLOBAL_SPEED 60
#define MAX_SPEEDUP 100

extern bool soundsEnabled;

class DynamicShapeArray
{
public:
	DynamicShapeArray();
	~DynamicShapeArray();

	void InitFactoryPrototypes();
	//creates Shapes and adds them to the Array
	void CreateRandomShape();
	void CreateRandomShapes(int amount);
	void CreateShape(float x, float y, float z, int size, int ShapeType);
	
	//Binds VAO and ib of the shape at the index
	void BindShape(int index);

	//movement
	void Move(int index);
	void Move(int index, glm::mat4 view, glm::mat4& projection);
	void MoveAll();
	void MoveAll(glm::mat4 view, glm::mat4& projection);
	void MoveSphere(int index, glm::vec3 speed);
	void SpeedUP(bool up);

	//Getters
	inline uint32_t getSize() { return size; };
	inline uint64_t getShapeTypeArraySize(int16_t shape) { return shapeTypeArray[shape].size(); };
	inline glm::mat4 getModel(int index) { return shapeArray[index]->matrices.model; };
	inline glm::mat4 getNormalModel(int index) { return shapeArray[index]->matrices.normalModel; };
	float * GetColor(int index);//Returns the color of the shape to pass into the shader
	uint32_t GetIndexPointerSize(uint32_t shapeType);//Returns the size of the ib to use when drawing
	void uploadMatricesToPtr(int shapeType, uint16_t type, void* ptr); // uploads all matrices of a shape type to a mapped ssbo pointer
	void uploadColorsToPtr(int shapeType, uint16_t type, void* ptr); // uploads all colors of a shape type to a mapped ssbo pointer

	//Setters
	void SetColor(int index, float r_value, float g_value, float b_value, float alpha_value = 1.0f);
	void SetRandomColor(int index, float alpha_value = 1.0f);
	void setRenderer(OpenGLRenderer* renderer);


private:
	std::vector<Shape *> shapeArray;
	//std::array<std::vector<std::reference_wrapper<Shape>>, 4> shapeTypeArray; // for batch rendering 
	std::array<std::vector<Shape*>, 4> shapeTypeArray; // for batch rendering 
	ShapeFactory* shapeFactory;
	uint32_t size;
	uint32_t capacity;
	
	//collision handling
	void CheckCollision(int index);
	void Collide(int index1, int index2);
	
	//assisting function
	float * GetNormals(int shapeType);
	void AddShape(Shape *shape);
};