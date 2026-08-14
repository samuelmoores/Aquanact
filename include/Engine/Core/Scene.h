#pragma once

#include "Engine/Core/Entity.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
	const std::vector<std::unique_ptr<Entity>>& Entities() const { return m_entities; }
	const std::vector<std::unique_ptr<Entity>>& Objects() const { return m_entities; }

private:
	std::string m_name;
	std::string m_musicPath;
	float m_musicVolume = 50.0f;
	std::vector<std::unique_ptr<Entity>> m_entities;
	bool m_firstFramePending = false;
};



