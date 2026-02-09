#pragma once
#include <glm/glm.hpp>
#include <Mesh.h>
#include <ShaderProgram.h>
#include <Animation.h>



class Object3D {
public:
	Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
	Object3D(char modelFile[]);
	~Object3D();
	Mesh* GetMesh();
	ShaderProgram* GetShader();
	glm::mat4 BuildModelMatrix();
	void Rotate(glm::vec3 delta);
	void Move(glm::vec3 delta);
	void Scale(glm::vec3 delta);
	void SetScale(glm::vec3 scale);
	void updateMeshAABB(glm::vec3 delta);
	bool intersectsRayMesh(glm::vec3 origin, glm::vec3& direction);
	bool skinned();
	std::string Name();
	glm::vec3 Position();
	glm::vec3 Rotation();
	void SetRotation(glm::vec3 newRotation);
	float BlendFactor();
	void IncBlendFactor(float delta);
	void StartAnimBlend();

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

	//line

private:
	Mesh* m_mesh;
	ShaderProgram m_shader;
	glm::vec3 m_position;
	glm::vec3 m_rotation;
	glm::vec3 m_scale;
	bool m_skinned;
	std::string m_name;
	float m_blendFactor;
	std::vector<Animation*> m_animations;
};
