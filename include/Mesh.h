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
	glm::vec3 tangent;    // offset 32 (3 floats)

	int boneIDs[4] = {0, 0, 0, 0};         // 16 bytes (4 ints)
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };       // 16 bytes (4 floats)
};

struct Skeleton {
	std::map<std::string, int> boneMapping;
	std::vector<aiMatrix4x4> boneOffsetMatrices;
	std::vector<aiMatrix4x4> finalTransformations;
};

static int numBoneUpdates = 0;

class Mesh {
	public:
		Mesh();
		Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		Mesh(char modelFile[]);
		
		//loading
		void assimpLoad(const std::string& path, bool flipUvs);
		void fromAssimpMesh(const aiMesh* mesh, std::vector<Vertex3D>& vertices, std::vector<uint32_t>& faces);
		void ReadNodeHeirarchy(const aiNode* node, const aiMatrix4x4& ParentTransform);
		void LoadTexture(aiMaterial* mat, aiTextureType textureType, std::string path);

		//open gl
		void Bind();
		void UnBind();
		uint32_t FacesSize() const;
		
		//bounding box
		void updateAABB(glm::vec3 position, glm::vec3 scale);
		glm::vec3 centerAABB();
		glm::vec3 dimensionAABB();
		bool intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;
		glm::vec3 minBounds();
		glm::vec3 maxBounds();
		void DrawBoundingBox();
		bool SphereAABBOverlap(const glm::vec3& center, float radius);

		//getter setter
		void SetBuffers(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
		void SetTexture(const char* colorFile);
		void SetDiffuseTextureMemory(aiTexture* text);
		const Skeleton& GetSkeleton() const;
		bool Skinned();
		void SetNextAnim(int nextAnim);
		void SetCurrentAnim(int currAnim);
		int GetNextAnim();
		int NumBuffers();
		void ClearBufferIndex();
		bool RayHit(const glm::vec3& ro, const glm::vec3& rd, float& tHit);

		//animation
		void SetAnim(int animIndex);
		void ReadNodeHeirarchy(float animTimeTicks, const aiNode* node, const aiMatrix4x4& ParentTransform, int animIndex);
		void RunAnimation(float animTime);
		void BlendAnimation(int nextAnim, float animTime, float blendFactor);
		void ReadNodeHeirarchyBlend(float blendFactor, float animTimeTicks_Start, float animTimeTicks_End, const aiNode* node, const aiMatrix4x4& ParentTransform, aiAnimation* start, aiAnimation* end, int animIndex);
		const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const std::string& NodeName);
		int FindScaling(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedScaling(aiVector3D& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);
		int FindRotation(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);
		int FindPosition(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);
		void CalcInterpolatedPosition(aiVector3D& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim);

	private:
		std::vector<Vertex3D> m_vertices;
		std::vector<uint32_t> m_faces;
		std::vector<uint32_t> m_vao;
		std::vector<uint32_t> m_textureColor;
		int m_currVao;
		int m_currTextureColor;
		glm::vec3 m_minBounds;
		glm::vec3 m_maxBounds;
		glm::vec3 m_meshMinBounds;
		glm::vec3 m_meshMaxBounds;
		Skeleton m_skeleton;
		Assimp::Importer m_importer;
		const aiScene* m_scene;
		aiMatrix4x4 m_GlobalInverseTransform;
		bool m_skinned;
		int m_currentAnim;
		int m_nextAnim;
		int m_totalVertices;
		std::vector<Mesh> m_subMeshes;
		std::vector<int> m_facesSize;
		std::vector<aiAnimation*> m_animations;
};
