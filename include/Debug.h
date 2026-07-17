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
	// Severity is a lightweight categorization for log messages.
	// It lets the overlay distinguish normal information from warnings,
	// recoverable errors, and fatal conditions that usually need immediate attention.
	enum class Severity
	{
		Info,
		Warning,
		Error,
		Fatal
	};

	Debug() = default;
	// Startup owns the debug overlay data structures and initializes any
	// persistent diagnostics that should be available as soon as the app boots.
	void startUp();
	// Shutdown releases the grid/axis helpers and clears the in-memory log buffer.
	void shutDown();
	// Editor-mode overlay: draws axis/grid plus the visible debug windows.
	void draw(const Camera& camera, const EngineGUI& gui);
	// Game-mode overlay: shows only the runtime input and gameplay diagnostics.
	void drawGameModeInput(const Input& input);
	// Cached controller/object state that powers the gameplay diagnostic panel.
	void SetGameplayDiagnostics(bool controllerRegistered, const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position);
	// Basic logging writes to the in-memory debug log window.
	void LogMessage(const std::string& message);
	// Severity-aware logging prefixes messages so they can be visually filtered.
	void LogMessage(Severity severity, const std::string& message);
	// Tags are a simple way to group logs by subsystem like Render, GUI, Build, or IO.
	void LogTagged(const std::string& tag, const std::string& message);
	// Tagged + severity-aware logging is useful for warnings and errors from a subsystem.
	void LogTagged(Severity severity, const std::string& tag, const std::string& message);
	// One-shot logging avoids repeated spam from conditions that only matter once.
	void LogOnce(const std::string& key, const std::string& message);
	// Standard wrapper for exceptions caught at subsystem boundaries.
	void LogException(const std::string& context, const std::exception& ex);
	// Convenience helper for "if this is false, log an error and continue".
	bool Ensure(bool condition, const std::string& context, const std::string& message);
	// Checks glGetError() and reports any pending OpenGL failure at a specific point.
	bool CheckOpenGLError(const std::string& context);
	// Logs build-time configuration values that are compiled into the binary.
	void LogBuildInfo();
	// Probes common runtime DLLs so missing dependency problems are visible at startup.
	void VerifyDependencies();
	// Emits a dependency-specific warning that explains what is missing or misconfigured.
	void LogDependencyHint(const std::string& dependency, const std::string& details);
	void SetGridSettings(float size);
	float GridSize() const;
	float GridSpacing() const;

private:
	// Rebuilds the axis/grid helpers when the grid configuration changes.
	void RebuildGrid();
	void RebuildAxis();
	// Small formatting helper used by the log window.
	std::string SeverityPrefix(Severity severity) const;

	class Axis* m_axis = nullptr;
	class Grid* m_grid = nullptr;
	float m_axisLength = 1200.0f;
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
	std::vector<std::string> m_logOnceKeys;
};
