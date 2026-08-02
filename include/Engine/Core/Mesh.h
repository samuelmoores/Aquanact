#pragma once
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <filesystem>
#include "Engine/Core/StbImage.h"
#include "Engine/Core/ShaderProgram.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>

struct ImportedModel;

struct Vertex3D {
	glm::vec3 position;   // offset 0  (3 floats)
	glm::vec2 texCoord;   // offset 12 (2 floats)
	glm::vec3 normal;     // offset 20 (3 floats)
	glm::vec3 tangent;    // offset 32 (3 floats)

	int boneIDs[4] = {-1, -1, -1, -1};
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct Skeleton {
	std::map<std::string, int> boneMapping;
	std::vector<aiMatrix4x4> boneOffsetMatrices;
	std::vector<aiMatrix4x4> finalTransformations;
};

struct SubMeshMaterial {
	glm::vec4 phong;
	glm::vec3 ambientColor;
	glm::vec3 directionalColor;
	glm::vec3 directionalLight;
};

class Mesh {
	public:
		Mesh();
		Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		Mesh(ImportedModel&& importedModel);

		//open gl
		void Bind(int index = 0);
		void UnBind();
		uint32_t FacesSize(int index) const;

		//bounding box
		void updateAABB(glm::vec3 position, glm::vec3 scale);
		glm::vec3 centerAABB();
		glm::vec3 dimensionAABB();
		bool intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;
		glm::vec3 minBounds();
		glm::vec3 maxBounds();
		const glm::vec3& LocalMinBounds() const { return m_minBounds; }
		const glm::vec3& LocalMaxBounds() const { return m_maxBounds; }
		void DrawBoundingBox();
		bool SphereAABBOverlap(const glm::vec3& center, float radius);
		bool RayHit(const glm::vec3& ro, const glm::vec3& rd, float& tHit);

		//getter setter
		void SetBuffers(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		void SetTexture(const char* colorFile);
		void LoadTexture(aiMaterial* mat, aiTextureType textureType);
		const Skeleton& GetSkeleton() const;
		Skeleton* GetSkeletonPtr();
		const SubMeshMaterial& GetMaterial(int index) const;
		bool HasNormalTexture(int index) const;
		uint32_t FacesOffset(int index) const;
		void SetAmbientColor(int index, glm::vec3 color);
		bool Skinned();
		int NumBuffers() const;

		//animation data access (for Animator setup)
		int NumAnimations() const;
		aiAnimation* GetAnimation(int i) const;
		const std::string& GetAnimationSource(int i) const;
		const aiNode* GetRootNode() const;



	private:
		enum class TextureSlot {
			Diffuse,
			Specular,
			Normal
		};

		void AdoptImportedModel(ImportedModel&& importedModel);
		void LoadMaterialTextures(aiMaterial* mat);
		void LoadTextureFile(TextureSlot slot, const std::filesystem::path& texturePath);
		void LoadTextureMemory(TextureSlot slot, aiTexture* texture);
		TextureSlot SlotForTextureType(aiTextureType textureType) const;
		aiReturn GetMaterialTexturePath(aiMaterial* mat, aiTextureType textureType, aiString& texturePath) const;
		std::vector<uint32_t>& TexturesForSlot(TextureSlot slot);
		std::vector<Vertex3D> m_vertices;
		std::vector<uint32_t> m_faces;
		std::vector<uint32_t> m_vao;
		std::vector<uint32_t> m_textureColor;
		std::vector<uint32_t> m_textureSpecular;
		std::vector<uint32_t> m_textureNormal;
		int m_currVao;
		int m_currTextureColor;
		glm::vec3 m_minBounds;
		glm::vec3 m_maxBounds;
		glm::vec3 m_meshMinBounds;
		glm::vec3 m_meshMaxBounds;
		Skeleton m_skeleton;
		std::unique_ptr<Assimp::Importer> m_importer;
		std::vector<std::unique_ptr<Assimp::Importer>> m_animImporters;
		const aiScene* m_scene;
		bool m_skinned;
		std::vector<int> m_facesSize;
		std::vector<int> m_faceOffsets;
		std::vector<aiAnimation*> m_animations;
		std::vector<std::string> m_animationSources;
		std::vector<SubMeshMaterial> m_materials;
		std::string m_sourcePath;
		static SubMeshMaterial s_defaultMaterial;

};


