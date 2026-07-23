#include "Engine/Entity.h"

#include "Engine/AnimatorComponent.h"
#include "Engine/Controller.h"
#include "Engine/Debug.h"
#include "Engine/Globals.h"
#include "Engine/ModelImporter.h"

#include <glm/gtc/matrix_transform.hpp>
#include <utility>

Entity::Entity(std::string name)
	: m_name(std::move(name))
{
}

Entity::Entity(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_skinned = false;
	m_mesh = new Mesh(vertices, faces);
	m_shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	m_shader.activate();
	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
	m_initialWorldCenter = WorldCenterPosition();
}

Entity::Entity(const char* modelFile)
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
	{
		m_name = m_name.substr(lastSlash + 1);
	}
}

Entity::~Entity()
{
	delete m_mesh;
}

Mesh* Entity::GetMesh() { return m_mesh; }
ShaderProgram* Entity::GetShader() { return &m_shader; }
AnimatorComponent* Entity::GetAnimatorComponent() { return GetComponent<AnimatorComponent>(); }
Controller* Entity::GetController() { return GetComponent<Controller>(); }
void Entity::UpdateComponents(float dt) { for (auto& component : m_components) if (component) component->Update(*this, dt); }
glm::mat4 Entity::BuildModelMatrix()
{
	auto m = glm::translate(glm::mat4(1), m_position);
	m = glm::rotate(m, m_rotation[1], glm::vec3(0, 1, 0));
	m = glm::rotate(m, m_rotation[0], glm::vec3(1, 0, 0));
	m = glm::rotate(m, m_rotation[2], glm::vec3(0, 0, 1));
	return glm::scale(m, m_scale);
}
void Entity::Rotate(glm::vec3 delta) { m_rotation += delta; }
void Entity::Move(glm::vec3 delta) { m_position += delta; updateMeshAABB(delta); }
void Entity::Translate(glm::vec3 delta) { m_position += delta; updateMeshAABB(delta); }
void Entity::Scale(glm::vec3 delta) { m_scale += delta; }
void Entity::SetScale(glm::vec3 scale) { m_scale = scale; }
void Entity::updateMeshAABB(glm::vec3 delta) { if (m_mesh) m_mesh->updateAABB(delta, m_scale); }
bool Entity::intersectsRayMesh(glm::vec3 origin, glm::vec3& direction) { return m_mesh && m_mesh->intersectsRay(origin, direction); }
bool Entity::skinned() { return m_skinned && GetComponent<AnimatorComponent>() != nullptr; }
std::string Entity::SourcePath() const { return m_sourcePath; }
glm::vec3 Entity::Position() const { return m_position; }
glm::vec3 Entity::WorldPosition() { return glm::vec3(BuildModelMatrix()[3]); }
glm::vec3 Entity::WorldCenterPosition() { const glm::vec3 localCenter = m_mesh ? m_mesh->centerAABB() : glm::vec3(0.0f); return glm::vec3(BuildModelMatrix() * glm::vec4(localCenter, 1.0f)); }
glm::vec3 Entity::InitialWorldCenterPosition() const { return m_initialWorldCenter; }
glm::vec3 Entity::DefaultPosition() const { return m_defaultPosition; }
glm::vec3 Entity::Rotation() const { return m_rotation; }
glm::vec3 Entity::Scale() const { return m_scale; }
void Entity::SetRotation(glm::vec3 newRotation) { m_rotation = newRotation; }
void Entity::SetDefaultPosition(glm::vec3 position) { m_defaultPosition = position; }
void Entity::ResetToDefaultPosition()
{
	const glm::vec3 delta = m_defaultPosition - m_position;
	Translate(delta);
}
bool Entity::HasAnimatorComponent() const { return GetComponent<AnimatorComponent>() != nullptr; }
