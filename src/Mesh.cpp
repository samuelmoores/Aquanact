#include <iomanip>
#include <filesystem>
#include <Engine.h>
#include <Mesh.h>
#include <glad/glad.h>
#include <stb_image.h>

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

	//cube only
	m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
	m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);

	m_meshMinBounds = m_minBounds;
	m_meshMaxBounds = m_maxBounds;
	
	SetBuffers();
	SetTexture("models/brick_wall_diff.png");
}
Mesh::Mesh(char modelFile[])
{
	assimpLoad(modelFile, true);
	m_currentAnim = 0;
	m_nextAnim = 1;

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
}

//loading
void Mesh::assimpLoad(const std::string& path, bool flipUvs) 
{
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

		std::cout << "num meshes: " << m_scene->mNumMeshes << std::endl;
		fromAssimpMesh(m_scene->mMeshes[0], vertices, faces);

		m_skeleton.finalTransformations.resize(m_skeleton.boneOffsetMatrices.size());

		if (m_skinned) { ReadNodeHeirarchy(m_scene->mRootNode, aiMatrix4x4()); };

		m_vertices = vertices;
		m_faces = faces;

		//================== Materials =================================

		aiMaterial* mat = m_scene->mMaterials[0];
		LoadTexture(mat, aiTextureType_DIFFUSE, path);
		//LoadTexture(mat, aiTextureType_NORMALS, path);
		
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
		faces.push_back(mesh->mFaces[i].mIndices[0]);
		faces.push_back(mesh->mFaces[i].mIndices[1]);
		faces.push_back(mesh->mFaces[i].mIndices[2]);
	}

	m_skinned = mesh->mNumBones != 0;
	if (!m_skinned) return;
	
	//-------------process bones-------------------
	std::map<std::string, int> boneMap = m_skeleton.boneMapping;
	m_skeleton.boneOffsetMatrices.resize(numBones);
	for (int j = 0; j < numBones; j++)
	{
		int boneID = 0;
		aiBone* bone = mesh->mBones[j];
		std::string boneName = bone->mName.C_Str();


		if (boneMap.find(boneName) == boneMap.end())
		{
			boneID = boneMap.size();
		
			if (boneID == 0)
				int titMilk = 69;
			
			boneMap[boneName] = boneID;
			m_skeleton.boneOffsetMatrices[boneID] = bone->mOffsetMatrix;
		}
		else
		{
			boneID = boneMap.find(boneName)->second;
		}

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

	//for each diffuse, normal, roughness...
	mat->GetTexture(textureType, 0, &texturePath);

	std::string textureFileName = texturePath.C_Str();

	//get texture filename
	size_t lastSlashIndex = textureFileName.find_last_of("/\\");
	if (lastSlashIndex != std::string::npos)
	{
		textureFileName = textureFileName.substr(lastSlashIndex + 1);
	}

	//search for diffuse, normal, roughness
	aiTexture* embeddedTexture = nullptr;
	for (unsigned int i = 0; i < m_scene->mNumTextures; i++)
	{
		std::string embeddedName = m_scene->mTextures[i]->mFilename.C_Str();
		size_t lastSlash2 = embeddedName.find_last_of("/\\");

		if (lastSlash2 != std::string::npos)
		{
			embeddedName = embeddedName.substr(lastSlash2 + 1);
		}

		if (embeddedName == textureFileName)
		{
			embeddedTexture = m_scene->mTextures[i];
			break;
		}
	}

	//Is there an embedded texture?
	if (embeddedTexture)
	{
		if (embeddedTexture->mHeight == 0)
		{
			switch (textureType)
			{
			case aiTextureType_DIFFUSE:
				SetDiffuseTextureMemory(embeddedTexture);
				break;
			case aiTextureType_NORMALS:
				SetNormalMapMemory(embeddedTexture);
				break;
			}
		}
	}
	else //otherwise load from file
	{
		std::filesystem::path p = path;
		p.replace_extension(".png");
		SetTexture(p.string().c_str());
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
void Mesh::updateAABB(glm::vec3 position, glm::vec3 scale)
{
	m_minBounds = position + m_meshMinBounds;
	m_maxBounds = position + m_meshMaxBounds;
	m_minBounds *= scale;
	m_maxBounds *= scale;

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

//animation
void Mesh::RunAnimation(float animTime)
{
	aiMatrix4x4 I = aiMatrix4x4();
	float ticksPerSec = (float)(m_scene->mAnimations[m_currentAnim]->mTicksPerSecond != 0 ? m_scene->mAnimations[m_currentAnim]->mTicksPerSecond : 60.0f);
	float timeTicks = animTime * ticksPerSec;
	float animTimeTicks = fmod(timeTicks, (float)m_scene->mAnimations[m_currentAnim]->mDuration);

	ReadNodeHeirarchy(animTimeTicks, m_scene->mRootNode, I, m_currentAnim);
}

int count = 0;
void Mesh::BlendAnimation(int nextAnim, float animTime, float blendFactor)
{
	count++;
	aiMatrix4x4 I = aiMatrix4x4();
	float ticksPerSec_Start = (float)(m_scene->mAnimations[m_currentAnim]->mTicksPerSecond != 0 ? m_scene->mAnimations[m_currentAnim]->mTicksPerSecond : 60.0f);
	float timeTicks_Start = animTime * ticksPerSec_Start;
	float animTimeTicks_Start = fmod(timeTicks_Start, (float)m_scene->mAnimations[m_currentAnim]->mDuration);

	float ticksPerSec_End = (float)(m_scene->mAnimations[nextAnim]->mTicksPerSecond != 0 ? m_scene->mAnimations[nextAnim]->mTicksPerSecond : 60.0f);
	float timeTicks_End = animTime * ticksPerSec_End;
	float animTimeTicks_End = fmod(timeTicks_End, (float)m_scene->mAnimations[nextAnim]->mDuration);

	aiAnimation* startAnim = m_scene->mAnimations[m_currentAnim];
	aiAnimation* endAnim = m_scene->mAnimations[nextAnim];

	ReadNodeHeirarchyBlend(blendFactor, animTimeTicks_Start, animTimeTicks_End, m_scene->mRootNode, I, startAnim, endAnim, nextAnim);

}
const aiNodeAnim* Mesh::FindNodeAnim(const aiAnimation* pAnimation, const std::string& NodeName)
{
	for (int i = 0; i < pAnimation->mNumChannels; i++) {
		const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

		if (std::string(pNodeAnim->mNodeName.data) == NodeName) {
			return pNodeAnim;
		}
	}

	return nullptr;
}
aiMatrix4x4 CreateScalingMatrix(const aiVector3D& scale)
{
	aiMatrix4x4 scalingMatrix;

	// Set the diagonal values for scaling
	scalingMatrix.a1 = scale.x; // X-axis scaling
	scalingMatrix.b2 = scale.y; // Y-axis scaling
	scalingMatrix.c3 = scale.z; // Z-axis scaling
	scalingMatrix.d4 = 1.0f;    // Homogeneous coordinate

	// Set the rest of the matrix to zero
	scalingMatrix.a2 = scalingMatrix.a3 = scalingMatrix.a4 = 0.0f;
	scalingMatrix.b1 = scalingMatrix.b3 = scalingMatrix.b4 = 0.0f;
	scalingMatrix.c1 = scalingMatrix.c2 = scalingMatrix.c4 = 0.0f;
	scalingMatrix.d1 = scalingMatrix.d2 = scalingMatrix.d3 = 0.0f;

	return scalingMatrix;
}
aiMatrix4x4 CreateTranslationMatrix(const aiVector3D& translation)
{
	aiMatrix4x4 translationMatrix;

	// Identity matrix first
	translationMatrix.a1 = 1.0f; translationMatrix.a2 = 0.0f; translationMatrix.a3 = 0.0f; translationMatrix.a4 = translation.x;
	translationMatrix.b1 = 0.0f; translationMatrix.b2 = 1.0f; translationMatrix.b3 = 0.0f; translationMatrix.b4 = translation.y;
	translationMatrix.c1 = 0.0f; translationMatrix.c2 = 0.0f; translationMatrix.c3 = 1.0f; translationMatrix.c4 = translation.z;
	translationMatrix.d1 = 0.0f; translationMatrix.d2 = 0.0f; translationMatrix.d3 = 0.0f; translationMatrix.d4 = 1.0f;

	return translationMatrix;
}
int Mesh::FindPosition(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	for (int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
		float t = (float)pNodeAnim->mPositionKeys[i + 1].mTime;
		if (AnimationTimeTicks < t) {
			return i;
		}
	}

	return 0;
}
void Mesh::CalcInterpolatedPosition(aiVector3D& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	// we need at least two values to interpolate...
	if (pNodeAnim->mNumPositionKeys == 1) {
		Out = pNodeAnim->mPositionKeys[0].mValue;
		return;
	}

	int PositionIndex = FindPosition(AnimationTimeTicks, pNodeAnim);
	int NextPositionIndex = PositionIndex + 1;
	assert(NextPositionIndex < pNodeAnim->mNumPositionKeys);
	float t1 = (float)pNodeAnim->mPositionKeys[PositionIndex].mTime;
	float t2 = (float)pNodeAnim->mPositionKeys[NextPositionIndex].mTime;
	float DeltaTime = t2 - t1;
	float Factor = (AnimationTimeTicks - t1) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
	const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}
int Mesh::FindRotation(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumRotationKeys > 0);

	for (int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
		float t = (float)pNodeAnim->mRotationKeys[i + 1].mTime;
		if (AnimationTimeTicks < t) {
			return i;
		}
	}

	return 0;
}
void Mesh::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	// we need at least two values to interpolate...
	if (pNodeAnim->mNumRotationKeys == 1) {
		Out = pNodeAnim->mRotationKeys[0].mValue;
		return;
	}

	int RotationIndex = FindRotation(AnimationTimeTicks, pNodeAnim);
	int NextRotationIndex = RotationIndex + 1;
	assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);
	float t1 = (float)pNodeAnim->mRotationKeys[RotationIndex].mTime;
	float t2 = (float)pNodeAnim->mRotationKeys[NextRotationIndex].mTime;
	float DeltaTime = t2 - t1;
	float Factor = (AnimationTimeTicks - t1) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;
	aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
	Out.Normalize();
}
int Mesh::FindScaling(float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumScalingKeys > 0);

	for (int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) 
	{
		float t = (float)pNodeAnim->mScalingKeys[i + 1].mTime;
		if (AnimationTimeTicks < t) 
		{
			return i;
		}
	}

	return 0;
}
void Mesh::CalcInterpolatedScaling(aiVector3D& Out, float AnimationTimeTicks, const aiNodeAnim* pNodeAnim)
{
	// we need at least two values to interpolate...
	if (pNodeAnim->mNumScalingKeys == 1) {
		Out = pNodeAnim->mScalingKeys[0].mValue;
		return;
	}

	int ScalingIndex = FindScaling(AnimationTimeTicks, pNodeAnim);
	int NextScalingIndex = ScalingIndex + 1;
	assert(NextScalingIndex < pNodeAnim->mNumScalingKeys);
	float t1 = (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime;
	float t2 = (float)pNodeAnim->mScalingKeys[NextScalingIndex].mTime;
	float DeltaTime = t2 - t1;
	float Factor = (AnimationTimeTicks - (float)t1) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
	const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}
void Mesh::SetAnim(int animIndex)
{
	m_currentAnim = animIndex;
}
void Mesh::ReadNodeHeirarchy(float animTimeTicks, const aiNode* node, const aiMatrix4x4& ParentTransform, int animIndex)
{
	bool debugMatrices = false;

	const std::string name = node->mName.C_Str();

	const aiAnimation* anim = m_scene->mAnimations[animIndex];
	aiMatrix4x4 NodeTransformation(node->mTransformation);

	if (name.find("$AssimpFbx$") != std::string::npos)
		NodeTransformation = aiMatrix4x4();

	if (debugMatrices)
	{
		std::cout << "********** node: " << name << " ***********************\n";
		std::cout << "NodeTransformation Matrix: \n";
		printMatrix(NodeTransformation);

	}

	const aiNodeAnim* animNode = FindNodeAnim(anim, name);

	if (animNode) 
	{
		// Interpolate scaling and generate scaling transformation matrix
		aiVector3D Scaling;
		CalcInterpolatedScaling(Scaling, animTimeTicks, animNode);
		aiMatrix4x4 ScalingM = CreateScalingMatrix(Scaling);

		// Interpolate rotation and generate rotation transformation matrix
		aiQuaternion RotationQ;
		CalcInterpolatedRotation(RotationQ, animTimeTicks, animNode);
		aiMatrix4x4 RotationM = aiMatrix4x4(RotationQ.GetMatrix());
		
		// Interpolate translation and generate translation transformation matrix
		aiVector3D Translation;
		CalcInterpolatedPosition(Translation, animTimeTicks, animNode);
		aiMatrix4x4 TranslationM = CreateTranslationMatrix(Translation);

		// Combine the above transformations
		NodeTransformation = TranslationM * RotationM * ScalingM;

		if (debugMatrices)
		{
			std::cout << "##### anim[0]: " << anim->mName.C_Str() << " #########\n";
			std::cout << "scaling mat: \n";
			printMatrix(ScalingM);
			std::cout << "rotation mat: \n";
			printMatrix(RotationM);
			std::cout << "translation mat: \n";
			printMatrix(TranslationM);
			std::cout << "NodeTransformation mat after anim: \n";
			printMatrix(NodeTransformation);
		}

		int tit = 59;
	}

	aiMatrix4x4 GlobalTransform = ParentTransform * NodeTransformation;
	std::string parentName = "no parent (identity matrix)";

	if (debugMatrices)
	{
		std::cout << "ParentTransform Matrix: \n";
		printMatrix(ParentTransform);
		std::cout << "ChildTransform Matrix (current)\n";
		printMatrix(NodeTransformation);
		std::cout << "--- GlobalTransform Matrix: Parent * Child ----\n";
		printMatrix(GlobalTransform);
	}

	if (m_skeleton.boneMapping.find(name) != m_skeleton.boneMapping.end())
	{
		numBoneUpdates++;
		int boneIndex = m_skeleton.boneMapping[name];
		m_skeleton.finalTransformations[boneIndex] = (GlobalTransform * m_skeleton.boneOffsetMatrices[boneIndex]);

		if (debugMatrices)
		{
			std::cout << "node in skeleton at bone: " << name << " index: " << boneIndex << std::endl;
			std::cout << "Offset matrix\n";
			printMatrix(m_skeleton.boneOffsetMatrices[boneIndex]);
			std::cout << "skeleton bone final transorm: GlobInv * Glob * Offset: \n";
			
			printMatrix(m_skeleton.finalTransformations[boneIndex]);
		}
	}
	else
	{
		if(debugMatrices)
			std::cout << name << " is not in skeleton\n";
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		//std::cout << "ReadNodeHeirarchy() parent: " << name << " | child: " << node->mChildren[i]->mName.C_Str() << std::endl;
		ReadNodeHeirarchy(animTimeTicks, node->mChildren[i], GlobalTransform, animIndex);
	}
}
void Mesh::ReadNodeHeirarchyBlend(float blendFactor, float animTimeTicks_Start, float animTimeTicks_End, const aiNode* node, const aiMatrix4x4& ParentTransform, aiAnimation* start, aiAnimation* end, int animIndex)
{
	bool debugMatrices = false;

	const std::string name = node->mName.C_Str();
	aiMatrix4x4 NodeTransformation(node->mTransformation);

	if (name.find("$AssimpFbx$") != std::string::npos)
		NodeTransformation = aiMatrix4x4();
	

	if (debugMatrices)
	{
		std::cout << "********** node: " << name << " ***********************\n";
		std::cout << "NodeTransformation Matrix: \n";
		printMatrix(NodeTransformation);

	}

	const aiAnimation* animStart = m_scene->mAnimations[m_currentAnim];
	const aiNodeAnim* animNodeStart = FindNodeAnim(animStart, name);
	aiVector3D   StartScaling;
	aiQuaternion StartRotation;
	aiVector3D   StartTranslation;

	const aiAnimation* animEnd = m_scene->mAnimations[animIndex];
	const aiNodeAnim* animNodeEnd = FindNodeAnim(animEnd, name);
	aiVector3D EndScaling;
	aiQuaternion EndRotation;
	aiVector3D EndTranslation;

	if (animNodeStart)
	{
		// Interpolate scaling and generate scaling transformation matrix
		CalcInterpolatedScaling(StartScaling, animTimeTicks_Start, animNodeStart);

		// Interpolate rotation and generate rotation transformation matrix
		CalcInterpolatedRotation(StartRotation, animTimeTicks_Start, animNodeStart);

		// Interpolate translation and generate translation transformation matrix
		CalcInterpolatedPosition(StartTranslation, animTimeTicks_Start, animNodeStart);

	}

	if (animNodeEnd)
	{
		// Interpolate scaling and generate scaling transformation matrix
		CalcInterpolatedScaling(EndScaling, animTimeTicks_End, animNodeEnd);

		// Interpolate rotation and generate rotation transformation matrix
		CalcInterpolatedRotation(EndRotation, animTimeTicks_End, animNodeEnd);

		// Interpolate translation and generate translation transformation matrix
		CalcInterpolatedPosition(EndTranslation, animTimeTicks_End, animNodeEnd);

	}

	//blend anim interp
	if (animNodeStart && animNodeEnd)
	{
		//interp scale
		const aiVector3D& Scale0 = StartScaling;
		const aiVector3D& Scale1 = EndScaling;
		aiVector3D BlendScaling = (1.0f - blendFactor) * Scale0 + Scale1 * blendFactor;
		aiMatrix4x4 ScalingM = CreateScalingMatrix(BlendScaling);

		//interp rot
		const aiQuaternion& Rot0 = StartRotation;
		const aiQuaternion& Rot1 = EndRotation;
		aiQuaternion blendRot;
		aiQuaternion::Interpolate(blendRot, Rot0, Rot1, blendFactor);
		aiMatrix4x4 RotationM = aiMatrix4x4(blendRot.GetMatrix());

		//interp trans
		const aiVector3D& Trans0 = StartTranslation;
		const aiVector3D& Trans1 =   EndTranslation;
		aiVector3D BlendTrans = (1.0f - blendFactor) * Trans0 + Trans1 * blendFactor;
		aiMatrix4x4 TranslationM = CreateTranslationMatrix(BlendTrans);

		//combine all to NodeTransformation
		NodeTransformation = TranslationM * RotationM * ScalingM;

		if (debugMatrices)
		{
			std::cout << "##### anim[0]: " << animStart->mName.C_Str() << " #########\n";
			std::cout << "##### anim[1]: " << animEnd->mName.C_Str() << " #########\n";
			std::cout << "scaling mat: \n";
			printMatrix(ScalingM);
			std::cout << "rotation mat: \n";
			printMatrix(RotationM);
			std::cout << "translation mat: \n";
			printMatrix(TranslationM);
			std::cout << "NodeTransformation mat after anim: \n";
			printMatrix(NodeTransformation);
		}
	}

	aiMatrix4x4 GlobalTransform = ParentTransform * NodeTransformation;
	std::string parentName = "no parent (identity matrix)";

	if (debugMatrices)
	{
		std::cout << "ParentTransform Matrix: \n";
		printMatrix(ParentTransform);
		std::cout << "ChildTransform Matrix (current)\n";
		printMatrix(NodeTransformation);
		std::cout << "--- GlobalTransform Matrix: Parent * Child ----\n";
		printMatrix(GlobalTransform);
	}

	if (m_skeleton.boneMapping.find(name) != m_skeleton.boneMapping.end())
	{
		numBoneUpdates++;
		int boneIndex = m_skeleton.boneMapping[name];
		m_skeleton.finalTransformations[boneIndex] = (GlobalTransform * m_skeleton.boneOffsetMatrices[boneIndex]);

		if (debugMatrices)
		{
			std::cout << "node in skeleton at bone: " << name << " index: " << boneIndex << std::endl;
			std::cout << "Offset matrix\n";
			printMatrix(m_skeleton.boneOffsetMatrices[boneIndex]);
			std::cout << "skeleton bone final transorm: GlobInv * Glob * Offset: \n";

			printMatrix(m_skeleton.finalTransformations[boneIndex]);
		}
	}
	else
	{
		if (debugMatrices)
			std::cout << name << " is not in skeleton\n";
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		//std::cout << "ReadNodeHeirarchy() parent: " << name << " | child: " << node->mChildren[i]->mName.C_Str() << std::endl;
		ReadNodeHeirarchyBlend(blendFactor, animTimeTicks_Start, animTimeTicks_End, node->mChildren[i], GlobalTransform, start, end, animIndex);
	}
}

//getter setter
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
void Mesh::SetDiffuseTextureMemory(aiTexture* text)
{
	StbImage stb_image_color;

	stb_image_color.loadFromMemory(text);

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
void Mesh::SetNormalMapMemory(aiTexture* normalMap)
{
	StbImage stb_image_normal;

	stb_image_normal.loadFromMemory(normalMap);

	glGenTextures(1, &m_textureNormal);

	glBindTexture(GL_TEXTURE_2D, m_textureNormal);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB,
		stb_image_normal.getWidth(),
		stb_image_normal.getHeight(),
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		stb_image_normal.getData()
	);
	glGenerateMipmap(GL_TEXTURE_2D);
}
const Skeleton& Mesh::GetSkeleton() const
{
	return m_skeleton;
}
bool Mesh::Skinned()
{
	return m_skinned;
}

void Mesh::SetNextAnim(int nextAnim)
{
	m_nextAnim = nextAnim;
}

void Mesh::SetCurrentAnim(int currAnim)
{
	m_currentAnim = currAnim;
}

int Mesh::GetNextAnim()
{
	return m_nextAnim;
}

//opengl
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