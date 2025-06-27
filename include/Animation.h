#pragma once  
#include <iostream>
#include <vector>
#include <map>
#include "glm/glm.hpp"  
#include "glm/gtc/quaternion.hpp"   
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
	Animation(const char* file);
	void assimpLoad(const std::string& path, bool flipUvs);
private:
	std::map<std::string, BoneAnimChannel> m_boneAnimChannels;
	float m_duration;
	float m_ticksPerSecond;
};
