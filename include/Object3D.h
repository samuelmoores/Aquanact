#pragma once
#include <glm/glm.hpp>
#include <Mesh.h>
#include <ShaderProgram.h>

class Object3D {
public:
	Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
	Object3D(const char* modelFile, const char* textureFile);
	Object3D(const char* modelFile, const char* textureFile, const char* normalMap);
	Mesh* GetMesh();
	ShaderProgram* GetShader();
	glm::mat4 BuildModelMatrix();
	void Rotate(glm::vec3 delta);
	void Move(glm::vec3 delta);
	void Scale(glm::vec3 delta);
	void SetScale(glm::vec3 scale);
	void updateMeshAABB();
	bool intersectsRayMesh(glm::vec3 origin, glm::vec3& direction);
private:
	Mesh m_mesh;
	ShaderProgram m_shader;
	glm::vec3 m_position;
	glm::vec3 m_rotation;
	glm::vec3 m_scale;
};
