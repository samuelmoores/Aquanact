#include <Mesh.h>


const int VERTICES_PER_FACE = 3;

Mesh::Mesh()
{
}

Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_vertices = vertices;
	m_faces = faces;

	//cube only
	m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
	m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);

	m_meshMinBounds = m_minBounds;
	m_meshMaxBounds = m_maxBounds;
	
	SetBuffers();
	SetTexture("models/brick_wall_diff.png");
	SetNormalMap("models/brick_wall_norm.png");
}

Mesh::Mesh(const char* modelFile, const char* textureFile)
{
	assimpLoad(modelFile, true);

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

	m_minBounds = glm::vec3(FLT_MAX);
	m_maxBounds = glm::vec3(-FLT_MAX);

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

	SetBuffers();
	SetTexture(textureFile);
}

Mesh::Mesh(const char* modelFile, const char* textureFile, const char* normalMap)
{
	assimpLoad(modelFile, true);

	SetBuffers();
	SetTexture(textureFile);
	SetNormalMap(normalMap);
}

void AddBoneData(Vertex3D& vertex, int boneID, float weight) 
{
	for (int i = 0; i < 4; i++) 
	{
		if (vertex.weights[i] == 0.0f) 
		{
			vertex.boneIDs[i] = boneID;
			vertex.weights[i] = weight;
			return;
		}
	}
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
		if (mesh->mTextureCoords[0]) {
		texCoord = {
			mesh->mTextureCoords[0][i].x,
			mesh->mTextureCoords[0][i].y
		};
	}

		glm::vec3 normal = glm::vec3(0.0f);
		if (mesh->HasNormals()) {
		normal = {
			mesh->mNormals[i].x,
			mesh->mNormals[i].y,
			mesh->mNormals[i].z
		};
	}

		vertices.push_back({ position, texCoord, normal });
	}
	
	//-------------process faces---------------------------
	faces.reserve(mesh->mNumFaces * VERTICES_PER_FACE);
	for (size_t i{ 0 }; i < mesh->mNumFaces; ++i) 
	{
		// We assume the faces are triangular, so we push three face indexes at a time into our faces list.
		faces.push_back(mesh->mFaces[i].mIndices[0]);
		faces.push_back(mesh->mFaces[i].mIndices[1]);
		faces.push_back(mesh->mFaces[i].mIndices[2]);
	}

	//-------------process bones-------------------
	int boneID = 0;
	std::map<std::string, int> boneMap = m_skeleton.boneMapping;
	for (int j = 0; j < numBones; j++)
	{
		aiBone* bone = mesh->mBones[j];
		std::string boneName = bone->mName.C_Str();
		m_skeleton.boneOffsetMatrices.resize(numBones);

		if (boneMap.find(boneName) == boneMap.end())
		{
			boneID = boneMap.size();
			boneMap[boneName] = boneID;
			m_skeleton.boneOffsetMatrices[boneID] = bone->mOffsetMatrix;
		}
		else
		{
			boneID = boneMap[boneName];
		}

		const aiMatrix4x4& mat = m_skeleton.boneOffsetMatrices[boneID];

		for (int k = 0; k < bone->mNumWeights; k++)
		{
			const aiVertexWeight& vw = bone->mWeights[k];
			float weight = vw.mWeight;
			int globalVertexID = vw.mVertexId;
			AddBoneData(vertices[globalVertexID], boneID, weight);
		}
	}
	m_skeleton.boneMapping = boneMap;
}

void Mesh::assimpLoad(const std::string& path, bool flipUvs) {
	int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
	if (flipUvs) {
		flags |= aiProcess_FlipUVs;
	}

	m_scene = m_importer.ReadFile(path, flags | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

	// If the import failed, report it
	if (nullptr == m_scene) 
	{
		std::cout << "ASSIMP ERROR: " << m_importer.GetErrorString() << std::endl;
		exit(1);
	}
	else {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> faces;

		//process vertices, faces and bones
		fromAssimpMesh(m_scene->mMeshes[0], vertices, faces);

		m_skeleton.finalTransformations.resize(m_skeleton.boneOffsetMatrices.size());

		aiMatrix4x4 m = aiMatrix4x4();
		ReadNodeHeirarchy(m_scene->mRootNode, m);

		m_vertices = vertices;
		m_faces = faces;
	}
}

void Mesh::SetBuffers()
{
	// Generate a vertex array object on the GPU.
	glGenVertexArrays(1, &m_vao);

	// "Bind" the newly-generated vao, which makes future functions operate on that specific object.
	glBindVertexArray(m_vao);

	// Generate a vertex buffer object on the GPU.
	uint32_t vbo;
	glGenBuffers(1, &vbo);

	// "Bind" the newly-generated vbo, which makes future functions operate on that specific object.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// This vbo is now associated with m_vao.

	// Copy the contents of the vertices list to the buffer that lives on the GPU.
	glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex3D), &m_vertices[0], GL_STATIC_DRAW);

	// Use offsetof to get exact offsets instead of hardcoding them
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), 0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)12);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)20);
	glEnableVertexAttribArray(2);

	glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex3D), (void*)32);
	glEnableVertexAttribArray(3); // boneIDs

	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)48);
	glEnableVertexAttribArray(4); // weights


	// Generate a second buffer, to store the indices of each triangle in the mesh.
	uint32_t ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_faces.size() * sizeof(uint32_t), &m_faces[0], GL_STATIC_DRAW);

	// Unbind the vertex array, so no one else can accidentally mess with it.
	glBindVertexArray(0);
}

void Mesh::SetTexture(const char* colorFile)
{
	StbImage stb_image_color;

	stb_image_color.loadFromFile(colorFile);

	glGenTextures(1, &m_textureColor);

	glBindTexture(GL_TEXTURE_2D, m_textureColor);
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

void Mesh::SetNormalMap(const char* normalMap)
{
	StbImage stb_image_normal;

	stb_image_normal.loadFromFile(normalMap);

	glGenTextures(1, &m_textureNormal);

	glBindTexture(GL_TEXTURE_2D, m_textureNormal);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		stb_image_normal.getWidth(),
		stb_image_normal.getHeight(),
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		stb_image_normal.getData()
	);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void Mesh::Bind() const
{
	glBindVertexArray(m_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_textureColor);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_textureNormal);

}

void Mesh::UnBind() const
{
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE, 0);

}

uint32_t Mesh::FacesSize() const
{
	return m_faces.size();
}

void Mesh::updateAABB(glm::vec3 position, glm::vec3 scale)
{
	m_minBounds = position + m_meshMinBounds;
	m_maxBounds = position + m_meshMaxBounds;
	m_minBounds *= scale;
	m_maxBounds *= scale;

	// Ensure correct ordering if scale is negative
	for (int i = 0; i < 3; ++i) {
		if (m_minBounds[i] > m_maxBounds[i])
			std::swap(m_minBounds[i], m_maxBounds[i]);
	}
}

bool Mesh::intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const
{
	float tMin = 0.0f;
	float tMax = 1e6f;

	for (int i = 0; i < 3; ++i) {
		if (std::abs(rayDir[i]) < 1e-6f) {
			// Ray is parallel to slab
			if (rayOrigin[i] < m_minBounds[i] || rayOrigin[i] > m_maxBounds[i])
				return false;
		}
		else {
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

const Skeleton& Mesh::GetSkeleton() const
{
	return m_skeleton;
}
