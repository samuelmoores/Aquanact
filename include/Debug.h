#pragma once

class Camera;
class Input;
class EngineGUI;

class Debug {
public:
	Debug() = default;
	void startUp();
	void shutDown();
	void draw(const Camera& camera, const EngineGUI& gui);
	void LogFrame(const Camera& camera, const Input& input);
	void SetLoggingEnabled(bool enabled);

private:
	class Axis* m_axis = nullptr;
	class Grid* m_grid = nullptr;
	bool m_loggingEnabled = false;
	int m_logFrameCount = 0;
};
