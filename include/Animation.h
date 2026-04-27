#pragma once
#include <string>
#include <unordered_map>
#include <assimp/anim.h>

class Animation {
public:
	Animation(aiAnimation* anim);
	float Duration() const;
	float TicksPerSecond() const;
	const aiNodeAnim* FindChannel(const std::string& name) const;
	void CalcPosition(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const;
	void CalcRotation(aiQuaternion& out, float timeTicks, const aiNodeAnim* ch) const;
	void CalcScaling(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const;

private:
	int FindPositionKey(float timeTicks, const aiNodeAnim* ch) const;
	int FindRotationKey(float timeTicks, const aiNodeAnim* ch) const;
	int FindScalingKey(float timeTicks, const aiNodeAnim* ch) const;

	aiAnimation* m_anim;
	float m_duration;
	float m_ticksPerSecond;
	std::unordered_map<std::string, const aiNodeAnim*> m_channelMap;
};
