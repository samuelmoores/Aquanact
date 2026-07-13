#pragma once
#include <glm/glm.hpp>
#include <Mesh.h>
#include <ShaderProgram.h>
#include <memory>
#include <vector>
#include <utility>

#include "Component.h"

class AnimatorComponent;



class Object3D {
public:
	Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
	Object3D(const char* modelFile);
	~Object3D();
	Mesh* GetMesh();
	ShaderProgram* GetShader();
	AnimatorComponent* GetAnimatorComponent();
	template<typename T>
	T* GetComponent();
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);
	void UpdateComponents(float dt);
	glm::mat4 BuildModelMatrix();
	void Rotate(glm::vec3 delta);
	void Move(glm::vec3 delta);
	void Translate(glm::vec3 delta);
	void Scale(glm::vec3 delta);
	void SetScale(glm::vec3 scale);
	void updateMeshAABB(glm::vec3 delta);
	bool intersectsRayMesh(glm::vec3 origin, glm::vec3& direction);
	bool skinned();
	std::string Name();
	std::string SourcePath();
	glm::vec3 Position();
	glm::vec3 WorldPosition();
	glm::vec3 WorldCenterPosition();
	glm::vec3 InitialWorldCenterPosition() const;
	glm::vec3 Rotation();
	glm::vec3 Scale();
	void SetRotation(glm::vec3 newRotation);

	void SetIgnoreCameraCollision(bool ignore) { m_ignoreCameraCollision = ignore; }
	bool IgnoreCameraCollision() const { return m_ignoreCameraCollision; }
	bool HasAnimatorComponent() const;


	//primitives

	//cube
	static inline std::vector<Vertex3D> cubeVertices = {
		// back face (z = -0.5)
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },

		// front face (z = +0.5)
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },

		// left face (x = -0.5)
		{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },

		// right face (x = +0.5)
		{ { 0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },

		// bottom face (y = -0.5)
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },

		// top face (y = +0.5)
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	};
	static inline std::vector<uint32_t> cubeFaces = {
		// back face
		0, 1, 2,
		0, 2, 3,

		// front face
		4, 5, 6,
		4, 6, 7,

		// left face
		8, 9,10,
		8,10,11,

		// right face
		12,13,14,
		12,14,15,

		// bottom face
		16,17,18,
		16,18,19,

		// top face
		20,21,22,
		20,22,23
	};

private:
	Mesh* m_mesh;
	ShaderProgram m_shader;
	glm::vec3 m_position;
	glm::vec3 m_rotation;
	glm::vec3 m_scale;
	glm::vec3 m_initialWorldCenter;
	bool m_skinned;
	bool m_ignoreCameraCollision = false;
	std::string m_name;
	std::string m_sourcePath;
	std::vector<std::unique_ptr<Component>> m_components;
};

template<typename T>
T* Object3D::GetComponent()
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

template<typename T, typename... Args>
T* Object3D::AddComponent(Args&&... args)
{
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw = component.get();
	m_components.push_back(std::move(component));
	return raw;
}
