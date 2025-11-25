#include "ShapeFactory.h"

//Normals
/*
	Indices for cube triangle points have been numbered in the following way on the 2 faces back and front(+4)
	1 - 3 5 - 7
	| X | | X |
	0 - 2 4 - 6
	back: 0123, front: 4567, left: 0145, right: 2367, bottom: 4062, top: 5173
	The only easy enough to do by hand
*/
ShapeFactory::ShapeFactory():cube_indices{
	4, 6, 5,//front
	7, 5, 6,//front
	0, 1, 2,//back
	3, 2, 1,//back

	0+8, 4+8, 1+8,//left
	5+8, 1+8, 4+8,//left
	2+8, 3+8, 6+8,//right
	3+8, 7+8, 6+8,//right

	5+16, 7+16, 1+16,//top
	3+16, 1+16, 7+16,//top
	4+16, 0+16, 6+16,//bottom
	2+16, 6+16, 0+16 //bottom
} 
,cube_normals{
	0.f,0.f,-1.f,
	0.f,0.f,-1.f,
	0.f,0.f,-1.f,
	0.f,0.f,-1.f,
	0.f,0.f,1.f,
	0.f,0.f,1.f,
	0.f,0.f,1.f,
	0.f,0.f,1.f,

	-1.f,0.f,0.f,
	-1.f,0.f,0.f,
	1.f,0.f,0.f,
	1.f,0.f,0.f,
	-1.f,0.f,0.f,
	-1.f,0.f,0.f,
	1.f,0.f,0.f,
	1.f,0.f,0.f,

	0.f,-1.f,0.f,
	0.f,1.f,0.f,
	0.f,-1.f,0.f,
	0.f,1.f,0.f,
	0.f,-1.f,0.f,
	0.f,1.f,0.f,
	0.f,-1.f,0.f,
	0.f,1.f,0.f,

}
{
	InitSphereIndices();
	InitCylinderIndices();
}
void ShapeFactory::setRenderer(OpenGLRenderer* rend) {
	renderer = rend;
}


void ShapeFactory::InitPrototypes()
{
	Prototypes.push_back(CreateCube(0.f, 0.f, 0.f, 1.f));
	Prototypes.push_back(CreateSphere(0.f, 0.f, 0.f, 1.f));
	Prototypes.push_back(CreateCylinder(0.f, 0.f, 0.f, 1.f, 1.f));
	Prototypes.push_back(CreateRing(0.f, 0.f, 0.f, 1.0f, 0.3f));
}

void ShapeFactory::InitSphereIndices() {
	unsigned int k1, k2;

	for (int i = 0, n = 0; i < SPHERE_STACK_NUM; ++i)
	{
		k1 = i * (SPHERE_SECTOR_NUM + 1);     // beginning of current stack
		k2 = k1 + SPHERE_SECTOR_NUM + 1;      // beginning of next stack

		for (int j = 0; j < SPHERE_SECTOR_NUM; ++j, ++k1, ++k2)
		{
			// 2 triangles per sector excluding 1st and last stacks
			if (i != 0)
			{
				sphere_indices[n++] = k1;
				sphere_indices[n++] = k2;
				sphere_indices[n++] = k1 + 1;
			}

			if (i != (SPHERE_STACK_NUM - 1))
			{
				sphere_indices[n++] = k1 + 1;
				sphere_indices[n++] = k2;
				sphere_indices[n++] = k2 + 1;
			}
		}
	}
}

void ShapeFactory::InitCylinderIndices() {
	int offset = CIRCLE_TRIANGLE_NUM + 2;

	AddCircleIndices(cylinder_indices, 0);
	AddCircleIndices(cylinder_indices, CIRCLE_TRIANGLE_NUM * 3, offset);

	for (int i = 1, pos = CIRCLE_TRIANGLE_NUM * 3 * 2; i <= CIRCLE_TRIANGLE_NUM; i++) {
		cylinder_indices[pos++] = i;
		cylinder_indices[pos++] = offset + i + 1;
		cylinder_indices[pos++] = i + 1;
		cylinder_indices[pos++] = offset + i;
		cylinder_indices[pos++] = i;
		cylinder_indices[pos++] = offset + i + 1;
	}
}

void ShapeFactory::AddCircleIndices(std::array<uint32_t, 3 * 4 * CIRCLE_TRIANGLE_NUM> &indices, int index, int offset) {
	for (int i = 1; i <= CIRCLE_TRIANGLE_NUM; i++) {
		indices[index++] = offset + i;
		indices[index++] = offset;
		indices[index++] = offset + i + 1;
	}
}
int32_t ShapeFactory::GetNormalPointerSize(int32_t shapeType) {
	switch (shapeType)
	{
	case T_CUBE:
		return 24;
	case T_CYLINDER:
		return 216;
	case T_SPHERE:
		return 2109;
	case T_RING:
		return 8 * (CIRCLE_VERTEX_NUM - 1) * 3;
	}
	return 0;
}

/*
Buffer Binder
- binds shape's vao and ibo
- created mainly because it removes 2 - 3 Getters / Setters
- TODO: replace with renderer.bindShape(shape.vao_id, shape_ib_id); or just do a batch draw.
*/
void ShapeFactory::BindShape(const Shape& shape) {
	renderer->BindShape(shape.shapeType);
}

Shape& ShapeFactory::CreateRandomShape(float x, float y, float z) {
	int shapeType = RandomInt(0, 3);
	int shapeSize = RandomInt(1, 10);
	float r, g, b, vx, vy, vz;
	r = RandomFloat(0.0f, 1.0f);
	g = RandomFloat(0.0f, 1.0f);
	b = RandomFloat(0.0f, 1.0f);
	vx = RandomFloat(0.0f, 0.9f);
	vy = RandomFloat(0.0f, 0.9f);
	vz = RandomFloat(0.0f, 0.9f);

	Shape& newShape = CreateShape(x, y, z, shapeSize, shapeType);
	
	//std::cout << "Spawned new shape: " << shapeType << std::endl;
	SetColor(newShape, r, g, b, 1.0f);
	newShape.speed[0] = vx;
	newShape.speed[1] = vy;
	newShape.speed[2] = vz;
	return newShape;
}


/*
Shape Creator
-creates new shape to add to the Array
*/
Shape& ShapeFactory::CreateShape(float x, float y, float z, int shapeSize, int ShapeType) {
	switch (ShapeType)
	{
	case T_CUBE:
		return CreateCube(x, y, z, static_cast<float>(shapeSize));
	case T_SPHERE:
		return CreateSphere(x + shapeSize / 2.0f, y + shapeSize / 2.0f, z + shapeSize / 2.0f, shapeSize / 2.0f);
	case T_CYLINDER:
		return CreateCylinder(x + shapeSize / 2.0f, y, z + shapeSize / 2.0f, shapeSize / 2.0f, static_cast<float>(shapeSize));
	
    case T_RING:
        float r = RandomFloat(0.0f, 1.0f);
		float r1 = .5f* shapeSize;
		float r2 = r1/(float)RandomInt(3, 10);
		return CreateRing(r+5,2*r2+5, r1+5,r1,r2);
    }
	return CreateCube(x, y, z, static_cast<float>(shapeSize));
}

Shape& ShapeFactory::CreateRing(float x,float y, float z, float r1, float r2) {
	if (firstRing) {
		float* circle1 = CreateCircle(x, y + r2, z, abs(r1 - r2));
		float* circle2 = CreateCircle(x, y, z, abs(r1 - 2 * r2));
		float* circle3 = CreateCircle(x, y - r2, z, abs(r1 - r2));
		float* circle4 = CreateCircle(x, y, z, r1);

		float* circle1b = CreateCircle(x, y + r2 / 2, z, abs(r1 - 3 * r2 / 2));
		float* circle2b = CreateCircle(x, y - r2 / 2, z, abs(r1 - 3 * r2 / 2));

		float* circle3b = CreateCircle(x, y - SQRT_2 * r2 / 2, z, abs(r1 - r2 / 2));
		float* circle4b = CreateCircle(x, y + SQRT_2 * r2 / 2, z, abs(r1 - r2 / 2));

		int vertex_num = (CIRCLE_VERTEX_NUM - 1);
		int vertex_size = (CIRCLE_VERTEX_NUM - 1) * 3;
		float ringVertices[(CIRCLE_VERTEX_NUM - 1) * 8 * 3];

		for (int i = 3, n = 0; n < vertex_size; i += 3) {
			ringVertices[n] = circle1[i];
			ringVertices[n + 1] = circle1[i + 1];
			ringVertices[n + 2] = circle1[i + 2];

			ringVertices[n + vertex_size] = circle1b[i];
			ringVertices[n + vertex_size + 1] = circle1b[i + 1];
			ringVertices[n + vertex_size + 2] = circle1b[i + 2];

			ringVertices[n + 2 * vertex_size] = circle2[i];
			ringVertices[n + 2 * vertex_size + 1] = circle2[i + 1];
			ringVertices[n + 2 * vertex_size + 2] = circle2[i + 2];

			ringVertices[n + 3 * vertex_size] = circle2b[i];
			ringVertices[n + 3 * vertex_size + 1] = circle2b[i + 1];
			ringVertices[n + 3 * vertex_size + 2] = circle2b[i + 2];

			ringVertices[n + 4 * vertex_size] = circle3[i];
			ringVertices[n + 4 * vertex_size + 1] = circle3[i + 1];
			ringVertices[n + 4 * vertex_size + 2] = circle3[i + 2];

			ringVertices[n + 5 * vertex_size] = circle3b[i];
			ringVertices[n + 5 * vertex_size + 1] = circle3b[i + 1];
			ringVertices[n + 5 * vertex_size + 2] = circle3b[i + 2];

			ringVertices[n + 6 * vertex_size] = circle4[i];
			ringVertices[n + 6 * vertex_size + 1] = circle4[i + 1];
			ringVertices[n + 6 * vertex_size + 2] = circle4[i + 2];

			ringVertices[n + 7 * vertex_size] = circle4b[i];
			ringVertices[n + 7 * vertex_size + 1] = circle4b[i + 1];
			ringVertices[n + 7 * vertex_size + 2] = circle4b[i + 2];

			ring_normals[n] = ring_normals[n + 2] = 0.0f;
			ring_normals[n + 1] = 1.0f;

			ring_normals[n + vertex_size] = -cos(2 * PI * i / (34 * 3));
			ring_normals[n + vertex_size + 1] = SQRT_2 / 2;
			ring_normals[n + vertex_size + 2] = -sin(2 * PI * i / (34 * 3));

			ring_normals[n + 2 * vertex_size] = -cos(2 * PI * i / (34 * 3));
			ring_normals[n + 2 * vertex_size + 1] = 0.0f;
			ring_normals[n + 2 * vertex_size + 2] = -sin(2 * PI * i / (34 * 3));

			ring_normals[n + 3 * vertex_size] = -cos(2 * PI * i / (34 * 3));
			ring_normals[n + 3 * vertex_size + 1] = -SQRT_2 / 2;
			ring_normals[n + 3 * vertex_size + 2] = -sin(2 * PI * i / (34 * 3));

			ring_normals[n + 4 * vertex_size] = 0.0f;
			ring_normals[n + 4 * vertex_size + 1] = -1.0f;
			ring_normals[n + 4 * vertex_size + 2] = 0.0f;

			ring_normals[n + 5 * vertex_size] = cos(2 * PI * i / (34 * 3));
			ring_normals[n + 5 * vertex_size + 1] = -SQRT_2 / 2;
			ring_normals[n + 5 * vertex_size + 2] = sin(2 * PI * i / (34 * 3));

			ring_normals[n + 6 * vertex_size] = cos(2 * PI * i / (34 * 3));
			ring_normals[n + 6 * vertex_size + 1] = 0.0f;
			ring_normals[n + 6 * vertex_size + 2] = sin(2 * PI * i / (34 * 3));

			ring_normals[n + 7 * vertex_size] = cos(2 * PI * i / (34 * 3));
			ring_normals[n + 7 * vertex_size + 1] = SQRT_2 / 2;
			ring_normals[n + 7 * vertex_size + 2] = sin(2 * PI * i / (34 * 3));
			
			n += 3;
		}
		/*
			1/\8
			2||7
			3||6
			4\/5
		*/
		int sector_num = 8;
		for (int sector = 0, pos = 0; sector < sector_num; sector++) {
			for (int i = 0; i < CIRCLE_TRIANGLE_NUM; i++) {
				ring_indices[pos++] = i + (sector % sector_num) * vertex_num;
				ring_indices[pos++] = i + ((sector + 1) % sector_num) * vertex_num;
				ring_indices[pos++] = i + 1 + (sector % sector_num) * vertex_num;
				ring_indices[pos++] = i + ((sector + 1) % sector_num) * vertex_num;
				ring_indices[pos++] = i + 1 + ((sector + 1) % sector_num) * vertex_num;
				ring_indices[pos++] = i + 1 + (sector % sector_num) * vertex_num;
			}
		}
		
		firstRing = false;
		Shape& tempShape = CreateShapeObject(ringVertices, 8 * vertex_size, T_RING, x, y, z, r1);
		tempShape.d2 = r2;
		free(circle1);
		free(circle2);
		free(circle3);
		free(circle4);
		free(circle1b);
		free(circle2b);
		free(circle3b);
		free(circle4b);
		return tempShape;
	}
	glm::mat4 model{ 1.f };
	model = glm::translate(model, glm::vec3{ x, y, z });
	model = glm::scale(model, glm::vec3{ r1, 4*r2, r1 });
	Shape* tempShapePtr = new Shape{ Prototypes.at(T_RING) };
	Shape& tempShape = *tempShapePtr;
	tempShape.matrices.model = model;
	tempShape.center[0] = x;
	tempShape.center[1] = y;
	tempShape.center[2] = z;
	return tempShape;
}


// Helper function for cylinder and ring creation
float* ShapeFactory::CreateCircle(float x, float y, float z, float radius) {
	int num_of_sides = CIRCLE_TRIANGLE_NUM;
	int num_of_vertices = num_of_sides + 2;
	int n = 3 * num_of_vertices;
	float twicePi = 2.0f * PI;
	float * vertices = (float *) malloc(sizeof(float) * (CIRCLE_TRIANGLE_NUM+2)*3);
	if (vertices != nullptr) {
		vertices[0] = x;
		vertices[1] = y;
		vertices[2] = z;

		for (int i = 3,p=1; p < num_of_vertices;p++)
		{		
			float tempx, tempz;
			tempx = x + (radius * cos(p * twicePi / num_of_sides));
			tempz = z + (radius * sin(p * twicePi / num_of_sides));
			vertices[i] = tempx;
			i++;
			vertices[i++] = y;
			vertices[i] = tempz;
			i++;
		}

		return vertices;
	}
	return nullptr;
}

Shape& ShapeFactory::CreateCube(float x0, float y0, float z0, float size) {
	float x1 = x0 + size;
	float y1 = y0 + size;
	float z1 = z0 + size;
	if (firstCube) {
		float positions[] = {
			x0, y0, z0,//00 back(0)0
			x0, y1, z0,//01 back(0)1
			x1, y0, z0,//10 back(0)2
			x1, y1, z0,//11 back(0)3
			x0, y0, z1,//00 front(1)4
			x0, y1, z1,//01 front(1)5
			x1, y0, z1,//10 front(1)6
			x1, y1, z1,//11 front(1)7

			x0, y0, z0,//00 left(0)0
			x0, y1, z0,//01 left(0)1
			x1, y0, z0,//10 left(0)2
			x1, y1, z0,//11 left(0)3
			x0, y0, z1,//00 right(1)4
			x0, y1, z1,//01 right(1)5
			x1, y0, z1,//10 right(1)6
			x1, y1, z1,//11 right(1)7

			x0, y0, z0,//00 up(0)0
			x0, y1, z0,//01 up(0)1
			x1, y0, z0,//10 up(0)2
			x1, y1, z0,//11 up(0)3
			x0, y0, z1,//00 down(1)4
			x0, y1, z1,//01 down(1)5
			x1, y0, z1,//10 down(1)6
			x1, y1, z1,//11 down(1)7
		};
		firstCube = false;
		return CreateShapeObject(positions, 24 * 3, T_CUBE, x0 + size / 2, y0 + size / 2, z0 + size / 2, size);
	}
	glm::mat4 model{ 1.f };
	model = glm::translate(model, glm::vec3{ x0, y0, z0 });
	model = glm::scale(model, glm::vec3{ size, size, size });
	Shape* tempShapePtr = new Shape{ Prototypes.at(T_CUBE) };
	Shape& tempShape = *tempShapePtr;
	tempShape.matrices.model = model;
	tempShape.center[0] = x0+size/2.f;
	tempShape.center[1] = y0+size/2.f;
	tempShape.center[2] = z0+size/2.f;
	tempShape.d = size;
	return tempShape;
}

Shape& ShapeFactory::CreateSphere(float x0, float y0, float z0, float radius) {

	float x, y, z, xy;                              // vertex position
	float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
	//float s, t;                                    // vertex texCoord
	if (firstSphere) {
		std::cout << x0 << ", " << y0 << ", " << z0 << "," << std::endl;
		float sectorStep = 2 * PI / SPHERE_SECTOR_NUM;
		float stackStep = PI / SPHERE_STACK_NUM;
		float sectorAngle, stackAngle;
		float points[(SPHERE_SECTOR_NUM + 1) * (SPHERE_STACK_NUM + 1) * 3];

		//points = (float*)malloc(size * sizeof(float));
		for (int i = 0, n = 0; i <= SPHERE_STACK_NUM; ++i)
		{
			stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
			xy = radius * cosf(stackAngle);             // r * cos(u)
			z = radius * sinf(stackAngle);              // r * sin(u)

			// add (SPHERE_SECTOR_NUM+1) vertices per stack
			// the first and last vertices have same position and normal, but different tex coords
			for (int j = 0; j <= SPHERE_SECTOR_NUM; ++j)
			{
				sectorAngle = j * sectorStep;           // starting from 0 to 2pi

				// vertex position (x, y, z)
				x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
				y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
				points[n] = x + x0;
				points[n + 1] = y + y0;
				points[n + 2] = z + z0;
				// calculating normals
				nx = x * lengthInv;
				ny = y * lengthInv;
				nz = z * lengthInv;

				sphere_normals[n] = nx;
				sphere_normals[n + 1] = ny;
				sphere_normals[n + 2] = nz;
				n += 3;
			}
		}
		firstSphere = false;
		return CreateShapeObject(points, (SPHERE_SECTOR_NUM + 1) * (SPHERE_STACK_NUM + 1) * 3, T_SPHERE, x0, y0, z0, 2 * radius);
	}
	glm::mat4 model{ 1.f };
	model = glm::translate(model, glm::vec3{ x0, y0, z0 });
	model = glm::scale(model, glm::vec3{ radius, radius, radius });
	Shape* tempShapePtr = new Shape{ Prototypes.at(T_SPHERE) };
	Shape& tempShape = *tempShapePtr;
	tempShape.matrices.model = model;
	tempShape.center[0] = x0;
	tempShape.center[1] = y0;
	tempShape.center[2] = z0;
	tempShape.d = 2 * radius;
	return tempShape;
}

Shape& ShapeFactory::CreateCylinder(float x, float y, float z, float radius, float height) {
	if (firstCylinder) {

		float* circle1, * circle2;
		circle1 = CreateCircle(x, y, z, radius);
		circle2 = CreateCircle(x, (y + height), z, radius);
		float cylinder_pos[216];
		for (int i = 0; i < 108; i++) {
			cylinder_pos[i] = circle1[i];
			cylinder_pos[i + 108] = circle2[i];
		}
		free(circle1);
		free(circle2);

		cylinder_normals[0] = cylinder_normals[2] = cylinder_normals[108] = cylinder_normals[110] = 0;
		cylinder_normals[1] = -34 * cos((2 * PI) / 34);
		cylinder_normals[109] = 34 * cos((2 * PI) / 34);

		for (int i = 1, n = 1; i < 36; i++) {
			cylinder_normals[3 * i] = cos(2 * PI * n / 34) + cos(2 * PI * (n + 1) / 34) + cos(PI * (2 * n + 1) / 34);
			cylinder_normals[3 * i + 1] = -2 * sin((2 * PI) / 34);
			cylinder_normals[3 * i + 2] = sin(2 * PI * n / 34) + sin(2 * PI * (n + 1) / 34) + sin(PI * (2 * n + 1) / 34);
			cylinder_normals[3 * i + 108] = cos(2 * PI * n / 34) + cos(2 * PI * (n + 1) / 34) + cos(PI * (2 * n + 1) / 34);
			cylinder_normals[3 * i + 109] = 2 * cos((2 * PI) / 34);
			cylinder_normals[3 * i + 110] = sin(2 * PI * n / 34) + sin(2 * PI * (n + 1) / 34) + sin(PI * (2 * n + 1) / 34);
			n += 1;
		}
		firstCylinder = false;
		return CreateShapeObject(cylinder_pos, 216, T_CYLINDER, x, y + height / 2, z, 2 * radius);
	}
	glm::mat4 model{ 1.f };
	model = glm::translate(model, glm::vec3{ x, y + height / 2.f, z });
	model = glm::scale(model, glm::vec3{ radius, height/2.f, radius });
	Shape prototype = Prototypes.at(T_CYLINDER);
	Shape* tempShapePtr = new Shape{ prototype };
	Shape& tempShape = *tempShapePtr;
	tempShape.matrices.model = model;
	tempShape.center[0] = x;
	tempShape.center[1] = y + height/2.f;
	tempShape.center[2] = z;
	tempShape.d = 2*radius;

	return tempShape;

}

// Creates an Object and its GPU buffer
Shape& ShapeFactory::CreateShapeObject(float * element, int elementSize, int shapeType, float x0, float y0, float z0, float d) {
	std::vector<float> dataVector;
	dataVector.reserve(elementSize);
	for (int i = 0; i < elementSize; i++) {
		dataVector.push_back(element[i]);
	}
	Shape* tempShapePtr = new Shape();
	Shape& tempShape = *tempShapePtr; // trick for heap allocation
	tempShape.size = elementSize;
	tempShape.shapeType = shapeType;
	tempShape.matrices.model = glm::mat4(1.0f);
	tempShape.speed[0] = 0.0f;
	tempShape.speed[1] = 0.0f;
	tempShape.speed[2] = 0.0f;
	tempShape.center[0] = x0;
	tempShape.center[1] = y0;
	tempShape.center[2] = z0;
	tempShape.d = d;
	int index_pointer_size = GetIndexPointerSize(tempShape.shapeType);
	int32_t normal_pointer_size	= GetNormalPointerSize(tempShape.shapeType);
	float* normals = GetNormals(tempShape.shapeType);
	uint32_t *index_array = GetIndexPointer(tempShape.shapeType);

	renderer->createObjectBuffer(tempShape, index_pointer_size, normal_pointer_size, normals, index_array, dataVector);

	return tempShape;
}

/*
Shape Color
-return shape color to pass to shader
*/
float* ShapeFactory::GetColor(Shape& shape) {
	return shape.color;
}

/*
*Simple Color Setter
- just sets rgba color of shape at index
*/
void ShapeFactory::SetColor(Shape& shape, float r_value, float g_value, float b_value, float alpha_value) {
	shape.color[0] = r_value;
	shape.color[1] = g_value;
	shape.color[2] = b_value;
	shape.color[3] = alpha_value;
}

void ShapeFactory::SetRandomColor(Shape& shape, float alpha_value) {
	shape.color[0] = RandomFloat(0.0f, 1.0f);
	shape.color[1] = RandomFloat(0.0f, 1.0f);
	shape.color[2] = RandomFloat(0.0f, 1.0f);
	shape.color[3] = alpha_value;

}


//returns shape normal array pointer for each shape
//getters to help when we need a pointer to a shape's normals or indices
float* ShapeFactory::GetNormals(int shapeType) {
	switch (shapeType)
	{
	case T_CUBE:
		return cube_normals.data();
	case T_SPHERE:
		return sphere_normals.data();
	case T_CYLINDER:
		return cylinder_normals.data();
	case T_RING:
		return ring_normals.data();
	}
	return nullptr; // TODO: REMOVE THIS
}
/*
Index Buffer Pointer Size
- this is needed to draw the right amount of triangles for each shape
*/
uint32_t ShapeFactory::GetIndexPointerSize(uint32_t shapeType) {
	switch (shapeType)
	{
	case T_CUBE:
		return 36;
	case T_CYLINDER:
		return CIRCLE_TRIANGLE_NUM * 3 * 4;
	case T_SPHERE:
		return 2 * 3 * (SPHERE_STACK_NUM - 1) * SPHERE_SECTOR_NUM;
	case T_RING:
		return 2 * 8 * CIRCLE_TRIANGLE_NUM * 3;
	}
	return 0;
}


uint32_t *ShapeFactory::GetIndexPointer(int shapeType) {
	switch (shapeType)
	{
	case T_CUBE:
		return &cube_indices[0];
	case T_CYLINDER:
		return &cylinder_indices[0];
	case T_SPHERE:
		return &sphere_indices[0];
	case T_RING:
		return &ring_indices[0];
	}
	return nullptr;
}

//Random number generators
// TODO: replace with a static random class.
int ShapeFactory::RandomInt(int min, int max) {
	uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<int> distributionInteger(min, max);
	return distributionInteger(generator);
}

float ShapeFactory::RandomFloat(float min, float max) {
	uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_real_distribution<float> distributionDouble(min, max);
	return static_cast<float>(distributionDouble(generator));
}

