#include "DynamicShapeArray.h"
#include <cmath>

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

float globalSpeed = 20.f / GLOBAL_SPEED;
float sphereSpeed = 1000.f / GLOBAL_SPEED;
int speedUP = 50;

bool firstCylinder = true;
bool firstRing = true;
bool firstSphere = true;
bool soundsEnabled = false; // testing

DynamicShapeArray::DynamicShapeArray() {
	shapeFactory = new ShapeFactory();
	capacity = 10;
	shapeArray.reserve(capacity);
	m_nearbyCache.reserve(20);
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
	int perAxis = static_cast<int>(std::ceil(std::cbrt(amount)));
	// TODO: change these hardcoded variables to something intuitive
	float maxSize = static_cast<float>(100.f / perAxis * .8f);
	maxSize = maxSize > 2 ? maxSize : 2;
	maxSize = maxSize < 10 ? maxSize : 10;
	float px = 0.f, py = 0.f, pz = 0.f;
	for (int i = 0 ; i < perAxis; ++i) {
		for (int j = 0; j < perAxis; ++j) {
			for (int k = 0; k < perAxis; ++k) {
				px = i * 100.f / perAxis;
				py = j * 100.f / perAxis;
				pz = k * 100.f / perAxis;
				AddShape(&shapeFactory->CreateRandomShape(px, py, pz, maxSize));
			}
		}
	}
}

/*
Shape Creator
-creates new shape to add to the Array
*/
void DynamicShapeArray::CreateShape(float x, float y, float z, float elementSize, int ShapeType) {
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

float* DynamicShapeArray::GetColor(uint32_t index) {
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

void DynamicShapeArray::UpdatePhysics(float deltaTime) {
	float speedFactor = speedUP * globalSpeed * deltaTime;
	// Still i = 2 because first 2 shapes are immovable (cube and sphere)
	for (uint32_t i = 2; i < size; ++i) {
		Shape* shape = shapeArray[i];
		shape->center[0] += shape->speed[0] * speedFactor;
		shape->center[1] += shape->speed[1] * speedFactor;
		shape->center[2] += shape->speed[2] * speedFactor;
	}
	CheckAllCollisions();
}

void DynamicShapeArray::UpdateMatrices(const glm::mat4& view, const glm::mat4& projection) {
	// Still i = 2 because first 2 shapes are immovable (cube and sphere)
	glm::mat4 viewProj = projection * view;
	for (uint32_t i = 1; i < size; ++i) {
		Shape* shape = shapeArray[i];
		glm::mat4 model{ 1.f };

		// New approach, translate using center instead of speed to avoid speedups
		model = glm::translate(glm::mat4{ 1.f }, glm::vec3(shape->center[0], shape->center[1], shape->center[2]));
		model = glm::scale(model, shape->scale);
		shape->matrices.model = model;
		shape->matrices.normalModel = glm::mat3x4{ glm::transpose(glm::inverse(shape->matrices.model)) };
		shape->matrices.mvp = viewProj * shape->matrices.model;
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
	float next_center[3] = { shape.center[0] + sphereSpeed * speed[0],
	shape.center[1] + sphereSpeed * speed[1],
	shape.center[2] + sphereSpeed * speed[2]};

	float upper_limit = 100.0f - shape.d / 2;
	float lower_limit = 0 + shape.d / 2;
	for (uint32_t i = 2; i < size; ++i) {
		CheckCollisionPair(i, index); // Check for sphere
	}
	if (next_center[0] > upper_limit || next_center[1] > upper_limit || next_center[2] > upper_limit || next_center[0] < lower_limit || next_center[1] < lower_limit || next_center[2] < lower_limit)
		return;
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

void DynamicShapeArray::CheckAllCollisions() {
	m_SpatialGrid.clear();
	std::vector<int> largeObjects; // Special case for large objects that span multiple cells

	// skip immovable objects 0, 1
	for (uint32_t i = 2; i < size; ++i) { 
		Shape* shape = shapeArray[i];

		if (shape->d > 30.f) {
			largeObjects.push_back(i);
		} else {
			// Next positions
			float px = shape->center[0] + shape->speed[0];
			float py = shape->center[1] + shape->speed[1];
			float pz = shape->center[2] + shape->speed[2];
			m_SpatialGrid.insert(i, px, py, pz);
		}
	}

	for (uint32_t i = 2; i < size; ++i) {
		if (shapeArray[i]->d > 30.f) continue;
		Shape* shape = shapeArray[i];
		float px = shape->center[0] + shape->speed[0];
		float py = shape->center[1] + shape->speed[1];
		float pz = shape->center[2] + shape->speed[2];
		m_nearbyCache.clear();	
		m_SpatialGrid.queryNeighbors(px, py, pz, m_nearbyCache);

		for (uint32_t j : m_nearbyCache) {
			if (j <= i) continue; // avoid double checks
			CheckCollisionPair(i, j);
		}
	}

	// We don't have large movable objects yet
	for (int largeIdx : largeObjects) {
		for (uint32_t i = 2; i < size; ++i) {
			if (i == largeIdx) continue;
			CheckCollisionPair(largeIdx, i);
		}
	}

	for (uint32_t i = 2; i < size; ++i) {
		CheckCollisionPair(i, 0); // Check for cube
		CheckCollisionPair(i, 1); // Check for sphere
	}

}

void DynamicShapeArray::CheckCollisionPair(int i, int j) {
	bool hasCollision{ false };
	//bool hasCollision; This is here to debug when all if statements are passed since VS breaks when an uninitialized bool is used

	Shape* shapeI = shapeArray[i];
	Shape* shapeJ = shapeArray[j];
	float iPos[] = { shapeI->center[0] + shapeI->speed[0],
		shapeI->center[1] + shapeI->speed[1],
		shapeI->center[2] + shapeI->speed[2] };
	float jPos[] = { shapeJ->center[0] + shapeJ->speed[0],
		shapeJ->center[1] + shapeJ->speed[1],
		shapeJ->center[2] + shapeJ->speed[2] };

	float* pos = iPos , * pos1 = jPos;
	int shapeIType, shapeJType, s;
	float dsqr, dx, dy, dz, size0 = shapeI->d, size02 = shapeI->d2, size1 = shapeJ->d, size1div2, size0div2, size10, size10div2, size1p0div2;

	s = i;
	shapeIType = shapeI->shapeType;
	shapeJType = shapeJ->shapeType;

	// Swap shapes to canonical ordering:
	// - Ring always goes to I
	// - Sphere always goes to I unless paired with Ring
	// - Cylinder goes to I only if paired with Cube
	if ( ( (shapeJType == T_RING || shapeJType == T_SPHERE) && (shapeIType != T_RING) ) || 
		(shapeIType == T_CUBE && shapeJType == T_CYLINDER) ) {
		std::swap(size0, size1);
		size02 = shapeJ->d2;
		std::swap(shapeIType, shapeJType);
		std::swap(i, j);

		std::swap(pos1, pos);
	}

	size1div2 = size1 * .5f;
	size0div2 = size0 * .5f;
	size1p0div2 = size1div2 + size0div2;
	size10 = size1 - size0;
	size10 = size10 < 0 ? -size10 : size10;
	size10div2 = size10 * .5f;

	// AABB early test 
	if (pos[0] + size0div2 < pos1[0] - size1div2 || pos1[0] + size1div2 < pos[0] - size0div2) return;
	if (pos[1] + size0div2 < pos1[1] - size1div2 || pos1[1] + size1div2 < pos[1] - size0div2) return;
	if (pos[2] + size0div2 < pos1[2] - size1div2 || pos1[2] + size1div2 < pos[2] - size0div2) return;
	
//	dx = abs(pos[0] - pos1[0]);
//	dy = abs(pos[1] - pos1[1]);
//	dz = abs(pos[2] - pos1[2]);
	// This unintelligible code is faster than std::abs, still left it here for readability
	dx = pos[0] - pos1[0];
	dx = dx < 0 ? -dx : dx;
	dy = pos[1] - pos1[1];
	dy = dy < 0 ? -dy : dy;
	dz = pos[2] - pos1[2];
	dz = dz < 0 ? -dz : dz;
	if (shapeIType == T_SPHERE) {
		if (shapeJType == T_SPHERE) {
			dsqr = dx * dx + dy * dy + dz * dz;
			hasCollision = (dsqr <= (size1p0div2 * size1p0div2)) && (dsqr >= size1 * size1 / 4.f);
		}
		else if (shapeJType == T_CUBE) {

			if (dx >= (size1p0div2)) { hasCollision = false; }
			else if (dy >= (size1p0div2)) { hasCollision = false; }
			else if (dz >= (size1p0div2)) { hasCollision = false; }
			//be completely in
			else if ((dx < size10div2) && (dy < size10div2) && (dz < size10div2)) { hasCollision = false; }

			else if (dx < (size1div2)) { hasCollision = true; }
			else if (dy < (size1div2)) { hasCollision = true; }
			else if (dz < (size1div2)) { hasCollision = true; }
			else {
				float cornerDistance_sq = ((dx - size1div2) * (dx - size1div2)) +
					((dy - size1div2) * (dy - size1div2)) +
					((dz - size1div2) * (dz - size1div2));
				hasCollision = (cornerDistance_sq < (size0div2 * size0div2));
			}

		}
		else if (shapeJType == T_CYLINDER) {
			dsqr = dx * dx + dz * dz;

			if (dx > (size1p0div2)) { hasCollision = false; }
			else if (dy > (size1p0div2)) { hasCollision = false; }
			else if (dz > (size1p0div2)) { hasCollision = false; }
			else if ((dsqr <= ((size1p0div2) * (size1p0div2)) && dy <= (size1div2))) { hasCollision = true; }
			else {
				dsqr = dx * dx + dy * dy + dz * dz;
				hasCollision = (dsqr <= (size0div2 + SQRT_2 * size1div2) * (size0div2 + SQRT_2 * size1div2));
			}
		}
	}
	else if (shapeIType == T_CUBE) {
		if (shapeJType == T_CUBE) {
			hasCollision = dx <= size1p0div2 && dy <= size1p0div2 && dz <= size1p0div2 && (dx >= size10div2 || dy >= size10div2 || dz >= size10div2);
		}
	}
	else if (shapeIType == T_CYLINDER) {
		if (shapeJType == T_CUBE) {
			if (dx >= (size1p0div2)) { hasCollision = false; }
			else if (dy >= (size1p0div2)) { hasCollision = false; }
			else if (dz >= (size1p0div2)) { hasCollision = false; }
			//be completely in 
			else if ((dx < size10div2 && (dy < size10div2) && (dz < size10div2))) { hasCollision = false; }

			else if (dx < (size1div2)) { hasCollision = true; }
			else if (dy < (size1div2)) { hasCollision = true; }
			else if (dz < (size1div2)) { hasCollision = true; }
			else {
				float cornerDistance_sq = ((dx - size1div2) * (dx - size1div2)) +
					((dy - size1div2) * (dy - size1div2)) +
					((dz - size1div2) * (dz - size1div2));
				hasCollision = (cornerDistance_sq < (size0* size0div2));
			}
		}
		else if (shapeJType == T_CYLINDER) {
			dsqr = dx * dx + dz * dz;
			if (dsqr > (size1p0div2) * (size1p0div2)) { hasCollision = false; }
			else if (dy >= (size1p0div2)) { hasCollision = false; }
			else if (dy < (size1div2)) { hasCollision = true; }
			else {
				dsqr = dx * dx + dy * dy + dz * dz;
				hasCollision = dsqr <= (SQRT_2 * size0div2 + SQRT_2 * size1div2) * (SQRT_2 * size0 + SQRT_2 * size1div2);
			}
		}
	}
	else if (shapeIType == T_RING) {
		float radi = size0 - (2 * size02), radB = size0;

		if (shapeJType == T_CUBE) {
			if (dx >= (size1p0div2)) { hasCollision = false; }
			else if (dy >= (size1p0div2)) { hasCollision = false; }
			else if (dz >= (size1p0div2)) { hasCollision = false; }
			//be completely in 
			else if ((dx < size10div2 && (dy < size10div2) && (dz < size10div2))) { hasCollision = false; }

			else if (dx < (size1div2)) { hasCollision = true; }
			else if (dy < (size1div2) + size02) { hasCollision = true; }
			else if (dz < (size1div2)) { hasCollision = true; }
			else {
				
				float cornerDistance_sq = ((dx - size1div2) * (dx - size1div2)) +
					((dy - size1div2) * (dy - size1div2)) +
					((dz - size1div2) * (dz - size1div2));
				hasCollision = true;
			}

		}
		else if (shapeJType == T_CYLINDER) {
			hasCollision = false;
		}
		else if (shapeJType == T_SPHERE) {
			dsqr = dx * dx + dz * dz;

			if (dx > (size1div2 + size0)) { hasCollision = false; }
			else if (dy > (size1div2 + size0)) { hasCollision = false; }
			else if (dz > (size1div2 + size0)) { hasCollision = false; }
			else if ((dsqr <= (((size0)+(size1div2)) * ((size1div2) + (size0))) && dy <= (size1div2))) { hasCollision = true; }
			else {
				dsqr = dx * dx + dy * dy + dz * dz;
				hasCollision = (dsqr <= (size0div2 + SQRT_2 * size1div2) * (size0div2 + SQRT_2 * size1div2));
			}
		}
		else if (shapeJType == T_RING) {
			hasCollision = false;
		}
	}

	if (hasCollision) {
		Collide(i, j);
		Collide(j, i);
#ifdef _WIN32
		// Play sound on collision for the first 5 shapes only to avoid sound spam
		if (i <= 5 && soundsEnabled) {
			PlaySound(TEXT("collision.wav"), NULL, SND_FILENAME | SND_ASYNC);
		}
#endif
	}
}

/*
*Collision
-changes the speeds of the shapes in a collision
*/
// TODO: FIX THE SIZE STUFF, it's horrifying it works remotely well
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
		// WOWZIES THIS IS SUPER WRONG. SIZE IS THE ELEMENT BUFFER SIZE NOT THE RADIUS OR HEIGHT
		float size1 = shape1.d / 2;
		//float size2 = shape2.size / 2;
		Tspeed = glm::normalize(speed);
		centerToCenter = glm::normalize(centerToCenter);
		glm::vec3 ctc2(centerToCenter[0], 0, centerToCenter[2]);
		float dists[2] = { cos((acos(abs(glm::dot(centerToCenter,X))) + acos(abs(glm::dot(centerToCenter,Z)))) / 2),  glm::dot(centerToCenter,Y) };
		float m = abs(dists[1]);
		m = std::max(m, abs(dists[0]));

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
		// WOWZIES THIS IS SUPER WRONG. SIZE IS THE ELEMENT BUFFER SIZE NOT THE RADIUS OR HEIGHT
		float size1 = shape1.d / 2.f;
		//float size2 = shape2.d / 2.f;
		Tspeed = glm::normalize(speed);
		centerToCenter = glm::normalize(centerToCenter);
		glm::vec3 ctc2(centerToCenter[0], 0, centerToCenter[2]);
		float dists[2] = { cos((acos(abs(glm::dot(centerToCenter,X))) + acos(abs(glm::dot(centerToCenter,Z)))) / 2),  glm::dot(centerToCenter,Y) };
		float m = abs(dists[1]);
		m = std::max(m, abs(dists[0]));

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
