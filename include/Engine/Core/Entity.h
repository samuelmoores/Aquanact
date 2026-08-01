#pragma once

#include "Engine/Core/Component.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class AnimatorComponent;
class Controller;

class Entity
{
public:
	Entity(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
	Entity(const char* modelFile);
	explicit Entity(std::string name = "Entity");
	virtual ~Entity();

	const std::string& Name() const { return m_name; }
	void SetName(std::string name) { m_name = std::move(name); }

	virtual const char* TypeName() const { return "Entity"; }
	virtual std::vector<BindableMember> GetBindableMembers() const { return {}; }
	virtual bool TryGetBindableValue(const std::string&, float&) const { return false; }
	virtual std::vector<BindableEvent> GetBindableEvents() const { return {}; }
	virtual void startUp();
	virtual void FirstFrame() {}
	void FirstFrameComponents();

	Mesh* GetMesh();
	ShaderProgram* GetShader();
	AnimatorComponent* GetAnimatorComponent();
	Controller* GetController();
	std::vector<Component*> Components();
	std::vector<const Component*> Components() const;
	Component* GetComponentByName(const std::string& name);
	const Component* GetComponentByName(const std::string& name) const;

	template<typename T>
	T* GetComponent();
	template<typename T>
	const T* GetComponent() const;
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);
	bool RemoveComponent(Component* component);
	template<typename T>
	bool RemoveComponent();

	void UpdateComponents(float dt);
	void UpdateControllers(float dt);
	void UpdateNonControllerComponents(float dt);
	glm::mat4 BuildModelMatrix();
	void Rotate(glm::vec3 delta);
	void Move(glm::vec3 delta);
	void Translate(glm::vec3 delta);
	void Scale(glm::vec3 delta);
	void SetScale(glm::vec3 scale);
	void updateMeshAABB(glm::vec3 delta);
	bool intersectsRayMesh(glm::vec3 origin, glm::vec3& direction);
	bool skinned();

	std::string SourcePath() const;
	glm::vec3 Position() const;
	glm::vec3 WorldPosition();
	glm::vec3 WorldCenterPosition();
	glm::vec3 InitialWorldCenterPosition() const;
	glm::vec3 DefaultPosition() const;
	glm::vec3 DefaultRotation() const;
	glm::vec3 Rotation() const;
	glm::vec3 Scale() const;
	void SetRotation(glm::vec3 newRotation);
	void SetDefaultPosition(glm::vec3 position);
	void SetDefaultRotation(glm::vec3 rotation);
	void ResetToDefaultPosition();
	void ResetToDefaultRotation();

	void SetIgnoreCameraCollision(bool ignore) { m_ignoreCameraCollision = ignore; }
	bool IgnoreCameraCollision() const { return m_ignoreCameraCollision; }
	bool HasAnimatorComponent() const;

protected:
	Mesh* m_mesh = nullptr;
	ShaderProgram m_shader;
	glm::vec3 m_position{0.0f};
	glm::vec3 m_rotation{0.0f};
	glm::vec3 m_scale{1.0f};
	glm::vec3 m_initialWorldCenter{0.0f};
	glm::vec3 m_defaultPosition{0.0f};
	glm::vec3 m_defaultRotation{0.0f};
	bool m_skinned = false;
	bool m_ignoreCameraCollision = false;
	std::string m_name;
	std::string m_sourcePath;
	std::vector<std::unique_ptr<Component>> m_components;

public:
	static inline std::vector<Vertex3D> cubeVertices = {
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	};
	static inline std::vector<uint32_t> cubeFaces = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9,10, 8,10,11,
		12,13,14, 12,14,15,
		16,17,18, 16,18,19,
		20,21,22, 20,22,23
	};
};

template<typename T>
T* Entity::GetComponent()
{
	for (auto& component : m_components)
	{
		if (auto* typed = dynamic_cast<T*>(component.get()))
		{
			return typed;
		}
	}
	return nullptr;
}

template<typename T>
const T* Entity::GetComponent() const
{
	for (const auto& component : m_components)
	{
		if (auto* typed = dynamic_cast<T*>(component.get()))
		{
			return typed;
		}
	}
	return nullptr;
}

template<typename T, typename... Args>
T* Entity::AddComponent(Args&&... args)
{
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw = component.get();
	raw->SetOwner(this);
	m_components.push_back(std::move(component));
	return raw;
}

template<typename T>
bool Entity::RemoveComponent()
{
	for (auto it = m_components.begin(); it != m_components.end(); ++it)
	{
		if (dynamic_cast<T*>(it->get()))
		{
			m_components.erase(it);
			return true;
		}
	}
	return false;
}

