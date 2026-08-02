#pragma once

#include "glm/glm.hpp"
#include <string>
#include <vector>

class Camera;
class EngineGUI;
class Input;
class Entity;
class GameplayManager;

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
	void SetGameplayDiagnostics(const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position);
	// Cached animation state shown in the gameplay debugger.
	void SetAnimationDiagnostics(const std::string& currentState, const std::string& desiredState, const std::string& lastTransitionDebug, const std::string& lastTransitionFrom, const std::string& lastTransitionTo, const std::string& lastTransitionLeftOperandText, const std::string& lastTransitionComparatorText, const std::string& lastTransitionRightOperandText, float lastTransitionLeftValue, float lastTransitionRightValue, bool lastTransitionPassed, const std::string& lastResolvedTargetState, int lastResolvedTargetClipIndex, bool lastResolvedTargetFound, const std::string& stateListText);
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
	void ClearLogs();
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
	double StartupToFirstDrawMs() const;
	void SetGameplayContext(const std::string& activeLevelName, std::size_t activeLevelObjects, std::size_t controllerCount, const std::string& engineMode);
	bool ShowLogWindow() const;
	bool ShowStatsWindow() const;
	void SetShowLogWindow(bool showLogWindow);
	void SetShowStatsWindow(bool showStatsWindow);
	bool ShowGameInputWindow() const;
	void SetShowGameInputWindow(bool show);
	bool ShowGameplayDiagnosticsWindow() const;
	void SetShowGameplayDiagnosticsWindow(bool show);
	bool ShowAnimationDiagnosticsWindow() const;
	void SetShowAnimationDiagnosticsWindow(bool show);
	bool ShowCameraCollisionDebug() const;
	void SetShowCameraCollisionDebug(bool show);
	void DrawCameraCollisionDebug(const Camera& camera);
	void SetPhysicsDiagnostics(const glm::vec3& cameraPosition, const glm::vec3& desiredPosition, const glm::vec3& resolvedPosition, float colliderRadius, int collisionCount, const glm::vec3& collisionNormal, float penetration, const std::string& collisionObject);
	bool ShowPhysicsDiagnosticsWindow() const;
	void SetShowPhysicsDiagnosticsWindow(bool show);

private:
	// Rebuilds the axis/grid helpers when the grid configuration changes.
	void RebuildGrid();
	void RebuildAxis();
	void RebuildPointLightDebugSpheres();
	void ClearEntityBoundingBoxes();
	// Small formatting helper used by the log window.
	std::string SeverityPrefix(Severity severity) const;

	class Axis* m_axis = nullptr;
	class Grid* m_grid = nullptr;
	std::vector<class Line*> m_pointLightDebugSpheres;
	std::vector<class Line*> m_entityBoundingBoxes;
	std::vector<class Entity*> m_entityBoundingBoxObjects;
	class Line* m_cameraCollisionSphere = nullptr;
	std::vector<glm::vec3> m_pointLightDebugColors;
	float m_axisLength = 1200.0f;
	float m_gridSize = 1200.0f;
	float m_gridSpacing = 50.0f;
	float m_lastFps = 0.0f;
	double m_startupToFirstDrawMs = -1.0;
	bool m_showLogWindow = false;
	bool m_showStatsWindow = false;
	bool m_showGameInputWindow = true;
	bool m_showGameplayDiagnosticsWindow = true;
	bool m_showAnimationDiagnosticsWindow = true;
	bool m_showCameraCollisionDebug = false;
	bool m_showPhysicsDiagnosticsWindow = true;
	unsigned int m_physicsDiagnosticsFrame = 0;
	glm::vec3 m_physicsCameraPosition{ 0.0f };
	glm::vec3 m_physicsDesiredPosition{ 0.0f };
	glm::vec3 m_physicsResolvedPosition{ 0.0f };
	glm::vec3 m_physicsCollisionNormal{ 0.0f };
	float m_physicsColliderRadius = 0.0f;
	float m_physicsPenetration = 0.0f;
	int m_physicsCollisionCount = 0;
	std::string m_physicsCollisionObject;
	bool m_controllerOwnerBound = false;
	std::string m_gameplayObjectName;
	std::string m_activeLevelName;
	std::string m_engineMode;
	std::size_t m_activeLevelObjects = 0;
	std::size_t m_controllerCount = 0;
	glm::vec3 m_gameplayMoveInput{ 0.0f };
	float m_gameplayMoveSpeed = 0.0f;
	float m_gameplayDt = 0.0f;
	glm::vec3 m_gameplayDelta{ 0.0f };
	glm::vec3 m_gameplayPosition{ 0.0f };
	std::string m_animationCurrentState;
	std::string m_animationDesiredState;
	std::string m_animationLastTransitionDebug;
	std::string m_animationLastTransitionFrom;
	std::string m_animationLastTransitionTo;
	std::string m_animationLastTransitionLeftOperandText;
	std::string m_animationLastTransitionComparatorText;
	std::string m_animationLastTransitionRightOperandText;
	float m_animationLastTransitionLeftValue = 0.0f;
	float m_animationLastTransitionRightValue = 0.0f;
	bool m_animationLastTransitionPassed = false;
	std::string m_animationLastResolvedTargetState;
	int m_animationLastResolvedTargetClipIndex = -1;
	bool m_animationLastResolvedTargetFound = false;
	std::string m_animationStateListText;
	std::vector<std::string> m_logMessages;
	std::vector<std::string> m_logOnceKeys;
};

