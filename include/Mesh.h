#pragma once
#include <iostream>
#include <map>
#include <vector>
#include <StbImage.h>
#include <ShaderProgram.h>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex3D {
	glm::vec3 position;   // offset 0  (3 floats)
	glm::vec2 texCoord;   // offset 12 (2 floats)
	glm::vec3 normal;     // offset 20 (3 floats)

	int boneIDs[4];         // 16 bytes (4 ints)
	float weights[4];       // 16 bytes (4 floats)
};

struct Skeleton {
	std::map<std::string, int> boneMapping;
	std::vector<aiMatrix4x4> boneOffsetMatrices;
	std::vector<aiMatrix4x4> finalTransformations;
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
		const Skeleton& GetSkeleton() const;
		void ReadNodeHeirarchy(const aiNode* node, const aiMatrix4x4& ParentTransform);
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
		Skeleton m_skeleton;
		Assimp::Importer m_importer;
		const aiScene* m_scene;
};
