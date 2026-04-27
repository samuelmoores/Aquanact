#include "Animation.h"
#include <cassert>

Animation::Animation(aiAnimation* anim)
	: m_anim(anim)
	, m_duration((float)anim->mDuration)
	, m_ticksPerSecond(anim->mTicksPerSecond != 0 ? (float)anim->mTicksPerSecond : 60.0f)
{
	for (unsigned int i = 0; i < anim->mNumChannels; i++)
		m_channelMap[anim->mChannels[i]->mNodeName.C_Str()] = anim->mChannels[i];
}

float Animation::Duration() const { return m_duration; }
float Animation::TicksPerSecond() const { return m_ticksPerSecond; }

const aiNodeAnim* Animation::FindChannel(const std::string& name) const
{
	auto it = m_channelMap.find(name);
	return it != m_channelMap.end() ? it->second : nullptr;
}

int Animation::FindPositionKey(float timeTicks, const aiNodeAnim* ch) const
{
	for (int i = 0; i < (int)ch->mNumPositionKeys - 1; i++)
		if (timeTicks < (float)ch->mPositionKeys[i + 1].mTime)
			return i;
	return (int)ch->mNumPositionKeys - 2;
}

int Animation::FindRotationKey(float timeTicks, const aiNodeAnim* ch) const
{
	assert(ch->mNumRotationKeys > 0);
	for (int i = 0; i < (int)ch->mNumRotationKeys - 1; i++)
		if (timeTicks < (float)ch->mRotationKeys[i + 1].mTime)
			return i;
	return (int)ch->mNumRotationKeys - 2;
}

int Animation::FindScalingKey(float timeTicks, const aiNodeAnim* ch) const
{
	assert(ch->mNumScalingKeys > 0);
	for (int i = 0; i < (int)ch->mNumScalingKeys - 1; i++)
		if (timeTicks < (float)ch->mScalingKeys[i + 1].mTime)
			return i;
	return (int)ch->mNumScalingKeys - 2;
}

void Animation::CalcPosition(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const
{
	if (ch->mNumPositionKeys == 1) { out = ch->mPositionKeys[0].mValue; return; }
	int i = FindPositionKey(timeTicks, ch);
	float t1 = (float)ch->mPositionKeys[i].mTime;
	float t2 = (float)ch->mPositionKeys[i + 1].mTime;
	float f = (timeTicks - t1) / (t2 - t1);
	assert(f >= 0.0f && f <= 1.0f);
	out = ch->mPositionKeys[i].mValue + f * (ch->mPositionKeys[i + 1].mValue - ch->mPositionKeys[i].mValue);
}

void Animation::CalcRotation(aiQuaternion& out, float timeTicks, const aiNodeAnim* ch) const
{
	if (ch->mNumRotationKeys == 1) { out = ch->mRotationKeys[0].mValue; return; }
	int i = FindRotationKey(timeTicks, ch);
	float t1 = (float)ch->mRotationKeys[i].mTime;
	float t2 = (float)ch->mRotationKeys[i + 1].mTime;
	float f = (timeTicks - t1) / (t2 - t1);
	assert(f >= 0.0f && f <= 1.0f);
	aiQuaternion::Interpolate(out, ch->mRotationKeys[i].mValue, ch->mRotationKeys[i + 1].mValue, f);
	out.Normalize();
}

void Animation::CalcScaling(aiVector3D& out, float timeTicks, const aiNodeAnim* ch) const
{
	if (ch->mNumScalingKeys == 1) { out = ch->mScalingKeys[0].mValue; return; }
	int i = FindScalingKey(timeTicks, ch);
	float t1 = (float)ch->mScalingKeys[i].mTime;
	float t2 = (float)ch->mScalingKeys[i + 1].mTime;
	float f = (timeTicks - t1) / (t2 - t1);
	assert(f >= 0.0f && f <= 1.0f);
	out = ch->mScalingKeys[i].mValue + f * (ch->mScalingKeys[i + 1].mValue - ch->mScalingKeys[i].mValue);
}
