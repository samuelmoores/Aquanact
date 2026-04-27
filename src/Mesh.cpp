#include <iomanip>
#include <filesystem>
#include <Engine.h>
#include <Mesh.h>
#include <glad/glad.h>
#include <stb_image.h>
#include <Line.h>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

const int VERTICES_PER_FACE = 3;
void AddBoneData(Vertex3D& vertex, int boneID, float weight) 
{
	for (int i = 0; i < 4; i++) 
	{
		if (vertex.boneIDs[i] == 0) 
		{
			vertex.boneIDs[i] = boneID;
			vertex.weights[i] = weight;
			return;
		}
	}

	//more than 4 weights
	assert(0);
}
void printMatrix(const aiMatrix4x4& m) {
	// Set precision and fixed notation
	std::cout << std::fixed << std::setprecision(3);

	// Print each row with proper alignment
	// Width of 9 accommodates sign, digits, decimal point, and precision
	std::cout << "[ "
		<< std::setw(9) << m.a1 << " "
		<< std::setw(9) << m.a2 << " "
		<< std::setw(9) << m.a3 << " "
		<< std::setw(9) << m.a4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.b1 << " "
		<< std::setw(9) << m.b2 << " "
		<< std::setw(9) << m.b3 << " "
		<< std::setw(9) << m.b4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.c1 << " "
		<< std::setw(9) << m.c2 << " "
		<< std::setw(9) << m.c3 << " "
		<< std::setw(9) << m.c4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.d1 << " "
		<< std::setw(9) << m.d2 << " "
		<< std::setw(9) << m.d3 << " "
		<< std::setw(9) << m.d4 << " ]\n";
	std::cout << "----------------------------------------------\n";
}

Mesh::Mesh()
{
}
Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_vertices = vertices;
	m_faces = faces;
	m_currVao = 0;
	m_currTextureColor = 0;

	//cube only
	m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
	m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);

	m_meshMinBounds = m_minBounds;
	m_meshMaxBounds = m_maxBounds;
	
	SetBuffers(vertices, faces);
	SetTexture("models/brick_wall_diff.png");
}
Mesh::Mesh(char modelFile[])
{
	m_currVao = 0;
	m_currTextureColor = 0;

	assimpLoad(modelFile, true);

	// load animations from sibling animations/ folder
	namespace fs = std::filesystem;
	fs::path animDir = fs::path(modelFile).parent_path() / "animations";
	if (fs::exists(animDir) && fs::is_directory(animDir))
	{
		static const std::vector<std::string> modelExts = { ".fbx", ".obj", ".gltf", ".glb", ".dae" };
		for (const auto& entry : fs::directory_iterator(animDir))
		{
			if (!entry.is_regular_file()) continue;
			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			if (std::find(modelExts.begin(), modelExts.end(), ext) == modelExts.end()) continue;

			std::string animPath = entry.path().string();
			std::cout << "[Mesh] loading animation: " << animPath << std::endl;

			auto& imp = m_animImporters.emplace_back(std::make_unique<Assimp::Importer>());
			const aiScene* animScene = imp->ReadFile(animPath, aiProcess_Triangulate);
			if (animScene)
			{
				for (unsigned int i = 0; i < animScene->mNumAnimations; i++)
					m_animations.push_back(animScene->mAnimations[i]);
			}
		}
	}

	//debug skeleton code
	if (false)
	{
		for (int i = 0; i < 100; ++i) {
			const auto& v = m_vertices[i];
			std::cout << "Vertex " << i << ": Pos(" << v.position.x << ", " << v.position.y << ", " << v.position.z << ")";
			std::cout << " Bones: [" << v.boneIDs[0] << ", " << v.boneIDs[1] << ", " << v.boneIDs[2] << ", " << v.boneIDs[3] << "]";
			std::cout << " Weights: [" << v.weights[0] << ", " << v.weights[1] << ", " << v.weights[2] << ", " << v.weights[3] << "]" << std::endl;
		}

		for (const auto& pair : m_skeleton.boneMapping) {
			std::cout << "Bone name: " << pair.first << ", Index: " << pair.second << std::endl;
		}

		for (size_t i = 0; i < m_skeleton.boneOffsetMatrices.size(); ++i) {
			const aiMatrix4x4& mat = m_skeleton.boneOffsetMatrices[i];
			std::cout << "Bone " << i << " offset matrix:\n";
			for (int row = 0; row < 4; ++row) {
				for (int col = 0; col < 4; ++col) {
					std::cout << mat[row][col] << " ";
				}
				std::cout << std::endl;
			}
		}
	}

	

	
}

//loading
void Mesh::assimpLoad(const std::string& path, bool flipUvs)
{
	m_minBounds = glm::vec3(FLT_MAX);
	m_maxBounds = glm::vec3(-FLT_MAX);

	int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
	
	if (flipUvs) 
	{
		flags |= aiProcess_FlipUVs;
	}

	m_scene = m_importer.ReadFile(path, flags | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

	// If the import failed, report it
	if (nullptr == m_scene) 
	{
		std::cout << "ASSIMP ERROR: " << m_importer.GetErrorString() << std::endl;
		exit(1);
	}
	else 
	{
		m_GlobalInverseTransform = m_scene->mRootNode->mTransformation;
		m_GlobalInverseTransform = m_GlobalInverseTransform.Inverse();

		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> faces;

		//process vertices, faces and bones
		m_totalVertices = 0;

		for (int i = 0; i < m_scene->mNumMeshes; i++)
		{
			fromAssimpMesh(m_scene->mMeshes[i], vertices, faces);
			aiMaterial* mat = m_scene->mMaterials[m_scene->mMeshes[i]->mMaterialIndex];
			aiString matName;
			mat->Get(AI_MATKEY_NAME, matName);
			std::cout << "[Mesh] mesh[" << i << "] material: " << matName.C_Str() << std::endl;
			LoadTexture(mat, aiTextureType_DIFFUSE, path);

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
			m_materials.push_back(submeshMat);
		}

		m_skeleton.finalTransformations.resize(m_skeleton.boneOffsetMatrices.size());

		if (m_skinned) { ReadNodeHeirarchy(m_scene->mRootNode, aiMatrix4x4()); };

		m_vertices = vertices;
		m_faces = faces;

		//================== Materials =================================

		//LoadTexture(mat, aiTextureType_NORMALS, path);

		for (int i = 0; i < m_vertices.size(); i++)
		{
			//find max and min positions
			const glm::vec3& pos = m_vertices[i].position;

			m_minBounds.x = std::min(m_minBounds.x, pos.x);
			m_minBounds.y = std::min(m_minBounds.y, pos.y);
			m_minBounds.z = std::min(m_minBounds.z, pos.z);

			m_maxBounds.x = std::max(m_maxBounds.x, pos.x);
			m_maxBounds.y = std::max(m_maxBounds.y, pos.y);
			m_maxBounds.z = std::max(m_maxBounds.z, pos.z);
		}

		m_meshMinBounds = m_minBounds;
		m_meshMaxBounds = m_maxBounds;
		
	}
}
void Mesh::fromAssimpMesh(const aiMesh* mesh, std::vector<Vertex3D>& vertices, std::vector<uint32_t>& faces)
{
	int numVertices = mesh->mNumVertices;
	int numIndices = mesh->mNumFaces * 3;
	int numBones = mesh->mNumBones;

	//-------------------process vertices--------------------
	for (size_t i{ 0 }; i < numVertices; ++i)
	{
		glm::vec3 position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		glm::vec2 texCoord = glm::vec2(0.0f); // Default if no texcoords
		
		if (mesh->mTextureCoords[0]) 
		{
			texCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}

		glm::vec3 normal = glm::vec3(0.0f);
		if (mesh->HasNormals()) 
		{
			normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		}

		glm::vec3 tangent = glm::vec3(0.0f);
		if (mesh->HasTangentsAndBitangents())
		{
			tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
		}

		vertices.push_back({ position, texCoord, normal, tangent });
	}
	
	//-------------process faces---------------------------
	faces.reserve(mesh->mNumFaces * VERTICES_PER_FACE);
	for (size_t i{ 0 }; i < mesh->mNumFaces; ++i) 
	{
		// We assume the faces are triangular, so we push three face indexes at a time into our faces list.
		faces.push_back(mesh->mFaces[i].mIndices[0] + m_totalVertices);
		faces.push_back(mesh->mFaces[i].mIndices[1] + m_totalVertices);
		faces.push_back(mesh->mFaces[i].mIndices[2] + m_totalVertices);
	}

	m_facesSize.push_back(faces.size());

	m_skinned = mesh->mNumBones != 0;
	if (!m_skinned)
	{
		m_totalVertices += numVertices;
		SetBuffers(vertices, faces);
		return;
	}
	
	//-------------process bones-------------------
	std::map<std::string, int> boneMap = m_skeleton.boneMapping;
	for (int j = 0; j < numBones; j++)
	{
		int boneID = 0;
		aiBone* bone = mesh->mBones[j];
		std::string boneName = bone->mName.C_Str();


		if (boneMap.find(boneName) == boneMap.end())
		{
			boneID = boneMap.size();
		
			boneMap[boneName] = boneID;
			m_skeleton.boneOffsetMatrices.push_back(bone->mOffsetMatrix);
		}
		else
		{
			boneID = boneMap.find(boneName)->second;
		}

		for (int k = 0; k < bone->mNumWeights; k++)
		{
			const aiVertexWeight& vw = bone->mWeights[k];
			float weight = vw.mWeight;
			int globalVertexID = vw.mVertexId + m_totalVertices;
			AddBoneData(vertices[globalVertexID], boneID, weight);
		}
	}

	//anims
	for (int i = 0; i < m_scene->mNumAnimations; i++)
	{
		m_animations.push_back(m_scene->mAnimations[i]);
	}

	m_totalVertices += numVertices;
	m_skeleton.boneMapping = boneMap;
	SetBuffers(vertices, faces);
}
void Mesh::ReadNodeHeirarchy(const aiNode* node, const aiMatrix4x4& ParentTransform)
{
	const std::string name = node->mName.C_Str();

	aiMatrix4x4 NodeTransformation(node->mTransformation);
	aiMatrix4x4 GlobalTransform = ParentTransform * NodeTransformation;

	if (m_skeleton.boneMapping.find(name) != m_skeleton.boneMapping.end())
	{
		int boneIndex = m_skeleton.boneMapping[name];
		m_skeleton.finalTransformations[boneIndex] = (GlobalTransform * m_skeleton.boneOffsetMatrices[boneIndex]);
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		ReadNodeHeirarchy(node->mChildren[i], GlobalTransform);
	}
}
void Mesh::LoadTexture(aiMaterial* mat, aiTextureType textureType, std::string path)
{
	aiString texturePath;

	// OpenPBR/PBR materials store base color under BASE_COLOR, not DIFFUSE
	aiReturn result = mat->GetTexture(textureType, 0, &texturePath);
	if (result != AI_SUCCESS && textureType == aiTextureType_DIFFUSE)
		result = mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);

	if (result != AI_SUCCESS || texturePath.length == 0)
	{
		std::cout << "[LoadTexture] no texture found for type " << textureType << " — dumping material properties:" << std::endl;
		for (unsigned int p = 0; p < mat->mNumProperties; p++)
		{
			aiMaterialProperty* prop = mat->mProperties[p];
			std::cout << "  [" << p << "] key=" << prop->mKey.C_Str()
				<< " type=" << prop->mType << " index=" << prop->mIndex << std::endl;
		}
		return;
	}

	std::string textureFileName = texturePath.C_Str();

	//get texture filename
	size_t lastSlashIndex = textureFileName.find_last_of("/\\");
	if (lastSlashIndex != std::string::npos)
	{
		textureFileName = textureFileName.substr(lastSlashIndex + 1);
	}
	std::cout << "[LoadTexture] raw path: " << texturePath.C_Str() << " | filename: " << textureFileName << std::endl;

	const aiTexture* embeddedTexture = m_scene->GetEmbeddedTexture(texturePath.C_Str());

	if (embeddedTexture && embeddedTexture->pcData != nullptr)
	{
		if (embeddedTexture->mHeight == 0)
		{
			switch (textureType)
			{
			case aiTextureType_DIFFUSE:
				SetDiffuseTextureMemory(const_cast<aiTexture*>(embeddedTexture));
				break;
			case aiTextureType_NORMALS:
				break;
			}
		}
	}
	else //load from file
	{
		std::filesystem::path texDir = m_texturesFolder.empty()
			? std::filesystem::path(path).parent_path()
			: std::filesystem::path(m_texturesFolder);
		std::filesystem::path texPath = texDir / textureFileName;
		SetTexture(texPath.string().c_str());
	}
}

//bounding box
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
	line.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	line.draw(Engine::Camera->GetViewMatrix());
}
void Mesh::updateAABB(glm::vec3 position, glm::vec3 scale)
{
	m_meshMinBounds += position;
	m_meshMaxBounds += position;


	// Ensure correct ordering if scale is negative
	for (int i = 0; i < 3; ++i) 
	{
		if (m_minBounds[i] > m_maxBounds[i])
			std::swap(m_minBounds[i], m_maxBounds[i]);
	}
}
glm::vec3 Mesh::centerAABB()
{
	glm::vec3 center;
	center.x = (m_minBounds.x + m_maxBounds.x) / 2;
	center.y = (m_minBounds.y + m_maxBounds.y) / 2;
	center.z = (m_minBounds.z + m_maxBounds.z) / 2;
	return center;
}
glm::vec3 Mesh::dimensionAABB()
{
	glm::vec3 dimensions;
	dimensions.x = m_maxBounds.x - m_minBounds.x;
	dimensions.x = m_maxBounds.y - m_minBounds.y;
	dimensions.x = m_maxBounds.z - m_minBounds.z;
	return dimensions;
}
bool Mesh::intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const
{
	float tMin = 0.0f;
	float tMax = 1e6f;

	for (int i = 0; i < 3; ++i) 
	{
		if (std::abs(rayDir[i]) < 1e-6f) 
		{
			// Ray is parallel to slab
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

	//std::cout << "  camera center: " << center.x << ", " << center.y << ", " << center.z << std::endl;
	//std::cout << "m_meshMinBounds: " << m_meshMinBounds.x << ", " << m_meshMinBounds.y << ", " << m_meshMinBounds.z << std::endl;
	//std::cout << "m_meshMaxBounds: " << m_meshMaxBounds.x << ", " << m_meshMaxBounds.y << ", " << m_meshMaxBounds.z << std::endl;

	float distSq = glm::length2(center - closest);
	//std::cout << "distSq from camera center to closest point on AABB < radius squared | " << distSq << " < " << radius * radius << std::endl;
	return distSq < radius * radius;
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

//animation data access
int Mesh::NumAnimations() const { return (int)m_animations.size(); }
aiAnimation* Mesh::GetAnimation(int i) const { return m_animations[i]; }
const aiNode* Mesh::GetRootNode() const { return m_scene->mRootNode; }



//getter setter
void Mesh::SetBuffers(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_vao.push_back(0);
	// Generate a vertex array object on the GPU.
	glGenVertexArrays(1, &m_vao.back());

	// "Bind" the newly-generated vao, which makes future functions operate on that specific object.
	glBindVertexArray(m_vao.back());

	// Generate a vertex buffer object on the GPU.
	uint32_t vbo;
	glGenBuffers(1, &vbo);

	// "Bind" the newly-generated vbo, which makes future functions operate on that specific object.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// This vbo is now associated with m_vao.

	// Copy the contents of the vertices list to the buffer that lives on the GPU.
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), &vertices[0], GL_STATIC_DRAW);

	// Use offsetof to get exact offsets instead of hardcoding them
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), 0);
	glEnableVertexAttribArray(0); // position

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)12);
	glEnableVertexAttribArray(1); // text coords

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)20);
	glEnableVertexAttribArray(2); // normal

	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)32);
	glEnableVertexAttribArray(3); // tangents

	glVertexAttribIPointer(4, 4, GL_INT, sizeof(Vertex3D), (void*)44);
	glEnableVertexAttribArray(4); // boneIDs

	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)60);
	glEnableVertexAttribArray(5); // weights

	// Generate a second buffer, to store the indices of each triangle in the mesh.
	uint32_t ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(uint32_t), &faces[0], GL_STATIC_DRAW);

	// Unbind the vertex array, so no one else can accidentally mess with it.
	glBindVertexArray(0);
}
void Mesh::SetTexture(const char* colorFile)
{
	m_textureColor.push_back(0);

	StbImage stb_image_color;

	stb_image_color.loadFromFile(colorFile);

	glGenTextures(1, &m_textureColor.back());

	glBindTexture(GL_TEXTURE_2D, m_textureColor.back());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		stb_image_color.getWidth(),
		stb_image_color.getHeight(),
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		stb_image_color.getData()
	);
	glGenerateMipmap(GL_TEXTURE_2D);

}
void Mesh::SetDiffuseTextureMemory(aiTexture* text)
{
	m_textureColor.push_back(0);
	StbImage stb_image_color;

	stb_image_color.loadFromMemory(text);

	glGenTextures(1, &m_textureColor.back());

	glBindTexture(GL_TEXTURE_2D, m_textureColor.back());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		stb_image_color.getWidth(),
		stb_image_color.getHeight(),
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		stb_image_color.getData()
	);
	glGenerateMipmap(GL_TEXTURE_2D);
}
SubMeshMaterial Mesh::s_defaultMaterial = {
	glm::vec4(0.3f, 0.7f, 0.1f, 8.0f),
	glm::vec3(0.2f, 0.2f, 0.2f),
	glm::vec3(1.0f, 1.0f, 1.0f),
	glm::vec3(-1.0f, -1.0f, -1.0f)
};

const SubMeshMaterial& Mesh::GetMaterial(int index) const
{
	if (index < 0 || index >= (int)m_materials.size())
		return s_defaultMaterial;
	return m_materials[index];
}

const Skeleton& Mesh::GetSkeleton() const
{
	return m_skeleton;
}
bool Mesh::Skinned()
{
	return m_skinned;
}
Skeleton* Mesh::GetSkeletonPtr() { return &m_skeleton; }
int Mesh::NumBuffers()
{
	return m_vao.size();
}
void Mesh::ClearBufferIndex()
{
	m_currVao = 0;
	m_currTextureColor = 0;
}


//opengl
void Mesh::Bind()
{
	glBindVertexArray(m_vao[m_currVao]);
	glActiveTexture(GL_TEXTURE0);// + m_currTextureColor);
	glBindTexture(GL_TEXTURE_2D, m_textureColor[m_currTextureColor]);
}
void Mesh::UnBind() 
{
	m_currVao++;
	m_currTextureColor++;
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE, 0);
}
uint32_t Mesh::FacesSize() const
{
	return m_facesSize[m_currVao];
}