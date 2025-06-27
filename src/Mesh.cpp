#include <Mesh.h>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>

const int VERTICES_PER_FACE = 3;

Mesh::Mesh()
{
}

Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_vertices = vertices;
	m_faces = faces;
	
	SetBuffers();
	SetTexture("models/brick_wall_diff.png");
	SetNormalMap("models/brick_wall_norm.png");
}

Mesh::Mesh(const char* modelFile, const char* textureFile)
{
	assimpLoad(modelFile, true);

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

void fromAssimpMesh(const aiMesh* mesh, std::vector<Vertex3D>& vertices, std::vector<uint32_t>& faces) {
	for (size_t i{ 0 }; i < mesh->mNumVertices; ++i) {
		if (mesh->HasTangentsAndBitangents()) {
			aiVector3D t = mesh->mTangents[i];
			aiVector3D b = mesh->mBitangents[i];
			if (isnan(t.x) || isnan(b.x)) std::cout << "Bad tangent/bitangent!\n";
		}
		else
		{
			std::cout << "mesh doesn't have tangents or bitangents\n";
		}

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

		glm::vec3 tangent = glm::vec3(0.0f);
		if (mesh->HasTangentsAndBitangents()) {
			tangent = {
				mesh->mTangents[i].x,
				mesh->mTangents[i].y,
				mesh->mTangents[i].z
			};
		}

		glm::vec3 bitangent = glm::vec3(0.0f);
		if (mesh->HasTangentsAndBitangents()) {
			bitangent = {
				mesh->mBitangents[i].x,
				mesh->mBitangents[i].y,
				mesh->mBitangents[i].z
			};
		}

		vertices.push_back({ position, texCoord, normal, tangent, bitangent });
	}

	faces.reserve(mesh->mNumFaces * VERTICES_PER_FACE);
	for (size_t i{ 0 }; i < mesh->mNumFaces; ++i) {
		// We assume the faces are triangular, so we push three face indexes at a time into our faces list.
		faces.push_back(mesh->mFaces[i].mIndices[0]);
		faces.push_back(mesh->mFaces[i].mIndices[1]);
		faces.push_back(mesh->mFaces[i].mIndices[2]);
	}
}

void Mesh::assimpLoad(const std::string& path, bool flipUvs) {
	int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
	if (flipUvs) {
		flags |= aiProcess_FlipUVs;
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |  // <-- This one is needed!
		aiProcess_FlipUVs);

	// If the import failed, report it
	if (nullptr == scene) {
		std::cout << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
		exit(1);
	}
	else {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> faces;
		fromAssimpMesh(scene->mMeshes[0], vertices, faces);
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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, texCoord));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, tangent));
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, bitangent));
	glEnableVertexAttribArray(4);


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


