#include "Animator.h"
#include <assimp/scene.h>
#include <cmath>

static aiMatrix4x4 MakeScaling(const aiVector3D& s)
{
	aiMatrix4x4 m;
	m.a1 = s.x; m.b2 = s.y; m.c3 = s.z; m.d4 = 1.0f;
	m.a2 = m.a3 = m.a4 = 0.0f;
	m.b1 = m.b3 = m.b4 = 0.0f;
	m.c1 = m.c2 = m.c4 = 0.0f;
	m.d1 = m.d2 = m.d3 = 0.0f;
	return m;
}

static aiMatrix4x4 MakeTranslation(const aiVector3D& t)
{
	aiMatrix4x4 m;
	m.a1 = 1; m.a2 = 0; m.a3 = 0; m.a4 = t.x;
	m.b1 = 0; m.b2 = 1; m.b3 = 0; m.b4 = t.y;
	m.c1 = 0; m.c2 = 0; m.c3 = 1; m.c4 = t.z;
	m.d1 = 0; m.d2 = 0; m.d3 = 0; m.d4 = 1.0f;
	return m;
}

Animator::Animator(const aiNode* rootNode, Skeleton* skeleton)
	: m_rootNode(rootNode), m_skeleton(skeleton) {}

void Animator::AddClip(Animation* clip)
{
	m_clips.emplace_back(clip);
}

void Animator::Play(int clipIndex)
{
	if (clipIndex == m_currentClip && m_blendFactor >= 1.0f) return;
	m_nextClip    = clipIndex;
	m_blendFactor = 0.0f;
}

void Animator::Update(float dt)
{
	if (m_clips.empty()) return;
	m_currentTime += dt;

	if (m_blendFactor < 1.0f)
	{
		m_blendFactor += dt * 3.0f;
		if (m_blendFactor >= 1.0f)
		{
			m_blendFactor  = 1.0f;
			m_currentClip  = m_nextClip;
		}
		Animation* a = m_clips[m_currentClip].get();
		Animation* b = m_clips[m_nextClip].get();
		float ticksA = fmodf(m_currentTime * a->TicksPerSecond(), a->Duration());
		float ticksB = fmodf(m_currentTime * b->TicksPerSecond(), b->Duration());
		TraverseBlend(ticksA, ticksB, m_blendFactor, m_rootNode, aiMatrix4x4(), a, b);
	}
	else
	{
		Animation* a = m_clips[m_currentClip].get();
		float ticks = fmodf(m_currentTime * a->TicksPerSecond(), a->Duration());
		Traverse(ticks, m_rootNode, aiMatrix4x4(), a);
	}
}

void Animator::Traverse(float timeTicks, const aiNode* node, const aiMatrix4x4& parent, Animation* anim)
{
	const std::string name = node->mName.C_Str();
	aiMatrix4x4 nodeTransform(node->mTransformation);

	if (name.find("$AssimpFbx$") != std::string::npos)
		nodeTransform = aiMatrix4x4();

	const aiNodeAnim* ch = anim->FindChannel(name);
	if (ch)
	{
		aiVector3D   s; anim->CalcScaling(s, timeTicks, ch);
		aiQuaternion r; anim->CalcRotation(r, timeTicks, ch);
		aiVector3D   t; anim->CalcPosition(t, timeTicks, ch);
		nodeTransform = MakeTranslation(t) * aiMatrix4x4(r.GetMatrix()) * MakeScaling(s);
	}

	aiMatrix4x4 global = parent * nodeTransform;

	auto it = m_skeleton->boneMapping.find(name);
	if (it != m_skeleton->boneMapping.end())
	{
		int idx = it->second;
		m_skeleton->finalTransformations[idx] = global * m_skeleton->boneOffsetMatrices[idx];
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
		Traverse(timeTicks, node->mChildren[i], global, anim);
}

void Animator::TraverseBlend(float ticksA, float ticksB, float blend,
	const aiNode* node, const aiMatrix4x4& parent, Animation* animA, Animation* animB)
{
	const std::string name = node->mName.C_Str();
	aiMatrix4x4 nodeTransform(node->mTransformation);

	if (name.find("$AssimpFbx$") != std::string::npos)
		nodeTransform = aiMatrix4x4();

	const aiNodeAnim* chA = animA->FindChannel(name);
	const aiNodeAnim* chB = animB->FindChannel(name);

	if (chA && chB)
	{
		aiVector3D   sA, sB; animA->CalcScaling(sA, ticksA, chA);  animB->CalcScaling(sB, ticksB, chB);
		aiQuaternion rA, rB; animA->CalcRotation(rA, ticksA, chA); animB->CalcRotation(rB, ticksB, chB);
		aiVector3D   tA, tB; animA->CalcPosition(tA, ticksA, chA); animB->CalcPosition(tB, ticksB, chB);

		aiVector3D   blendS = (1.0f - blend) * sA + blend * sB;
		aiQuaternion blendR; aiQuaternion::Interpolate(blendR, rA, rB, blend); blendR.Normalize();
		aiVector3D   blendT = (1.0f - blend) * tA + blend * tB;

		nodeTransform = MakeTranslation(blendT) * aiMatrix4x4(blendR.GetMatrix()) * MakeScaling(blendS);
	}

	aiMatrix4x4 global = parent * nodeTransform;

	auto it = m_skeleton->boneMapping.find(name);
	if (it != m_skeleton->boneMapping.end())
	{
		int idx = it->second;
		m_skeleton->finalTransformations[idx] = global * m_skeleton->boneOffsetMatrices[idx];
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
		TraverseBlend(ticksA, ticksB, blend, node->mChildren[i], global, animA, animB);
}
