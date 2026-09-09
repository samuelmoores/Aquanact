#pragma once

#include <glm/glm.hpp>

class SceneManager;

class SpawnManagerWindow final
{
public:
	void Draw(SceneManager& scenes, bool& open);

private:
	glm::vec3 m_position{ 0.0f };
	glm::vec3 m_rotation{ 0.0f };
};
