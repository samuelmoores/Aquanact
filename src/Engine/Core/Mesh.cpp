#include <algorithm>
#include <cassert>
#include <cfloat>
#include <iomanip>
#include <stdexcept>
#include <filesystem>

#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Line.h"
#include "Engine/Core/ModelImporter.h"
#include "Engine/Core/Mesh.h"
#include <stb_image.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

namespace {
	const int VERTICES_PER_FACE = 3;

	uint32_t CreateTextureFromPixels(int width, int height, const unsigned char* pixels, bool mipmapped)
	{
		uint32_t textureId = 0;
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		if (mipmapped)
			glGenerateMipmap(GL_TEXTURE_2D);
		return textureId;
	}

}

Mesh::Mesh()
{
	m_currVao = 0;
	m_currTextureColor = 0;
	m_scene = nullptr;
	m_skinned = false;
	m_minBounds = glm::vec3(0.0f);
	m_maxBounds = glm::vec3(0.0f);
	m_meshMinBounds = glm::vec3(0.0f);
	m_meshMaxBounds = glm::vec3(0.0f);
}

Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
	: Mesh()
{
	m_vertices = std::move(vertices);
	m_faces = std::move(faces);

	m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
	m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);
	m_meshMinBounds = m_minBounds;
	m_meshMaxBounds = m_maxBounds;

	SetBuffers(m_vertices, m_faces);
	SetTexture("models/brick_wall_diff.png");
}

Mesh::Mesh(ImportedModel&& importedModel)
	: Mesh()
{
	AdoptImportedModel(std::move(importedModel));
}

void Mesh::AdoptImportedModel(ImportedModel&& importedModel)
{
	// Animation bridge state kept until the animator stops using Assimp node types.
	m_importer = std::move(importedModel.importer); // Owns the imported scene lifetime.
	m_animImporters = std::move(importedModel.animImporters); // Keeps sibling animation scenes alive.
	m_scene = importedModel.scene; // Temporary bridge for AnimatorComponent::GetRootNode().
	m_skeleton = std::move(importedModel.skeleton); // Bone mapping and final transforms used by Animator.
	m_animations = std::move(importedModel.animations); // Temporary bridge until animation data is engine-owned.
	m_animationSources = std::move(importedModel.animationSources); // Used to name animation states in AnimatorComponent.

	// Mesh payload used directly by rendering and object setup.
	m_skinned = importedModel.skinned; // Cached so Entity can decide whether to attach AnimatorComponent.
	m_vertices = std::move(importedModel.vertices); // Final imported vertex buffer for GPU upload.
	m_faces = std::move(importedModel.faces); // Final imported index buffer for GPU upload.
	m_facesSize = std::move(importedModel.facesSize); // One index-count entry per imported submesh.
	m_materials = std::move(importedModel.materials); // Per-submesh material parameters.
	m_faceOffsets.clear();
	int runningOffset = 0;
	for (int faceCount : m_facesSize)
	{
		m_faceOffsets.push_back(runningOffset);
		runningOffset += faceCount;
	}

	// Bounds used by selection, ray tests, and world-space interaction.
	m_minBounds = importedModel.minBounds; // Imported object-space bounds.
	m_maxBounds = importedModel.maxBounds; // Imported object-space bounds.
	m_meshMinBounds = importedModel.meshMinBounds; // Runtime AABB min for object picking/collision.
	m_meshMaxBounds = importedModel.meshMaxBounds; // Runtime AABB max for object picking/collision.
	m_sourcePath = std::move(importedModel.sourcePath);

	const auto invalid = [](float v)
	{
		return !std::isfinite(v);
	};
	const bool hasValidBounds =
		!invalid(m_minBounds.x) && !invalid(m_minBounds.y) && !invalid(m_minBounds.z) &&
		!invalid(m_maxBounds.x) && !invalid(m_maxBounds.y) && !invalid(m_maxBounds.z) &&
		m_minBounds.x <= m_maxBounds.x &&
		m_minBounds.y <= m_maxBounds.y &&
		m_minBounds.z <= m_maxBounds.z;
	if (!hasValidBounds)
	{
		m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
		m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);
		m_meshMinBounds = m_minBounds;
		m_meshMaxBounds = m_maxBounds;
	}

	if (m_scene)
	{
		for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
		{
			aiMaterial* mat = m_scene->mMaterials[m_scene->mMeshes[i]->mMaterialIndex];
			LoadMaterialTextures(mat);
		}
	}

	if (!m_vertices.empty() && !m_faces.empty())
	{
		SetBuffers(m_vertices, m_faces);
	}
}

glm::vec3 Mesh::minBounds()
{
	return m_meshMinBounds;
}

glm::vec3 Mesh::maxBounds()
{
	return m_meshMaxBounds;
}

void Mesh::DrawBoundingBox()
{
	Line line(m_meshMinBounds, m_meshMaxBounds);
	line.UpdateProjection(Root::Current().Render().GetEngineCamera().GetProjectionMatrix());
	line.draw(Root::Current().Render().GetEngineCamera().GetViewMatrix());
}

void Mesh::updateAABB(glm::vec3 position, glm::vec3 scale)
{
	m_meshMinBounds += position;
	m_meshMaxBounds += position;

	for (int i = 0; i < 3; ++i)
	{
		if (m_minBounds[i] > m_maxBounds[i])
			std::swap(m_minBounds[i], m_maxBounds[i]);
	}
	(void)scale;
}

glm::vec3 Mesh::centerAABB()
{
	auto invalid = [](float v) { return !std::isfinite(v); };
	if (invalid(m_minBounds.x) || invalid(m_minBounds.y) || invalid(m_minBounds.z) ||
		invalid(m_maxBounds.x) || invalid(m_maxBounds.y) || invalid(m_maxBounds.z) ||
		m_minBounds.x > m_maxBounds.x || m_minBounds.y > m_maxBounds.y || m_minBounds.z > m_maxBounds.z)
	{
		Root::Current().Debugger().LogTagged(
			Debug::Severity::Warning,
			"Mesh",
			"Invalid AABB encountered. min=(" +
				std::to_string(m_minBounds.x) + "," + std::to_string(m_minBounds.y) + "," + std::to_string(m_minBounds.z) +
				") max=(" +
				std::to_string(m_maxBounds.x) + "," + std::to_string(m_maxBounds.y) + "," + std::to_string(m_maxBounds.z) +
				")");
		return glm::vec3(0.0f);
	}

	return {
		(m_minBounds.x + m_maxBounds.x) / 2.0f,
		(m_minBounds.y + m_maxBounds.y) / 2.0f,
		(m_minBounds.z + m_maxBounds.z) / 2.0f
	};
}

glm::vec3 Mesh::dimensionAABB()
{
	return {
		m_maxBounds.x - m_minBounds.x,
		m_maxBounds.y - m_minBounds.y,
		m_maxBounds.z - m_minBounds.z
	};
}

bool Mesh::intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const
{
	float tMin = 0.0f;
	float tMax = 1e6f;

	for (int i = 0; i < 3; ++i)
	{
		if (std::abs(rayDir[i]) < 1e-6f)
		{
			if (rayOrigin[i] < m_minBounds[i] || rayOrigin[i] > m_maxBounds[i])
				return false;
		}
		else
		{
			float ood = 1.0f / rayDir[i];
			float t1 = (m_minBounds[i] - rayOrigin[i]) * ood;
			float t2 = (m_maxBounds[i] - rayOrigin[i]) * ood;
			if (t1 > t2) std::swap(t1, t2);

			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax)
				return false;
		}
	}

	return true;
}

bool Mesh::SphereAABBOverlap(const glm::vec3& center, float radius)
{
	glm::vec3 closest;
	closest.x = std::clamp(center.x, m_meshMinBounds.x, m_meshMaxBounds.x);
	closest.y = std::clamp(center.y, m_meshMinBounds.y, m_meshMaxBounds.y);
	closest.z = std::clamp(center.z, m_meshMinBounds.z, m_meshMaxBounds.z);
	return glm::length2(center - closest) < radius * radius;
}

bool Mesh::RayHit(const glm::vec3& ro, const glm::vec3& rd, float& tHit)
{
	glm::vec3 invDir = 1.0f / rd;
	glm::vec3 t0 = (m_meshMinBounds - ro) * invDir;
	glm::vec3 t1 = (m_meshMaxBounds - ro) * invDir;
	glm::vec3 tmin = glm::min(t0, t1);
	glm::vec3 tmax = glm::max(t0, t1);

	float tNear = std::max({ tmin.x, tmin.y, tmin.z });
	float tFar = std::min({ tmax.x, tmax.y, tmax.z });
	if (tNear > tFar || tFar < 0.0f)
		return false;

	tHit = tNear >= 0.0f ? tNear : tFar;
	return true;
}

const Skeleton& Mesh::GetSkeleton() const
{
	return m_skeleton;
}

Skeleton* Mesh::GetSkeletonPtr()
{
	return &m_skeleton;
}

const SubMeshMaterial& Mesh::GetMaterial(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_materials.size()))
		return s_defaultMaterial;
	return m_materials[index];
}

bool Mesh::HasNormalTexture(int index) const
{
	return index >= 0 &&
		index < static_cast<int>(m_textureNormal.size()) &&
		m_textureNormal[index] != 0;
}

bool Mesh::HasColorTexture(int index) const
{
	return index >= 0 &&
		index < static_cast<int>(m_textureColor.size()) &&
		m_textureColor[index] != 0;
}

bool Mesh::HasSpecularTexture(int index) const
{
	return index >= 0 &&
		index < static_cast<int>(m_textureSpecular.size()) &&
		m_textureSpecular[index] != 0;
}

uint32_t Mesh::FacesOffset(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_faceOffsets.size()))
		return 0;
	return static_cast<uint32_t>(m_faceOffsets[index]);
}

void Mesh::SetAmbientColor(int index, glm::vec3 color)
{
	if (index >= 0 && index < static_cast<int>(m_materials.size()))
		m_materials[index].ambientColor = color;
}

bool Mesh::Skinned()
{
	return m_skinned;
}

int Mesh::NumBuffers() const
{
	return static_cast<int>(std::min(m_facesSize.size(), m_materials.size()));
}

int Mesh::NumAnimations() const
{
	return static_cast<int>(m_animations.size());
}

aiAnimation* Mesh::GetAnimation(int i) const
{
	return m_animations[i];
}

const std::string& Mesh::GetAnimationSource(int i) const
{
	return m_animationSources[i];
}

const aiNode* Mesh::GetRootNode() const
{
	return m_scene ? m_scene->mRootNode : nullptr;
}

void Mesh::SetBuffers(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_vao.push_back(0);
	glGenVertexArrays(1, &m_vao.back());
	glBindVertexArray(m_vao.back());

	uint32_t vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), vertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)12);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)20);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)32);
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(4, 4, GL_INT, sizeof(Vertex3D), (void*)44);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)60);
	glEnableVertexAttribArray(5);

	uint32_t ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(uint32_t), faces.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);
	m_facesSize.push_back(static_cast<int>(faces.size()));
}

void Mesh::SetTexture(const char* colorFile)
{
	LoadTextureFile(TextureSlot::Diffuse, colorFile);
}

std::vector<uint32_t>& Mesh::TexturesForSlot(TextureSlot slot)
{
	switch (slot)
	{
	case TextureSlot::Diffuse:
		return m_textureColor;
	case TextureSlot::Specular:
		return m_textureSpecular;
	case TextureSlot::Normal:
		return m_textureNormal;
	}
	return m_textureColor;
}

void Mesh::LoadTextureFile(TextureSlot slot, const std::filesystem::path& texturePath)
{
	StbImage image;
	image.loadFromFile(texturePath.string().c_str());
	TexturesForSlot(slot).push_back(CreateTextureFromPixels(image.getWidth(), image.getHeight(), image.getData(), true));
}

void Mesh::LoadTextureMemory(TextureSlot slot, aiTexture* texture)
{
	StbImage image;
	image.loadFromMemory(texture);
	TexturesForSlot(slot).push_back(CreateTextureFromPixels(image.getWidth(), image.getHeight(), image.getData(), true));
}

Mesh::TextureSlot Mesh::SlotForTextureType(aiTextureType textureType) const
{
	if (textureType == aiTextureType_SPECULAR)
		return TextureSlot::Specular;
	if (textureType == aiTextureType_NORMALS)
		return TextureSlot::Normal;
	return TextureSlot::Diffuse;
}

aiReturn Mesh::GetMaterialTexturePath(aiMaterial* mat, aiTextureType textureType, aiString& texturePath) const
{
	aiReturn result = mat->GetTexture(textureType, 0, &texturePath);
	if (result != AI_SUCCESS && textureType == aiTextureType_DIFFUSE)
		result = mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);
	if (result != AI_SUCCESS && textureType == aiTextureType_NORMALS)
		result = mat->GetTexture(aiTextureType_HEIGHT, 0, &texturePath);
	return result;
}

void Mesh::LoadMaterialTextures(aiMaterial* mat)
{
	LoadTexture(mat, aiTextureType_DIFFUSE); // Fallback to BASE_COLOR.
	LoadTexture(mat, aiTextureType_SPECULAR);
	LoadTexture(mat, aiTextureType_NORMALS); // Fallback to HEIGHT/bump maps.
}

void Mesh::LoadTexture(aiMaterial* mat, aiTextureType textureType)
{
	const TextureSlot slot = SlotForTextureType(textureType);
	aiString texturePath;
	const aiReturn result = GetMaterialTexturePath(mat, textureType, texturePath);

	if (result != AI_SUCCESS || texturePath.length == 0)
	{
		TexturesForSlot(slot).push_back(0);
		return;
	}

	const aiTexture* embeddedTexture = m_scene ? m_scene->GetEmbeddedTexture(texturePath.C_Str()) : nullptr;
	if (!embeddedTexture || !embeddedTexture->pcData || embeddedTexture->mHeight != 0)
	{
		TexturesForSlot(slot).push_back(0);
		return;
	}

	LoadTextureMemory(slot, const_cast<aiTexture*>(embeddedTexture));
}

SubMeshMaterial Mesh::s_defaultMaterial = {
	glm::vec4(0.3f, 0.7f, 0.1f, 8.0f),
	glm::vec3(0.2f, 0.2f, 0.2f),
	glm::vec3(1.0f, 1.0f, 1.0f),
	glm::vec3(-1.0f, -1.0f, -1.0f)
};

void Mesh::Bind(int index)
{
	glBindVertexArray(m_vao[0]);
	glActiveTexture(GL_TEXTURE0);
	if (index < static_cast<int>(m_textureColor.size()) && m_textureColor[index] != 0)
		glBindTexture(GL_TEXTURE_2D, m_textureColor[index]);

	glActiveTexture(GL_TEXTURE1);
	if (index < static_cast<int>(m_textureSpecular.size()) && m_textureSpecular[index] != 0)
		glBindTexture(GL_TEXTURE_2D, m_textureSpecular[index]);

	glActiveTexture(GL_TEXTURE2);
	if (index < static_cast<int>(m_textureNormal.size()) && m_textureNormal[index] != 0)
		glBindTexture(GL_TEXTURE_2D, m_textureNormal[index]);
}

void Mesh::UnBind()
{
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

uint32_t Mesh::FacesSize(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_facesSize.size()))
		return 0;
	return static_cast<uint32_t>(m_facesSize[index]);
}



