#include "Engine/Core/ModelImporter.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Root.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cctype>
#include <filesystem>
#include <fstream>
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

	bool IsRigControlNode(const aiNode* node)
	{
		std::string name = node && node->mName.length > 0 ? node->mName.C_Str() : "";
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return name.find("_ctl") != std::string::npos
			|| name.find("_ik") != std::string::npos
			|| name.find("_fk") != std::string::npos
			|| name.find("control") != std::string::npos;
	}

	void FindRigControlMeshReferences(const aiNode* node, const aiScene* scene,
		std::vector<bool>& referencedByControl, std::vector<bool>& referencedOutsideControl)
	{
		if (!node)
			return;

		const bool isControlNode = IsRigControlNode(node);
		for (unsigned int index = 0; index < node->mNumMeshes; ++index)
		{
			const unsigned int meshIndex = node->mMeshes[index];
			if (meshIndex >= scene->mNumMeshes)
				continue;
			if (isControlNode)
				referencedByControl[meshIndex] = true;
			else
				referencedOutsideControl[meshIndex] = true;
		}

		for (unsigned int index = 0; index < node->mNumChildren; ++index)
			FindRigControlMeshReferences(node->mChildren[index], scene,
				referencedByControl, referencedOutsideControl);
	}

	std::vector<bool> RigControlMeshesToIgnore(const aiScene* scene)
	{
		std::vector<bool> ignore(scene ? scene->mNumMeshes : 0, false);
		if (!scene || !scene->mRootNode)
			return ignore;

		std::vector<bool> referencedByControl(scene->mNumMeshes, false);
		std::vector<bool> referencedOutsideControl(scene->mNumMeshes, false);
		FindRigControlMeshReferences(scene->mRootNode, scene,
			referencedByControl, referencedOutsideControl);
		for (unsigned int index = 0; index < scene->mNumMeshes; ++index)
		{
			const aiMesh* mesh = scene->mMeshes[index];
			ignore[index] = std::string(mesh->mName.C_Str()) == "RootNode"
				&& mesh->mNumBones == 0
				&& referencedByControl[index]
				&& !referencedOutsideControl[index];
		}
		return ignore;
	}

	void WriteNodeMeshReferences(std::ofstream& log, const aiNode* node,
		const std::string& parentPath, const aiScene* scene)
	{
		if (!node)
			return;

		const std::string nodeName = node->mName.length > 0 ? node->mName.C_Str() : "<unnamed>";
		const std::string nodePath = parentPath.empty() ? nodeName : parentPath + "/" + nodeName;
		for (unsigned int index = 0; index < node->mNumMeshes; ++index)
		{
			const unsigned int meshIndex = node->mMeshes[index];
			if (meshIndex >= scene->mNumMeshes)
				continue;

			const aiMatrix4x4& transform = node->mTransformation;
			log << "  Node reference: mesh=" << meshIndex << " path=" << nodePath
				<< " localTransform=["
				<< transform.a1 << ',' << transform.a2 << ',' << transform.a3 << ',' << transform.a4 << ';'
				<< transform.b1 << ',' << transform.b2 << ',' << transform.b3 << ',' << transform.b4 << ';'
				<< transform.c1 << ',' << transform.c2 << ',' << transform.c3 << ',' << transform.c4 << ';'
				<< transform.d1 << ',' << transform.d2 << ',' << transform.d3 << ',' << transform.d4 << "]\n";
		}

		for (unsigned int index = 0; index < node->mNumChildren; ++index)
			WriteNodeMeshReferences(log, node->mChildren[index], nodePath, scene);
	}

	void WriteMultiMeshImportLog(const std::string& sourcePath, const aiScene* scene)
	{
		if (!scene || scene->mNumMeshes == 1)
			return;

		try
		{
			const std::filesystem::path logPath =
				Root::Current().FileSystemRef().ExecutableDirectory() / "logs" / "aquanact-importer.txt";
			std::filesystem::create_directories(logPath.parent_path());
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (!log.is_open())
				return;
			const std::vector<bool> ignoredControlMeshes = RigControlMeshesToIgnore(scene);

			log << "\n=== FBX import with " << scene->mNumMeshes << " meshes ===\n"
				<< "Source: " << sourcePath << '\n'
				<< "Materials: " << scene->mNumMaterials << '\n';
			for (unsigned int index = 0; index < scene->mNumMeshes; ++index)
			{
				const aiMesh* mesh = scene->mMeshes[index];
				aiString materialName("<invalid material index>");
				if (mesh->mMaterialIndex < scene->mNumMaterials)
					scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, materialName);
				log << "Mesh " << index << ": name="
					<< (mesh->mName.length > 0 ? mesh->mName.C_Str() : "<unnamed>")
					<< " vertices=" << mesh->mNumVertices
					<< " faces=" << mesh->mNumFaces
					<< " materialIndex=" << mesh->mMaterialIndex
					<< " materialName=" << materialName.C_Str()
					<< " bones=" << mesh->mNumBones
					<< " importGeometry=" << (ignoredControlMeshes[index] ? "ignored-control-shape" : "yes") << '\n';
			}
			log << "Scene node mesh references and local transforms:\n";
			WriteNodeMeshReferences(log, scene->mRootNode, "", scene);
			log.flush();
		}
		catch (...)
		{
			// Diagnostic logging must not prevent an FBX from importing.
		}
	}

}

ImportedModel ModelImporter::Import(const std::string& path, bool flipUvs, bool loadAnimations) const
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
		Root::Current().Debugger().LogMessage(std::string("ASSIMP ERROR: ") + model.importer->GetErrorString());
		return model;
	}
	WriteMultiMeshImportLog(path, model.scene);
	const std::vector<bool> ignoredControlMeshes = RigControlMeshesToIgnore(model.scene);

	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> faces;
	int vertexOffset = 0;

	for (int i = 0; i < static_cast<int>(model.scene->mNumMeshes); ++i)
	{
		const aiMesh* mesh = model.scene->mMeshes[i];
		if (ignoredControlMeshes[static_cast<std::size_t>(i)])
			continue;
		const int numVertices = static_cast<int>(mesh->mNumVertices);
		const int numBones = static_cast<int>(mesh->mNumBones);
		glm::vec3 subMeshMin(FLT_MAX);
		glm::vec3 subMeshMax(-FLT_MAX);

		for (int v = 0; v < numVertices; ++v)
		{
			glm::vec3 position{ mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z };
			subMeshMin = glm::min(subMeshMin, position);
			subMeshMax = glm::max(subMeshMax, position);
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
		model.subMeshNames.emplace_back(mesh->mName.C_Str());
		model.sourceMeshIndices.push_back(static_cast<unsigned int>(i));
		model.subMeshMinBounds.push_back(subMeshMin);
		model.subMeshMaxBounds.push_back(subMeshMax);

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
		float roughness = 0.0f;
		mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
		mat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
		if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
		{
			// Keep the fallback path consistent with phong.frag when an FBX
			// material has a PBR roughness factor but no usable texture.
			roughness = std::clamp(roughness, 0.0f, 1.0f);
			shininess = 128.0f - 126.0f * roughness * roughness;
		}
		else
		{
			mat->Get(AI_MATKEY_SHININESS, shininess);
		}

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

	if (loadAnimations)
	{
		ImportedAnimationSet animationSet = ImportAnimations(path);
		model.animImporters = std::move(animationSet.importers);
		model.animations = std::move(animationSet.animations);
		model.animationSources = std::move(animationSet.sources);
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

ImportedAnimationSet ModelImporter::ImportAnimations(const std::string& modelPath) const
{
	ImportedAnimationSet result;
	const std::filesystem::path animationDirectory =
		std::filesystem::path(modelPath).parent_path() / "animations";
	if (!std::filesystem::exists(animationDirectory) || !std::filesystem::is_directory(animationDirectory))
		return result;

	const std::string modelStem = ToLower(std::filesystem::path(modelPath).stem().string());
	static const std::vector<std::string> modelExtensions = { ".fbx", ".obj", ".gltf", ".glb", ".dae" };
	std::vector<std::filesystem::path> animationFiles;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(animationDirectory))
	{
		if (!entry.is_regular_file())
			continue;

		const std::string extension = ToLower(entry.path().extension().string());
		if (std::find(modelExtensions.begin(), modelExtensions.end(), extension) == modelExtensions.end())
			continue;

		const std::string animationStem = ToLower(entry.path().stem().string());
		const std::string animationPrefix = modelStem + "_";
		const bool isLegacySalvadoreHurt = modelStem == "drsalvador" && animationStem == "drsalvadore_hurt";
		if (animationStem != modelStem && animationStem.rfind(animationPrefix, 0) != 0 && !isLegacySalvadoreHurt)
			continue;
		animationFiles.push_back(entry.path());
	}
	std::sort(animationFiles.begin(), animationFiles.end());

	for (const std::filesystem::path& animationFile : animationFiles)
	{
		auto animationImporter = std::make_unique<Assimp::Importer>();
		const aiScene* animationScene = animationImporter->ReadFile(animationFile.string(), aiProcess_Triangulate);
		if (!animationScene)
			continue;

		for (unsigned int index = 0; index < animationScene->mNumAnimations; ++index)
		{
			result.animations.push_back(animationScene->mAnimations[index]);
			// Use the authored file name as the stable identity shown in the editor.
			result.sources.push_back(animationFile.filename().string());
		}
		result.importers.push_back(std::move(animationImporter));
	}
	return result;
}

