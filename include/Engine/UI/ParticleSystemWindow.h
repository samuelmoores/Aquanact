#pragma once

class SceneManager;

class ParticleSystemWindow
{
public:
	void Draw(SceneManager& sceneManager, unsigned int selectedEntityId, bool& open) const;
};
