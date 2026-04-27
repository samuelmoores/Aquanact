#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Animation.h"
#include "Mesh.h"

struct aiNode;

class Animator {
public:
	Animator(const aiNode* rootNode, Skeleton* skeleton);
	void AddClip(Animation* clip);
	void Play(int clipIndex);
	void Update(float dt);

private:
	void Traverse(float timeTicks, const aiNode* node, const aiMatrix4x4& parent, Animation* anim);
	void TraverseBlend(float ticksA, float ticksB, float blend, const aiNode* node, const aiMatrix4x4& parent, Animation* animA, Animation* animB);

	std::vector<std::unique_ptr<Animation>> m_clips;
	const aiNode* m_rootNode;
	Skeleton* m_skeleton;
	int m_currentClip = 0;
	int m_nextClip    = 0;
	float m_blendFactor = 1.0f;
	float m_currentTime = 0.0f;
};
