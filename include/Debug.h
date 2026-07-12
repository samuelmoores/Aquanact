#pragma once

#include <string>
#include <vector>

class Camera;
class Input;
class EngineGUI;

class Debug {
public:
	Debug() = default;
	void startUp();
	void shutDown();
	void draw(const Camera& camera, const EngineGUI& gui);
	void LogMessage(const std::string& message);
	void LogFrame(const Camera& camera, const Input& input);
	void SetLoggingEnabled(bool enabled);
	void SetGridSettings(float size, float spacing);
	float GridSize() const;
	float GridSpacing() const;

private:
	void RebuildGrid();

	class Axis* m_axis = nullptr;
	class Grid* m_grid = nullptr;
	float m_gridSize = 1200.0f;
	float m_gridSpacing = 50.0f;
	bool m_loggingEnabled = false;
	int m_logFrameCount = 0;
	std::vector<std::string> m_logMessages;
};
