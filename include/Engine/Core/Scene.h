#pragma once

#include "Engine/Core/Entity.h"
#include "Engine/Core/LevelCollider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class PathedCamera;

class Scene
{
public:
	explicit Scene(std::string name = "Scene");
	~Scene();

	const std::string& Name() const { return m_name; }
	void SetName(std::string name);
	const std::string& MusicPath() const { return m_musicPath; }
	void SetMusicPath(std::string path) { m_musicPath = std::move(path); }
	float MusicVolume() const { return m_musicVolume; }
	void SetMusicVolume(float volume) { m_musicVolume = volume; }

	void startUp();
	void FirstFrame();
	void Clear();
	Entity* AddObject(std::unique_ptr<Entity> entity);
	bool RemoveObject(Entity* entity);
	LevelCollider* AddLevelCollider(std::unique_ptr<LevelCollider> collider);
	bool RemoveLevelCollider(LevelCollider* collider);
	const std::vector<std::unique_ptr<Entity>>& Entities() const { return m_entities; }
	const std::vector<std::unique_ptr<Entity>>& Objects() const { return m_entities; }
	const std::vector<std::unique_ptr<LevelCollider>>& LevelColliders() const { return m_levelColliders; }

	// Every level/cutscene owns an independent gameplay camera and authored path.
	// The render manager only selects the camera belonging to the active scene.
	PathedCamera& CameraSystem();
	const PathedCamera& CameraSystem() const;
	void EnsureCameraSystemStarted();

private:
	std::string m_name;
	std::string m_musicPath;
	float m_musicVolume = 50.0f;
	std::vector<std::unique_ptr<Entity>> m_entities;
	std::vector<std::unique_ptr<LevelCollider>> m_levelColliders;
	std::unique_ptr<PathedCamera> m_cameraSystem;
	bool m_cameraSystemStarted = false;
	bool m_firstFramePending = false;
};



