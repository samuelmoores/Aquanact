#pragma once

#include <cfloat>
#include <memory>
#include <string>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"

struct ImportedModel {
	std::unique_ptr<Assimp::Importer> importer;
	std::vector<std::unique_ptr<Assimp::Importer>> animImporters;
	const aiScene* scene = nullptr;
	bool skinned = false;
	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> faces;
	std::vector<int> facesSize;
	Skeleton skeleton;
	std::vector<SubMeshMaterial> materials;
	std::vector<aiAnimation*> animations;
	std::vector<std::string> animationSources;
	glm::vec3 minBounds{ FLT_MAX };
	glm::vec3 maxBounds{ -FLT_MAX };
	glm::vec3 meshMinBounds{ 0.0f };
	glm::vec3 meshMaxBounds{ 0.0f };
	std::string sourcePath;
};

class ModelImporter {
public:
	ModelImporter() = default;

	ImportedModel Import(const std::string& path, bool flipUvs = true) const;
};
