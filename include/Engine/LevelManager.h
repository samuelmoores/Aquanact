#pragma once

#include "Engine/Level.h"

#include <memory>
#include <string>
#include <vector>

class LevelManager
{
public:
	LevelManager() = default;
	~LevelManager();

	Level* startUp();
	void Clear();
	Level* CreateLevel(std::string name);
	Level* FindLevel(const std::string& name) const;
	bool SetActiveLevel(const std::string& name);
	Level* ActiveLevel();
	const Level* ActiveLevel() const;
	const std::vector<std::unique_ptr<Level>>& Levels() const { return m_levels; }

private:
	std::vector<std::unique_ptr<Level>> m_levels;
	Level* m_activeLevel = nullptr;
};
