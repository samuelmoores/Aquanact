#pragma once  
#include <iostream>
#include <vector>
#include <map>
#include <unordered_set>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"  
#include "glm/gtc/quaternion.hpp"   
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Mesh.h>

struct KeyPosition {  
   glm::vec3 position;  
   float timestamp;  
};  

struct KeyRotation {  
   glm::quat rotation;  
   float timestamp;  
};  

struct KeyScale {  
   glm::vec3 scale;  
   float timestamp;  
};  

struct BoneAnimChannel {  
   std::string boneName;  
   std::vector<KeyPosition> positions;  
   std::vector<KeyRotation> rotations;  
   std::vector<KeyScale> scales;  
};

class Animation {
public:
	Animation(const char* file, Mesh& mesh);
	void assimpLoad(const std::string& path, bool flipUvs);
	float duration();
	float ticksPerSecond();
	const std::map<std::string, BoneAnimChannel>& boneAnimChannels() const;
	const Mesh& mesh() const;
    const aiNode* root();
	int numBones();
private:
	Mesh m_mesh;
	std::map<std::string, BoneAnimChannel> m_boneAnimChannels;
	float m_duration;
	float m_ticksPerSecond;
	Assimp::Importer m_importer;
	const aiNode* m_root;
	int m_numBones;
};
