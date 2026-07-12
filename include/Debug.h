#pragma once

#include <string>
#include <vector>

class Camera;
class EngineGUI;

class Debug {
public:
	Debug() = default;
	void startUp();
	void shutDown();
	void draw(const Camera& camera, const EngineGUI& gui);
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
	std::vector<std::string> m_logMessages;
};
