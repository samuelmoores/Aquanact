#pragma once
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include "Animation.h"
#include "Mesh.h"

struct aiNode;

struct AnimEvent {
	int clipIndex;
	float tickTime;
	std::function<void()> callback;
};

class Animator {
public:
	Animator(const aiNode* rootNode, Skeleton* skeleton);
	void AddClip(Animation* clip);
	void Play(int clipIndex);
	void Update(float dt);

	// Register a callback to fire each time the animation cursor crosses tickTime in the given clip.
	// tickTime is in animation ticks (same units as Animation::Duration()).
	void AddEvent(int clipIndex, float tickTime, std::function<void()> callback);

private:
	void Traverse(float timeTicks, const aiNode* node, const aiMatrix4x4& parent, Animation* anim);
	void TraverseBlend(float ticksA, float ticksB, float blend, const aiNode* node, const aiMatrix4x4& parent, Animation* animA, Animation* animB);
	void FireEvents(int clipIndex, float prevTicks, float currTicks, float duration);

	std::vector<std::unique_ptr<Animation>> m_clips;
	std::vector<AnimEvent> m_events;
	const aiNode* m_rootNode;
	Skeleton* m_skeleton;
	int m_currentClip = 0;
	int m_nextClip    = 0;
	float m_blendFactor = 1.0f;
	float m_currentTime = 0.0f;
	float m_prevTicks   = 0.0f;
};
