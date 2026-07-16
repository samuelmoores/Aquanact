#pragma once

#include "glm/glm.hpp"
#include <string>
#include <vector>

class Camera;
class EngineGUI;
class Input;
class Object3D;

class Debug {
public:
	Debug() = default;
	void startUp();
	void shutDown();
	void draw(const Camera& camera, const EngineGUI& gui);
	void drawGameModeInput(const Input& input);
	void SetGameplayDiagnostics(bool controllerRegistered, const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position);
	void LogMessage(const std::string& message);
	void SetGridSettings(float size, float spacing);
	float GridSize() const;
	float GridSpacing() const;

private:
	void RebuildGrid();

	class Axis* m_axis = nullptr;
	class Grid* m_grid = nullptr;
	float m_gridSize = 1200.0f;
	float m_gridSpacing = 50.0f;
	float m_lastFps = 0.0f;
	bool m_controllerRegistered = false;
	std::string m_gameplayObjectName;
	glm::vec3 m_gameplayMoveInput{ 0.0f };
	float m_gameplayMoveSpeed = 0.0f;
	float m_gameplayDt = 0.0f;
	glm::vec3 m_gameplayDelta{ 0.0f };
	glm::vec3 m_gameplayPosition{ 0.0f };
	std::vector<std::string> m_logMessages;
};
