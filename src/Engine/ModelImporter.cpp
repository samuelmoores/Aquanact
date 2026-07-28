#include "Engine/ModelImporter.h"

#include "Engine/Debug.h"
#include "Engine/Globals.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <filesystem>
#include <map>
#include <stdexcept>

namespace {
	const int VERTICES_PER_FACE = 3;

	void AddBoneData(Vertex3D& vertex, int boneID, float weight)
	{
		for (int i = 0; i < 4; i++)
		{
			if (vertex.boneIDs[i] == -1)
			{
				vertex.boneIDs[i] = boneID;
				vertex.weights[i] = weight;
				return;
			}
		}

		assert(0);
	}

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), ::tolower);
		return value;
	}

}

ImportedModel ModelImporter::Import(const std::string& path, bool flipUvs) const
{
	ImportedModel model;
	model.importer = std::make_unique<Assimp::Importer>();

	int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
	if (flipUvs)
	{
		flags |= aiProcess_FlipUVs;
	}

	model.scene = model.importer->ReadFile(path, flags | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
	if (!model.scene)
	{
		gDebug.LogMessage(std::string("ASSIMP ERROR: ") + model.importer->GetErrorString());
		throw std::runtime_error(model.importer->GetErrorString());
	}

	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> faces;
	int vertexOffset = 0;

	for (int i = 0; i < static_cast<int>(model.scene->mNumMeshes); ++i)
	{
		const aiMesh* mesh = model.scene->mMeshes[i];
		const int numVertices = static_cast<int>(mesh->mNumVertices);
		const int numBones = static_cast<int>(mesh->mNumBones);

		for (int v = 0; v < numVertices; ++v)
		{
			glm::vec3 position{ mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z };
			glm::vec2 texCoord{ 0.0f };
			if (mesh->mTextureCoords[0])
			{
				texCoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
			}

			glm::vec3 normal{ 0.0f };
			if (mesh->HasNormals())
			{
				normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };
			}

			glm::vec3 tangent{ 0.0f };
			if (mesh->HasTangentsAndBitangents())
			{
				tangent = { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
			}

			vertices.push_back({ position, texCoord, normal, tangent });
		}

		for (int f = 0; f < static_cast<int>(mesh->mNumFaces); ++f)
		{
			faces.push_back(mesh->mFaces[f].mIndices[0] + vertexOffset);
			faces.push_back(mesh->mFaces[f].mIndices[1] + vertexOffset);
			faces.push_back(mesh->mFaces[f].mIndices[2] + vertexOffset);
		}

		model.facesSize.push_back(static_cast<int>(mesh->mNumFaces * VERTICES_PER_FACE));

		model.skinned = model.skinned || mesh->mNumBones != 0;
		if (model.skinned)
		{
			std::map<std::string, int> boneMap = model.skeleton.boneMapping;
			for (int j = 0; j < numBones; ++j)
			{
				aiBone* bone = mesh->mBones[j];
				std::string boneName = bone->mName.C_Str();

				int boneID = 0;
				if (boneMap.find(boneName) == boneMap.end())
				{
					boneID = static_cast<int>(boneMap.size());
					boneMap[boneName] = boneID;
					model.skeleton.boneOffsetMatrices.push_back(bone->mOffsetMatrix);
				}
				else
				{
					boneID = boneMap.find(boneName)->second;
				}

				for (int k = 0; k < static_cast<int>(bone->mNumWeights); ++k)
				{
					const aiVertexWeight& vw = bone->mWeights[k];
					AddBoneData(vertices[vertexOffset + vw.mVertexId], boneID, vw.mWeight);
				}
			}

			model.skeleton.boneMapping = boneMap;
		}

		aiMaterial* mat = model.scene->mMaterials[mesh->mMaterialIndex];
		aiColor4D ambient(0.2f, 0.2f, 0.2f, 1.0f);
		aiColor4D specular(0.5f, 0.5f, 0.5f, 1.0f);
		float shininess = 32.0f;
		mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
		mat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
		mat->Get(AI_MATKEY_SHININESS, shininess);

		float specStrength = glm::length(glm::vec3(specular.r, specular.g, specular.b)) / glm::sqrt(3.0f);
		SubMeshMaterial submeshMat;
		submeshMat.phong = glm::vec4(1.0f, 1.0f, specStrength, glm::max(shininess, 1.0f));
		submeshMat.ambientColor = glm::vec3(ambient.r, ambient.g, ambient.b);
		submeshMat.directionalColor = glm::vec3(1.0f, 1.0f, 1.0f);
		submeshMat.directionalLight = glm::vec3(-1.0f, -1.0f, -1.0f);
		model.materials.push_back(submeshMat);

		vertexOffset += numVertices;
	}

	model.skeleton.finalTransformations.resize(model.skeleton.boneOffsetMatrices.size());
	model.vertices = std::move(vertices);
	model.faces = std::move(faces);

	const auto animDir = std::filesystem::path(path).parent_path() / "animations";
	const std::string modelStem = ToLower(std::filesystem::path(path).stem().string());
	if (model.skinned && std::filesystem::exists(animDir) && std::filesystem::is_directory(animDir))
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(animDir))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			std::string ext = ToLower(entry.path().extension().string());
			static const std::vector<std::string> modelExts = { ".fbx", ".obj", ".gltf", ".glb", ".dae" };
			if (std::find(modelExts.begin(), modelExts.end(), ext) == modelExts.end())
			{
				continue;
			}

			const std::string animationStem = ToLower(entry.path().stem().string());
			const std::string animationPrefix = modelStem + "_";
			if (animationStem != modelStem && animationStem.rfind(animationPrefix, 0) != 0)
			{
				continue;
			}

			std::string animPath = entry.path().string();
			auto animImporter = std::make_unique<Assimp::Importer>();
			const aiScene* animScene = animImporter->ReadFile(animPath, aiProcess_Triangulate);
			if (!animScene)
			{
				continue;
			}

			for (unsigned int i = 0; i < animScene->mNumAnimations; ++i)
			{
				model.animations.push_back(animScene->mAnimations[i]);
				model.animationSources.push_back(animPath);
			}
			model.animImporters.push_back(std::move(animImporter));
		}
	}

	const glm::vec3 minInit{ FLT_MAX };
	const glm::vec3 maxInit{ -FLT_MAX };
	model.minBounds = minInit;
	model.maxBounds = maxInit;
	for (const auto& vertex : model.vertices)
	{
		model.minBounds.x = std::min(model.minBounds.x, vertex.position.x);
		model.minBounds.y = std::min(model.minBounds.y, vertex.position.y);
		model.minBounds.z = std::min(model.minBounds.z, vertex.position.z);
		model.maxBounds.x = std::max(model.maxBounds.x, vertex.position.x);
		model.maxBounds.y = std::max(model.maxBounds.y, vertex.position.y);
		model.maxBounds.z = std::max(model.maxBounds.z, vertex.position.z);
	}
	model.meshMinBounds = model.minBounds;
	model.meshMaxBounds = model.maxBounds;
	model.sourcePath = path;
	return model;
}

