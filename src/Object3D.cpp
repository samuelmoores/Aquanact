#include <EngineCamera.h>
#include "Globals.h"
#include "ModelImporter.h"
#include "Object3D.h"
#include "AnimatorComponent.h"
#include <iomanip>

void printMatrix(const glm::mat4& m) {
	std::cout << std::fixed << std::setprecision(3);

	for (int row = 0; row < 4; ++row) {
		std::cout << "[ ";
		for (int col = 0; col < 4; ++col) {
			std::cout << std::setw(9) << m[col][row];
			if (col < 3) std::cout << " ";
		}
		std::cout << " ]\n";
	}
	std::cout << "----------------------------------------------\n";
}


Object3D::Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_skinned = false;
	m_mesh = new Mesh(vertices, faces);

	m_shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	//m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();
	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//                         
	//m_shader.setUniform("material", glm::vec4(1.0f, 0.5f, 0.9f, 228.0f));
	//m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	//m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	//m_shader.setUniform("directionalLight", glm::vec3(0, -1, 0));

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
	m_initialWorldCenter = WorldCenterPosition();
}

Object3D::Object3D(const char* modelFile)
{
	auto model = ModelImporter().Import(modelFile, true);
	m_mesh = new Mesh(std::move(model));
	m_skinned = m_mesh->Skinned();

	if (m_skinned)
	{
		AddComponent<AnimatorComponent>(m_mesh);
	}

	m_shader.load("shaders/phong.vert", "shaders/phong.frag");

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
	m_initialWorldCenter = WorldCenterPosition();

	m_sourcePath = modelFile;
	m_name = m_sourcePath;
	size_t lastSlash = m_name.find_last_of("/\\");
	if (lastSlash != std::string::npos)
		m_name = m_name.substr(lastSlash + 1);
}

Object3D::~Object3D()
{
}

Mesh* Object3D::GetMesh()
{
	return m_mesh;
}

AnimatorComponent* Object3D::GetAnimatorComponent()
{
	return GetComponent<AnimatorComponent>();
}

ShaderProgram* Object3D::GetShader()
{
	return &m_shader;
}

glm::mat4 Object3D::BuildModelMatrix()
{
	auto m = glm::translate(glm::mat4(1), m_position);
	//std::cout << "m_position: " << m_position.x << ", " << m_position.y << ", " << m_position.z << std::endl;
	//std::cout << "m after glm::translate m_position: \n";
	//printMatrix(m);

	m = glm::rotate(m, m_rotation[1], glm::vec3(0, 1, 0));
	//std::cout << "m after glm::rotate y: \n";
	//printMatrix(m);

	m = glm::rotate(m, m_rotation[0], glm::vec3(1, 0, 0));
	//std::cout << "m after glm::rotate x: \n";
	//printMatrix(m);

	m = glm::rotate(m, m_rotation[2], glm::vec3(0, 0, 1));
	//std::cout << "m after glm::rotate z: \n";
	//printMatrix(m);

	m = glm::scale(m, m_scale);

	return m;
}

void Object3D::Rotate(glm::vec3 delta)
{
	m_rotation += delta;
}

void Object3D::Move(glm::vec3 delta)
{
	m_position += delta;
	updateMeshAABB(delta);
}

void Object3D::Translate(glm::vec3 delta)
{
	m_position += delta;
	updateMeshAABB(delta);
}

glm::vec3 Object3D::Position()
{
	return m_position;
}

glm::vec3 Object3D::WorldPosition()
{
	const glm::mat4 model = BuildModelMatrix();
	return glm::vec3(model[3]);
}

glm::vec3 Object3D::WorldCenterPosition()
{
	const glm::vec3 localCenter = m_mesh ? m_mesh->centerAABB() : glm::vec3(0.0f);
	const glm::vec4 worldCenter = BuildModelMatrix() * glm::vec4(localCenter, 1.0f);
	return glm::vec3(worldCenter);
}

glm::vec3 Object3D::InitialWorldCenterPosition() const
{
	return m_initialWorldCenter;
}

glm::vec3 Object3D::Rotation()
{
	return m_rotation;
}

void Object3D::SetRotation(glm::vec3 newRotation)
{
	m_rotation = newRotation;
}

void Object3D::Scale(glm::vec3 delta)
{
	m_scale += delta;
}

void Object3D::SetScale(glm::vec3 scale)
{
	m_scale = scale;
}

void Object3D::updateMeshAABB(glm::vec3 delta)
{
	m_mesh->updateAABB(delta, m_scale);
}

void Object3D::UpdateComponents(float dt)
{
	for (auto& component : m_components)
	{
		if (component)
		{
			component->Update(*this, dt);
		}
	}
}

bool Object3D::intersectsRayMesh(glm::vec3 origin, glm::vec3& direction)
{
	return m_mesh->intersectsRay(origin, direction);
}

bool Object3D::skinned()
{
	return m_skinned && GetComponent<AnimatorComponent>() != nullptr;
}

bool Object3D::HasAnimatorComponent() const
{
	for (const auto& component : m_components)
	{
		if (dynamic_cast<AnimatorComponent*>(component.get()) != nullptr)
		{
			return true;
		}
	}
	return false;
}

std::string Object3D::Name()
{
	return m_name;
}

std::string Object3D::SourcePath()
{
	return m_sourcePath;
}

glm::vec3 Object3D::Scale()
{
	return m_scale;
}
