#include "DynamicShapeArray.h"

#ifdef _WIN32
	#include <Windows.h>
    #undef max
#endif



//Normals
/*
	Indices for cube triangle points have been numbered in the following way on the 2 faces back and front(+4)
	1 - 3 5 - 7
	| X | | X |
	0 - 2 4 - 6
	back: 0123, front: 4567, left: 0145, right: 2367, bottom: 4062, top: 5173
	The only easy enough to do by hand
*/
float cube_normals[] = {
		-sqrt(2.0f),-sqrt(2.0f),-sqrt(2.0f),
		-sqrt(2.0f),sqrt(2.0f),-sqrt(2.0f),
		sqrt(2.0f),-sqrt(2.0f),-sqrt(2.0f),
		sqrt(2.0f),sqrt(2.0f),-sqrt(2.0f),
		-sqrt(2.0f),-sqrt(2.0f),sqrt(2.0f),
		-sqrt(2.0f),sqrt(2.0f),sqrt(2.0f),
		sqrt(2.0f),-sqrt(2.0f),sqrt(2.0f),
		sqrt(2.0f),sqrt(2.0f),sqrt(2.0f)
};

float sphere_normals[2109];
float cylinder_normals[216];
float ring_normals[8*(CIRCLE_VERTEX_NUM - 1) * 3];

//indice arrays
unsigned int cube_indices[] = {
		4, 6, 5,//front
		7, 5, 6,//front
		0, 1, 2,//back
		3, 2, 1,//back
		0, 4, 1,//left
		5, 1, 4,//left
		2, 3, 6,//right
		3, 7, 6,//right
		5, 7, 1,//top
		3, 1, 7,//top
		4, 0, 6,//bottom
		2, 6, 0 //bottom
};

unsigned int cylinder_indices[CIRCLE_TRIANGLE_NUM * 3 * 4];
unsigned int sphere_indices[2 * 3 * (SPHERE_STACK_NUM - 1) * SPHERE_SECTOR_NUM];
unsigned int  ring_indices[2 * 8 * CIRCLE_TRIANGLE_NUM * 3];


float globalSpeed = 0.5f / GLOBAL_SPEED;
int speedUP = 50;

bool firstCylinder = true;
bool firstRing = true;
bool firstSphere = true;
bool soundsEnabled = false; // testing

DynamicShapeArray::DynamicShapeArray() {
	shapeFactory = new ShapeFactory();
	capacity = 10;
	shapeArray.reserve(capacity);
	size = 0;
}

DynamicShapeArray::~DynamicShapeArray() {
	for (uint64_t i = 0; i < shapeArray.size(); ++i) {
		delete shapeArray[i];
	}
}

void DynamicShapeArray::CreateRandomShape() {
	AddShape(&shapeFactory->CreateRandomShape());
	std::cout << " Shape amount:" << size << std::endl;
}

void DynamicShapeArray::CreateRandomShapes(int amount) {
	int count = 0;
	int perAxis = std::ceil(std::cbrt(amount));
	float px = 0.f, py = 0.f, pz = 0.f;
	for (int i = 0 ; i < perAxis; ++i) {
		for (int j = 0; j < perAxis; ++j) {
			for (int k = 0; k < perAxis; ++k) {
				px = i * 100.f / perAxis;
				py = j * 100.f / perAxis;
				pz = k * 100.f / perAxis;
				AddShape(&shapeFactory->CreateRandomShape(px, py, pz));
			}
		}
	}
}

/*
Shape Creator
-creates new shape to add to the Array
*/
void DynamicShapeArray::CreateShape(float x, float y, float z, int elementSize, int ShapeType) {
	Shape* newShape = &shapeFactory->CreateShape(x, y, z, elementSize, ShapeType);
	AddShape(newShape);
}
void DynamicShapeArray::InitFactoryPrototypes()
{
	shapeFactory->InitPrototypes();
}


//Adds a shape to shapeArray
void DynamicShapeArray::AddShape(Shape *shape) {
	shapeArray.push_back(shape);
	shapeTypeArray.at(shape->shapeType).push_back(shapeArray.back()); // TODO: every time shapeArray resizes we lose the pointers.
	size++;
}


void DynamicShapeArray::SetRandomColor(int index, float alpha_value) {
	shapeFactory->SetRandomColor(*shapeArray[index], alpha_value);
}

void DynamicShapeArray::SetColor(int index, float r_value, float g_value, float b_value, float alpha_value) {
	shapeFactory->SetColor(*shapeArray[index], r_value, g_value, b_value, alpha_value);
}
void DynamicShapeArray::setRenderer(OpenGLRenderer* renderer) {
	shapeFactory->setRenderer(renderer);
}

float* DynamicShapeArray::GetColor(int index) {
	if (index < size) {
		return shapeFactory->GetColor(*shapeArray[index]);
	}
	else return nullptr;
}

uint32_t DynamicShapeArray::GetIndexPointerSize(uint32_t shapeType) {
	return shapeFactory->GetIndexPointerSize(shapeType);
}

float* DynamicShapeArray::GetNormals(int shapeType) {
	return shapeFactory->GetNormals(shapeType);
}

void DynamicShapeArray::BindShape(int index) {
	shapeFactory->BindShape(*shapeArray[index]);
}

void DynamicShapeArray::Move(int index) {
	Shape& shape = *shapeArray[index];
	float* speed = shape.speed;
	if (speed[0] || speed[1] || speed[2]) {
		CheckCollision(index);
		shape.matrices.model = glm::translate(glm::mat4{1.f}, glm::vec3(speed[0] * (speedUP * globalSpeed), speed[1] * (speedUP * globalSpeed), speed[2] * (speedUP * globalSpeed))) * shape.matrices.model;
		shape.matrices.normalModel = glm::mat3x4{ glm::transpose(glm::inverse(shape.matrices.model)) };
		shape.center[0] += speed[0] * (speedUP * globalSpeed);
		shape.center[1] += speed[1] * (speedUP * globalSpeed);
		shape.center[2] += speed[2] * (speedUP * globalSpeed);
	}
}
void DynamicShapeArray::Move(int index, glm::mat4 view, glm::mat4& projection) {
	Move(index);
	shapeArray[index]->matrices.mvp = projection * view * shapeArray[index]->matrices.model;
}
void DynamicShapeArray::MoveAll() {
	// Still i = 2 because first 2 shapes are immovable (cube and sphere)
	for (int i = 2; i < size; i++) {
		Move(i);
	}
}
void DynamicShapeArray::MoveAll(glm::mat4 view, glm::mat4& projection) {
	// Still i = 2 because first 2 shapes are immovable (cube and sphere)
	for (int i = 2; i < size; i++) {
		Move(i, view, projection);
	}
}

void DynamicShapeArray::uploadMatricesToPtr(int shapeType, uint16_t type, void* ptr) {
	objMatrices* matricesPtr = static_cast<objMatrices*>(ptr);
	for (uint64_t i = 0; i < shapeTypeArray[shapeType].size(); ++i) {
		Shape* shape = shapeTypeArray[shapeType][i];
		matricesPtr[i].mvp = shape->matrices.mvp;
		matricesPtr[i].model = shape->matrices.model;
		matricesPtr[i].normalModel = shape->matrices.normalModel;
	}
}
void DynamicShapeArray::uploadColorsToPtr(int shapeType, uint16_t type, void* ptr) {
	float* colorsPtr = static_cast<float*>(ptr);
	for (uint64_t i = 0; i < shapeTypeArray[shapeType].size(); ++i) {
		Shape* shape = shapeTypeArray[shapeType][i];
		colorsPtr[i * 4 + 0] = shape->color[0];
		colorsPtr[i * 4 + 1] = shape->color[1];
		colorsPtr[i * 4 + 2] = shape->color[2];
		colorsPtr[i * 4 + 3] = shape->color[3];
	}
}

/*
*Sphere Mover
-Handles Sphere movement because it needs to be moved by input
*/

void DynamicShapeArray::MoveSphere(int index, glm::vec3 speed)
{
	Shape& shape = *shapeArray[index];
	int next_center[3] = { shape.center[0] + speed[0],
	shape.center[1] + speed[1],
	shape.center[2] + speed[2]};

	float upper_limit = 100.0f - shape.d / 2;
	float lower_limit = 0 + shape.d / 2;
	CheckCollision(index);
	if (next_center[0] > upper_limit || next_center[1] > upper_limit || next_center[2] > upper_limit || next_center[0] < lower_limit || next_center[1] < lower_limit || next_center[2] < lower_limit)
		return;
	shape.matrices.model = glm::translate(glm::mat4(1.0f), speed) * shape.matrices.model;
	shape.center[0] = next_center[0];
	shape.center[1] = next_center[1];
	shape.center[2] = next_center[2];
}

/*
*Speed modifier
*/
void DynamicShapeArray::SpeedUP(bool up) {

	if (speedUP < MAX_SPEEDUP && up) {
		speedUP++;
		std::cout << "Speed modifier: " << speedUP << " (Default: 50, Max: " << MAX_SPEEDUP << ")" << std::endl;
	}
	else if (speedUP > 0 && !up) {
		speedUP--;
		std::cout << "Speed modifier: " << speedUP << " (Default: 50, Max: " << MAX_SPEEDUP << ")" << std::endl;
	}
	
}


/*
*Handling Collisions
-handles collisions and changes speeds when it happens
*/
void DynamicShapeArray::CheckCollision(int index) {
	// indices 0 and 1 are the semi-transparent cube and the textured sphere. They're immovable by collision.
	if (index >= size || index < 2) {
		return;
	}
	bool hasCollision;
	Shape& shapeInd = *shapeArray[index];
	float bigPos[] = { shapeInd.center[0] + shapeInd.speed[0],
		shapeInd.center[1] + shapeInd.speed[1],
		shapeInd.center[2] + shapeInd.speed[2] };
	float nextPos1[3];

	float* pos, * pos1;
	int shapeType = shapeInd.shapeType, j, s;
	float dsqr, dx, dy, dz, size0 = shapeInd.d, size1;
	for (int i = 0; i < size; i++) {
		hasCollision = false;
		j = index;
		s = i;
		Shape shapeI = *shapeArray[i];
		Shape shapeJ = *shapeArray[j];
		nextPos1[0] = shapeI.center[0] + shapeI.speed[0];
		nextPos1[1] = shapeI.center[1] + shapeI.speed[1];
		nextPos1[2] = shapeI.center[2] + shapeI.speed[2];
		pos = bigPos;
		pos1 = nextPos1;
		if (i == index)
			continue;
		if (shapeJ.shapeType == T_RING || shapeJ.shapeType == T_SPHERE || (shapeI.shapeType == T_CUBE && shapeJ.shapeType == T_CYLINDER)) {
			Shape temp = shapeI;
			shapeI = shapeJ;
			shapeJ = temp;
			j = i;
			s = index;
			pos1 = bigPos;
			pos = nextPos1;
		}
		shapeType = shapeJ.shapeType;
		size0 = shapeI.d;
		size1 = shapeJ.d;
		dx = abs(pos[0] - pos1[0]);
		dy = abs(pos[1] - pos1[1]);
		dz = abs(pos[2] - pos1[2]);
		if (shapeI.shapeType == T_SPHERE) {
			if (shapeJ.shapeType == T_SPHERE) {
				dsqr = dx * dx + dy * dy + dz * dz;
				hasCollision = (dsqr <= (size0 / 2 + size1 / 2) * (size0 / 2 + size1 / 2)) && (dsqr >= size1 * size1 / 4);
			}
			else if (shapeJ.shapeType == T_CUBE) {

				if (dx >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dy >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dz >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				//be completely in
				else if ((dx < abs((size1 - size0) / 2)) && (dy < abs((size1 - size0) / 2)) && (dz < abs((size1 - size0) / 2))) { hasCollision = false; }

				else if (dx < (size1 / 2)) { hasCollision = true; }
				else if (dy < (size1 / 2)) { hasCollision = true; }
				else if (dz < (size1 / 2)) { hasCollision = true; }
				else {
					float cornerDistance_sq = ((dx - size1 / 2) * (dx - size1 / 2)) +
						((dy - size1 / 2) * (dy - size1 / 2)) +
						((dz - size1 / 2) * (dz - size1 / 2));
					hasCollision = (cornerDistance_sq < (size0 / 2 * size0 / 2));
				}

			}
			else if (shapeJ.shapeType == T_CYLINDER) {
				dsqr = dx * dx + dz * dz;

				if (dx > (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dy > (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dz > (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if ((dsqr <= (((size0 / 2) + (size1 / 2)) * ((size1 / 2) + (size0 / 2))) && dy <= (size1 / 2))) { hasCollision = true; }
				else {
					dsqr = dx * dx + dy * dy + dz * dz;
					hasCollision = (dsqr <= (size0 / 2 + SQRT_2 * size1 / 2) * (size0 / 2 + SQRT_2 * size1 / 2));
				}
			}
		}
		else if (shapeI.shapeType == T_CUBE) {
			if (shapeJ.shapeType == T_CUBE) {
				hasCollision = dx <= size1 / 2 + size0 / 2 && dy <= size1 / 2 + size0 / 2 && dz <= size1 / 2 + size0 / 2 && (dx >= abs(size1 - size0) / 2 || dy >= abs(size1 - size0) / 2 || dz >= abs(size1 - size0) / 2);

			}
		}
		else if (shapeI.shapeType == T_CYLINDER) {
			if (shapeJ.shapeType == T_CUBE) {
				if (dx >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dy >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dz >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				//be completely in 
				else if ((dx < abs((size1 / 2) - (size0 / 2)) && (dy < abs((size1 / 2) - (size0 / 2))) && (dz < abs((size1 / 2) - (size0 / 2))))) { hasCollision = false; }

				else if (dx < (size1 / 2)) { hasCollision = true; }
				else if (dy < (size1 / 2)) { hasCollision = true; }
				else if (dz < (size1 / 2)) { hasCollision = true; }
				else {
					float cornerDistance_sq = ((dx - size1 / 2) * (dx - size1 / 2)) +
						((dy - size1 / 2) * (dy - size1 / 2)) +
						((dz - size1 / 2) * (dz - size1 / 2));
					hasCollision = (cornerDistance_sq < (size0* size0 / 2));
				}
			}
			else if (shapeJ.shapeType == T_CYLINDER) {
				dsqr = dx * dx + dz * dz;
				if (dsqr > (size1 / 2 + size0 / 2) * (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dy >= (size1 / 2 + size0 / 2)) { hasCollision = false; }
				else if (dy < (size1 / 2)) { hasCollision = true; }
				else {
					dsqr = dx * dx + dy * dy + dz * dz;
					hasCollision = dsqr <= (SQRT_2 * size0 / 2 + SQRT_2 * size1 / 2) * (SQRT_2 * size0 + SQRT_2 * size1 / 2);
				}
			}
		}
		else if (shapeI.shapeType == T_RING) {
			float size00 = shapeI.d2, radi = size0 - (2 * size00), radB = size0;

			if (shapeJ.shapeType == T_CUBE) {
				if (dx >= (size1 / 2 + size0)) { hasCollision = false; }
				else if (dy >= (size1 / 2 + size0)) { hasCollision = false; }
				else if (dz >= (size1 / 2 + size0)) { hasCollision = false; }
				//be completely in 
				else if ((dx < abs((size1 / 2) - (size0)) && (dy < abs((size1 / 2) - (size0))) && (dz < abs((size1 / 2) - (size0))))) { hasCollision = false; }

				else if (dx < (size1 / 2)) { hasCollision = true; }
				else if (dy < (size1 / 2) + size00) { hasCollision = true; }
				else if (dz < (size1 / 2)) { hasCollision = true; }
				else {
					float cornerDistance_sq = ((dx - size1 / 2) * (dx - size1 / 2)) +
						((dy - size1 / 2) * (dy - size1 / 2)) +
						((dz - size1 / 2) * (dz - size1 / 2));
					hasCollision = true;
				}

			}
			else if (shapeJ.shapeType == T_CYLINDER) {
				hasCollision = false;
			}
			else if (shapeJ.shapeType == T_SPHERE) {
				dsqr = dx * dx + dz * dz;

				if (dx > (size1 / 2 + size0)) { hasCollision = false; }
				else if (dy > (size1 / 2 + size0)) { hasCollision = false; }
				else if (dz > (size1 / 2 + size0)) { hasCollision = false; }
				else if ((dsqr <= (((size0)+(size1 / 2)) * ((size1 / 2) + (size0))) && dy <= (size1 / 2))) { hasCollision = true; }
				else {
					dsqr = dx * dx + dy * dy + dz * dz;
					hasCollision = (dsqr <= (size0 / 2 + SQRT_2 * size1 / 2) * (size0 / 2 + SQRT_2 * size1 / 2));
				}
			}
			else if (shapeJ.shapeType == T_RING) {
				hasCollision = false;
			}
		}

		if (hasCollision) {
			Collide(s, j);
			Collide(j, s);
#ifdef _WIN32
			if (index <= 10 && soundsEnabled) {
				PlaySound(TEXT("collision.wav"), NULL, SND_FILENAME | SND_ASYNC);
			}
#endif
		}
	}
}

/*
*Collision
-changes the speeds of the shapes in a collision
*/
void DynamicShapeArray::Collide(int index1, int index2) {
	Shape& shape1 = *shapeArray[index1];
	Shape& shape2 = *shapeArray[index2];
	if (shape2.speed[0] == 0 && shape2.speed[1] == 0 && shape2.speed[2] == 0) {
		return;
	}

	int shapeType2 = shape2.shapeType;
	int shapeType1 = shape1.shapeType;
	float* pos = shape1.center;
	float* pos1 = shape2.center;
	glm::vec3 speed(shape2.speed[0], shape2.speed[1], shape2.speed[2]);
	glm::vec3 Tspeed(0.0f, 0.0f, 0.0f);
	float dx = pos[0] - pos1[0];
	float dy = pos[1] - pos1[1];
	float dz = pos[2] - pos1[2];

	if (shapeType1 == T_SPHERE) {

		glm::vec3 centerToCenter(dx, dy, dz);
		centerToCenter = glm::normalize(centerToCenter);
		Tspeed = glm::normalize(speed);

		speed = glm::length(speed) * normalize((-centerToCenter) * glm::dot(Tspeed, centerToCenter));

		shape2.speed[0] = speed[0];
		shape2.speed[1] = speed[1];
		shape2.speed[2] = speed[2];
	}
	if (shapeType1 == T_CUBE) {
		glm::vec3 X(1.0f, 0.0f, 0.0f);
		glm::vec3 Y(0.0f, 1.0f, 0.0f);
		glm::vec3 Z(0.0f, 0.0f, 1.0f);

		glm::vec3 centerToCenter(dx, dy, dz);

		centerToCenter = glm::normalize(centerToCenter);
		float dists[3] = { glm::dot(centerToCenter,X),  glm::dot(centerToCenter,Y),  glm::dot(centerToCenter,Z) };
		float m = abs(dists[0]);
		m = std::max(m, abs(dists[1]));
		m = std::max(m, abs(dists[2]));
		glm::vec3 M;

		if (m == abs(dists[0])) {
			shape2.speed[0] = -speed[0];
		}if (m == abs(dists[1])) {
			shape2.speed[1] = -speed[1];
		}if (m == abs(dists[2])) {
			shape2.speed[2] = -speed[2];
		}
	}
	if (shapeType1 == T_CYLINDER) {
		glm::vec3 X(1.0f, 0.0f, 0.0f);
		glm::vec3 Y(0.0f, 1.0f, 0.0f);
		glm::vec3 Z(0.0f, 0.0f, 1.0f);

		glm::vec3 centerToCenter(dx, dy, dz);
		float size1 = shape1.size / 2;
		float size2 = shape2.size / 2;
		Tspeed = glm::normalize(speed);
		centerToCenter = glm::normalize(centerToCenter);
		glm::vec3 ctc2(centerToCenter[0], 0, centerToCenter[2]);
		float dists[2] = { cos((acos(abs(glm::dot(centerToCenter,X))) + acos(abs(glm::dot(centerToCenter,Z)))) / 2),  glm::dot(centerToCenter,Y) };
		float m = abs(dists[1]);
		m = std::max(m, abs(dists[0]));

		glm::vec3 M;

		if ((dists[1] >= SQRT_2 / 2 && dists[1] < 1) || (dists[1] <= -SQRT_2 / 2 && dists[1] > -1)) {
			shape2.speed[1] = -speed[1];

		}
		else if (dy < size1) {

			Tspeed = glm::length(speed) * normalize((-centerToCenter) * glm::dot(Tspeed, centerToCenter));
			shape2.speed[0] = Tspeed[0];
			shape2.speed[2] = Tspeed[2];
		}
	}
	if (shapeType1 == T_RING) {
		glm::vec3 X(1.0f, 0.0f, 0.0f);
		glm::vec3 Y(0.0f, 1.0f, 0.0f);
		glm::vec3 Z(0.0f, 0.0f, 1.0f);

		float s1 = shape1.d;
		float s2 = shape1.d2;


		glm::vec3 centerToCenter(dx - s1 - s2, dy, dz - s1 - s2);
		float size1 = shape1.size / 2;
		float size2 = shape2.size / 2;
		Tspeed = glm::normalize(speed);
		centerToCenter = glm::normalize(centerToCenter);
		glm::vec3 ctc2(centerToCenter[0], 0, centerToCenter[2]);
		float dists[2] = { cos((acos(abs(glm::dot(centerToCenter,X))) + acos(abs(glm::dot(centerToCenter,Z)))) / 2),  glm::dot(centerToCenter,Y) };
		float m = abs(dists[1]);
		m = std::max(m, abs(dists[0]));

		glm::vec3 M;

		if ((dists[1] >= SQRT_2 / 2 && dists[1] < 1) || (dists[1] <= -SQRT_2 / 2 && dists[1] > -1)) {
			shape2.speed[1] = -speed[1];

		}
		else if (dy < size1) {

			Tspeed = glm::length(speed) * normalize((-centerToCenter) * glm::dot(Tspeed, centerToCenter));
			shape2.speed[1] = Tspeed[1];
			shape2.speed[0] = Tspeed[0];
			shape2.speed[2] = Tspeed[2];
		}
	}
}
