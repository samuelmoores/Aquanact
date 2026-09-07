#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Core/Animation.h"
#include "Engine/Core/Mesh.h"

struct aiNode;

struct AnimEvent {
	int clipIndex;
	float tickTime;
	std::function<void()> callback;
};

// Owns animation clips and evaluates them against a referenced skeleton hierarchy.
// Each Update advances the active clip, blends toward a requested clip when needed,
// writes the resulting bone transforms to the skeleton, and fires crossed timeline events.
class Animator {
public:
	// Construction
	Animator(const aiNode* rootNode, Skeleton* skeleton);

	// Playback
	void Play(int clipIndex, float blendSeconds = 0.33f);
	// Selects a clip immediately and rewinds it to its first pose. This is used
	// at scene/play-session boundaries where playback must not inherit runtime
	// state from the previous session.
	void Restart(int clipIndex);
	void Update(float dt);
	// Evaluates the active clip at an explicit time without advancing runtime
	// state. Used by cutscene scrubbing and deterministic timeline playback.
	void EvaluateClipAt(int clipIndex, float seconds, bool loop = true);

	// Returns the full clip length in seconds.
	float ClipDuration(int clipIndex) const;

	// Current playback position, expressed in the active clip's animation ticks.
	int CurrentClipIndex() const;
	float CurrentTimeTicks() const;
	float CurrentClipDurationTicks() const;
	int CurrentClipFrameCount() const;

	// Clip management
	void AddClip(Animation* clip);
	int ClipCount() const;

	// Animation events
	// Register a callback to fire each time the animation cursor crosses tickTime in the given clip.
	// tickTime is in animation ticks (same units as Animation::Duration()).
	void AddEvent(int clipIndex, float tickTime, std::function<void()> callback);

private:
	// Pose evaluation
	void Traverse(float timeTicks, const aiNode* node, const aiMatrix4x4& parent, Animation* anim);
	void TraverseBlend(float ticksA, float ticksB, float blend, const aiNode* node, const aiMatrix4x4& parent, Animation* animA, Animation* animB);

	// Event dispatch
	void FireEvents(int clipIndex, float prevTicks, float currTicks, float duration);

	// Animation dependencies
	const aiNode* m_rootNode;
	Skeleton* m_skeleton;

	// Owned animation data
	std::vector<std::unique_ptr<Animation>> m_clips;
	std::vector<AnimEvent> m_events;

	// Active playback state
	// No clip is active until the state machine applies its saved initial state.
	// Defaulting to zero made the first imported file visible in the editor; for
	// Griff that file is the falling animation.
	int m_currentClip = -1;
	float m_currentTime = 0.0f;

	// Blend target state
	int m_nextClip = -1;
	float m_nextTime = 0.0f;
	float m_blendFactor = 1.0f;
	float m_blendSpeed = 3.0f;

	// Event cursor state
	float m_prevTicks = 0.0f;
};


