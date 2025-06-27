#pragma once
#include <vector>
#include <glad/glad.h>
#include <StbImage.h>
#include <ShaderProgram.h>
#include <Camera.h>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Vertex3D {
	glm::vec3 position;   // offset 0  (3 floats)
	glm::vec2 texCoord;   // offset 12 (2 floats)
	glm::vec3 normal;     // offset 20 (3 floats)
	glm::vec3 tangent;    // offset 32 (3 floats)
	glm::vec3 bitangent;  // offset 44 (3 floats)

	int boneIDs[4];         // 16 bytes (4 ints)
	float weights[4];       // 16 bytes (4 floats)
};

class Mesh {
	public:
		Mesh();
		Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		Mesh(const char* modelFile, const char* textureFile);
		Mesh(const char* modelFile, const char* textureFile, const char* normalMap);
		void assimpLoad(const std::string& path, bool flipUvs);
		void fromAssimpMesh(const aiMesh* mesh, std::vector<Vertex3D>& vertices, std::vector<uint32_t>& faces);
		void SetBuffers();
		void SetTexture(const char* colorFile);
		void SetNormalMap(const char* normalMap);
		void Bind() const;
		void UnBind() const;
		uint32_t FacesSize() const;
		void updateAABB(glm::vec3 position, glm::vec3 scale);
		bool intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;
	private:
		std::vector<Vertex3D> m_vertices;
		std::vector<uint32_t> m_faces;
		uint32_t m_vao;
		uint32_t m_textureColor;
		uint32_t m_textureNormal;
		glm::vec3 m_minBounds;
		glm::vec3 m_maxBounds;
		glm::vec3 m_meshMinBounds;
		glm::vec3 m_meshMaxBounds;
		std::map<std::string, int> m_boneMapping;
		std::vector<aiMatrix4x4> m_boneOffsetMatrices;
		std::vector<glm::mat4> m_finalBoneMatrices;
};
