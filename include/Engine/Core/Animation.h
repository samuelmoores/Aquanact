#pragma once

#include <string>
#include <unordered_map>

#include <assimp/anim.h>

class Animation {
public:
	// Construction
	Animation(aiAnimation* anim);

	// Clip timing
	float Duration() const;
	float TicksPerSecond() const;

	// Channel lookup
	const aiNodeAnim* FindChannel(const std::string& name) const;

	// Transform sampling
	void CalcPosition(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const;
	void CalcRotation(aiQuaternion& out, float timeTicks, const aiNodeAnim* ch) const;
	void CalcScaling(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const;

private:
	// Keyframe lookup
	int FindPositionKey(float timeTicks, const aiNodeAnim* ch) const;
	int FindRotationKey(float timeTicks, const aiNodeAnim* ch) const;
	int FindScalingKey(float timeTicks, const aiNodeAnim* ch) const;

	// Source animation
	aiAnimation* m_anim;

	// Clip timing
	float m_duration;
	float m_ticksPerSecond;

	// Channel lookup cache
	std::unordered_map<std::string, const aiNodeAnim*> m_channelMap;
};

