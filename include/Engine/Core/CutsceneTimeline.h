#pragma once

#include <string>
#include <vector>

struct CutsceneAnimationTrack
{
	unsigned int entityId = 0;
	std::string animationName;
	float startTime = 0.0f;
	float duration = 1.0f;
	float speed = 1.0f;
	float blendIn = 0.0f;
	float blendOut = 0.0f;
	bool loop = false;
};

struct CutsceneSoundEvent
{
	std::string soundPath;
	float startTime = 0.0f;
	float volume = 100.0f;
};

struct CutsceneTimeline
{
	float duration = 5.0f;
	std::vector<CutsceneAnimationTrack> animationTracks;
	std::vector<CutsceneSoundEvent> soundEvents;
};
