#pragma once
#include <vector>
#include <glad/glad.h>
#include <StbImage.h>
#include <ShaderProgram.h>
#include <Camera.h>

struct Vertex3D {
	glm::vec3 position;   // offset 0  (3 floats)
	glm::vec2 texCoord;   // offset 12 (2 floats)
	glm::vec3 normal;     // offset 20 (3 floats)
	glm::vec3 tangent;    // offset 32 (3 floats)
	glm::vec3 bitangent;  // offset 44 (3 floats)
};

class Mesh {
	public:
		Mesh();
		Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		Mesh(const char* modelFile, const char* textureFile);
		Mesh(const char* modelFile, const char* textureFile, const char* normalMap);
		void assimpLoad(const std::string& path, bool flipUvs);
		void SetBuffers();
		void SetTexture(const char* colorFile);
		void SetNormalMap(const char* normalMap);
		void Bind() const;
		void UnBind() const;
		uint32_t FacesSize() const;
	private:
		std::vector<Vertex3D> m_vertices;
		std::vector<uint32_t> m_faces;
		uint32_t m_vao;
		uint32_t m_textureColor;
		uint32_t m_textureNormal;
};
