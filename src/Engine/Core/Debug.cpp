#include "Engine/Core/Debug.h"

#include "Engine/Core/Axis.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/Core/Grid.h"
#include "Engine/Core/Line.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/FrameAllocator.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/GameCamera.h"
#include "Engine/Core/CameraCollider.h"
#include "Engine/Core/Mesh.h"

#include <imgui.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {
	const auto g_programStartTime = std::chrono::high_resolution_clock::now();

	std::vector<LineVertex3D> MakeWireSphereVertices(const glm::vec3& color)
	{
		constexpr int segments = 48;
		constexpr float pi = 3.14159265358979323846f;
		std::vector<LineVertex3D> vertices;
		vertices.reserve(segments * 2 * 3);

		auto addVertex = [&](const glm::vec3& position) {
			vertices.push_back({ position.x, position.y, position.z, color.r, color.g, color.b });
		};

		for (int i = 0; i < segments; ++i)
		{
			const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * pi;
			const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * pi;

			addVertex(glm::vec3(std::cos(a0), std::sin(a0), 0.0f));
			addVertex(glm::vec3(std::cos(a1), std::sin(a1), 0.0f));

			addVertex(glm::vec3(std::cos(a0), 0.0f, std::sin(a0)));
			addVertex(glm::vec3(std::cos(a1), 0.0f, std::sin(a1)));

			addVertex(glm::vec3(0.0f, std::cos(a0), std::sin(a0)));
			addVertex(glm::vec3(0.0f, std::cos(a1), std::sin(a1)));
		}

		return vertices;
	}
}

void Debug::startUp()
{
	if (!Root::Current().State().IsEditorMode())
	{
		return;
	}

	// The debug subsystem owns the editor overlay primitives and records
	// early build/dependency diagnostics before the rest of the app starts running.
	RebuildAxis();
	RebuildGrid();
	LogBuildInfo();
	VerifyDependencies();
}

void Debug::shutDown()
{
	// Release overlay helpers first so they cannot outlive the renderer or Scene data.
	delete m_axis;
	m_axis = nullptr;
	delete m_grid;
	m_grid = nullptr;
	for (Line* sphere : m_pointLightDebugSpheres)
	{
		delete sphere;
	}
	m_pointLightDebugSpheres.clear();
	m_pointLightDebugColors.clear();
	delete m_cameraCollisionSphere;
	m_cameraCollisionSphere = nullptr;
	m_logMessages.clear();
	m_logOnceKeys.clear();
}

void Debug::DrawCameraCollisionDebug(const Camera& camera)
{
	if (!m_showCameraCollisionDebug)
	{
		return;
	}

	const GameCamera& gameCamera = Root::Current().Render().GetGameCamera();
	const CameraCollider& collider = gameCamera.Collider();
	if (!m_cameraCollisionSphere)
	{
		m_cameraCollisionSphere = new Line(MakeWireSphereVertices(glm::vec3(1.0f, 1.0f, 0.0f)));
	}

	m_cameraCollisionSphere->UpdateProjection(camera.GetProjectionMatrix());
	m_cameraCollisionSphere->draw(
		camera.GetViewMatrix(),
		glm::translate(glm::mat4(1.0f), collider.Position()) * glm::scale(glm::mat4(1.0f), glm::vec3(collider.Radius())));
}

void Debug::RebuildGrid()
{
	// Grid settings are mutable in the editor, so rebuild the helper when size or spacing changes.
	delete m_grid;
	m_grid = new Grid(m_gridSize, m_gridSpacing);
}

void Debug::RebuildAxis()
{
	delete m_axis;
	m_axis = new Axis(m_axisLength);
}

void Debug::RebuildPointLightDebugSpheres()
{
	for (Line* sphere : m_pointLightDebugSpheres)
	{
		delete sphere;
	}
	m_pointLightDebugSpheres.clear();
	m_pointLightDebugColors.clear();

	for (const PointLight& pointLight : Root::Current().Render().Lights().PointLights())
	{
		const glm::vec3 color = glm::clamp(pointLight.color, glm::vec3(0.0f), glm::vec3(1.0f));
		m_pointLightDebugSpheres.push_back(new Line(MakeWireSphereVertices(color)));
		m_pointLightDebugColors.push_back(color);
	}
}

void Debug::draw(const Camera& camera, const EngineGUI& gui)
{
	// This runs only in editor mode and is responsible for the visible debugging overlay.
	// It intentionally reads current frame data instead of caching renderer state itself.
	static bool firstFrame = true;
	if (firstFrame)
	{
		firstFrame = false;
		m_lastFps = 0.0f;
		if (m_startupToFirstDrawMs < 0.0)
		{
			const auto elapsed = std::chrono::high_resolution_clock::now() - g_programStartTime;
			m_startupToFirstDrawMs = std::chrono::duration<double>(elapsed).count();
		}
	}
	else
	{
		m_lastFps = static_cast<float>(Root::Current().Profiler().SmoothedFps());
	}

	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();
	DrawCameraCollisionDebug(camera);

	if (m_axis && gui.ShowAxis())
	{
		m_axis->UpdateProjection(projection);
		m_axis->draw(view);
	}

	if (m_grid && gui.ShowGrid())
	{
		m_grid->UpdateProjection(projection);
		m_grid->draw(view, !gui.ShowAxis());
	}

	const std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
	bool rebuildLightSpheres = m_pointLightDebugSpheres.size() != pointLights.size();
	if (!rebuildLightSpheres)
	{
		for (std::size_t i = 0; i < pointLights.size(); ++i)
		{
			const glm::vec3 color = glm::clamp(pointLights[i].color, glm::vec3(0.0f), glm::vec3(1.0f));
			if (m_pointLightDebugColors[i] != color)
			{
				rebuildLightSpheres = true;
				break;
			}
		}
	}

	if (rebuildLightSpheres)
	{
		RebuildPointLightDebugSpheres();
	}

	for (std::size_t i = 0; i < pointLights.size() && i < m_pointLightDebugSpheres.size(); ++i)
	{
		Line* sphere = m_pointLightDebugSpheres[i];
		if (!sphere)
		{
			continue;
		}

		const PointLight& pointLight = pointLights[i];
		const float markerRadius = std::clamp(pointLight.radius * 0.03f, 15.0f, 80.0f);
		const glm::mat4 model =
			glm::translate(glm::mat4(1.0f), pointLight.position) *
			glm::scale(glm::mat4(1.0f), glm::vec3(markerRadius));

		sphere->UpdateProjection(projection);
		glLineWidth(2.0f);
		sphere->draw(view, model);
	}

	if (m_showLogWindow)
	{
		ImGui::Begin("Debug Log", &m_showLogWindow, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
		if (ImGui::Button("Clear Logs"))
		{
			ClearLogs();
		}
		ImGui::Separator();
		for (const std::string& message : m_logMessages)
		{
			ImGui::TextUnformatted(message.c_str());
		}
		ImGui::End();
	}

	if (m_showStatsWindow)
	{
		ImGui::Begin("Debug Stats", &m_showStatsWindow, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
		ImGui::Text("FPS: %.1f", m_lastFps);
		const Scene* activeLevel = Root::Current().Levels().ActiveLevel();
		ImGui::Text("Scene objects: %zu", activeLevel ? activeLevel->Objects().size() : 0);
		ImGui::Separator();
		ImGui::Text("Render commands: %zu", Root::Current().Render().LastFrameCommandCount());
		ImGui::Text("Skipped objects: %zu", Root::Current().Render().LastFrameSkippedObjects());
		ImGui::Text("Build time: %.4f ms", Root::Current().Render().LastFrameBuildMs());
		ImGui::Text("Flush time: %.4f ms", Root::Current().Render().LastFrameFlushMs());
		ImGui::Separator();
		ImGui::Text("Debug overlay: %.4f ms", Root::Current().Render().LastFrameDebugOverlayMs());
		ImGui::Text("Editor GUI/MyGUI: %.4f ms", Root::Current().Render().LastFrameEditorGuiMs());
		ImGui::Text("UI creator: %.4f ms", Root::Current().Render().LastFrameUiCreatorMs());
		ImGui::Text("Runtime GUI: %.4f ms", Root::Current().Render().LastFrameRuntimeGuiMs());
		ImGui::Text("Startup to first draw: %.2f s", m_startupToFirstDrawMs);
		ImGui::Separator();
		ImGui::Text("Frame allocator capacity: %.2f KB", static_cast<double>(Root::Current().Render().FrameAllocatorCapacityBytes()) / 1024.0);
		ImGui::Text("Frame allocator used: %.2f KB", static_cast<double>(Root::Current().Render().FrameAllocatorUsedBytes()) / 1024.0);
		ImGui::Text("Frame allocator peak: %.2f KB", static_cast<double>(Root::Current().Render().FrameAllocatorPeakBytes()) / 1024.0);
		ImGui::End();
	}
}

void Debug::drawGameModeInput(const Input& input)
{
	// Game mode uses this panel to show live input and gameplay state without the editor UI.
	m_lastFps = static_cast<float>(Root::Current().Profiler().SmoothedFps());

	if (m_showGameInputWindow)
	{
		bool open = m_showGameInputWindow;
		ImGui::Begin("Game Input", &open);
	ImGui::Text("FPS: %.1f", m_lastFps);
	ImGui::Text("Frame: %.3f ms", Root::Current().Profiler().FrameMs());
	ImGui::Text("Window focused: %s", input.WindowFocused() ? "yes" : "no");
	ImGui::Text("Look active: %s", input.LookActive() ? "yes" : "no");
	ImGui::Text("Look became active: %s", input.LookBecameActive() ? "yes" : "no");
	ImGui::Text("Engine mode: %s", Root::Current().State().IsGameMode() ? "Game" : "Editor");
	ImGui::Text("Gameplay flow: %s",
		Root::Current().Gameplay().State() == GameplayManager::GameState::MainMenu ? "MainMenu" :
		Root::Current().Gameplay().State() == GameplayManager::GameState::Playing ? "Playing" :
		Root::Current().Gameplay().State() == GameplayManager::GameState::Paused ? "Paused" : "Unknown");
	ImGui::Text("Runtime UI mode: %s",
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::MainMenu ? "MainMenu" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::GameplayHUD ? "GameplayHUD" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::PauseMenu ? "PauseMenu" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::PlayerUI ? "PlayerUI" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::Custom ? "Custom" : "Unknown");
	const Scene* activeLevel = Root::Current().Levels().ActiveLevel();
	ImGui::Text("Active Scene: %s", activeLevel ? activeLevel->Name().c_str() : "<none>");
	ImGui::Text("Active Scene objects: %zu", activeLevel ? activeLevel->Objects().size() : 0);
	ImGui::Text("Controller components: %zu", Root::Current().Gameplay().ControllerCount());
	const glm::vec3 move = input.MoveInput();
	const glm::vec2 mouse = input.MouseDelta();
	ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f", move.x, move.z);
	ImGui::Text("Mouse delta: %.2f, %.2f", mouse.x, mouse.y);
		ImGui::Text("Delta time: %.4f", input.DeltaTime());
		ImGui::Checkbox("Camera Collision Debug", &m_showCameraCollisionDebug);
	ImGui::Separator();
	ImGui::Separator();
	ImGui::TextUnformatted("Recent logs:");
	const std::size_t logCount = m_logMessages.size();
	const std::size_t startIndex = logCount > 5 ? logCount - 5 : 0;
	for (std::size_t i = startIndex; i < logCount; ++i)
	{
		ImGui::BulletText("%s", m_logMessages[i].c_str());
	}
		ImGui::End();
		m_showGameInputWindow = open;
	}

	if (m_showPhysicsDiagnosticsWindow)
	{
		bool open = m_showPhysicsDiagnosticsWindow;
		ImGui::Begin("Physics Diagnostics", &open);
		ImGui::Text("Camera: %.3f, %.3f, %.3f", m_physicsCameraPosition.x, m_physicsCameraPosition.y, m_physicsCameraPosition.z);
		ImGui::Text("Desired: %.3f, %.3f, %.3f", m_physicsDesiredPosition.x, m_physicsDesiredPosition.y, m_physicsDesiredPosition.z);
		ImGui::Text("Resolved: %.3f, %.3f, %.3f", m_physicsResolvedPosition.x, m_physicsResolvedPosition.y, m_physicsResolvedPosition.z);
		ImGui::Text("Collider radius: %.3f", m_physicsColliderRadius);
		ImGui::Text("Collision count: %d", m_physicsCollisionCount);
		ImGui::Text("Last object: %s", m_physicsCollisionObject.empty() ? "<none>" : m_physicsCollisionObject.c_str());
		ImGui::Text("Normal: %.3f, %.3f, %.3f", m_physicsCollisionNormal.x, m_physicsCollisionNormal.y, m_physicsCollisionNormal.z);
		ImGui::Text("Penetration: %.3f", m_physicsPenetration);
		ImGui::End();
		m_showPhysicsDiagnosticsWindow = open;
	}

	if (Root::Current().Profiler().IsEnabled())
	{
		bool open = true;
		ImGui::Begin("Profiler", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
		for (const FrameProfiler::Sample& sample : Root::Current().Profiler().Samples())
		{
			ImGui::Text(
				"%-16s current %7.3f ms | avg %7.3f ms | max %7.3f ms",
				sample.name.c_str(),
				sample.currentMs,
				sample.averageMs,
				sample.maximumMs);
		}
		ImGui::End();
		if (!open)
		{
			Root::Current().Profiler().SetEnabled(false);
		}
	}

	if (m_showGameplayDiagnosticsWindow)
	{
		bool open = m_showGameplayDiagnosticsWindow;
		ImGui::Begin("Gameplay Diagnostics", &open);
	ImGui::Text("FPS: %.1f", m_lastFps);
	ImGui::Text("Frame: %.3f ms", Root::Current().Profiler().FrameMs());
	ImGui::Text("Controller owner bound: %s", m_controllerOwnerBound ? "yes" : "no");
	ImGui::Text("Object: %s", m_gameplayObjectName.empty() ? "<none>" : m_gameplayObjectName.c_str());
	ImGui::Text("Active Scene: %s", m_activeLevelName.empty() ? "<none>" : m_activeLevelName.c_str());
	ImGui::Text("Engine mode: %s", m_engineMode.empty() ? "<none>" : m_engineMode.c_str());
	ImGui::Text("Active Scene objects: %zu", m_activeLevelObjects);
	ImGui::Text("Controller component count: %zu", m_controllerCount);
	ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f, %.2f", m_gameplayMoveInput.x, m_gameplayMoveInput.y, m_gameplayMoveInput.z);
	ImGui::Text("Move speed: %.2f", m_gameplayMoveSpeed);
	ImGui::Text("Delta time: %.4f", m_gameplayDt);
	ImGui::Text("Applied delta: %.3f, %.3f, %.3f", m_gameplayDelta.x, m_gameplayDelta.y, m_gameplayDelta.z);
	ImGui::Text("Position: %.3f, %.3f, %.3f", m_gameplayPosition.x, m_gameplayPosition.y, m_gameplayPosition.z);
		ImGui::End();
		m_showGameplayDiagnosticsWindow = open;
	}

	if (m_showAnimationDiagnosticsWindow)
	{
		bool open = m_showAnimationDiagnosticsWindow;
		ImGui::Begin("Animation Diagnostics", &open);
	ImGui::Text("Current state: %s", m_animationCurrentState.empty() ? "<none>" : m_animationCurrentState.c_str());
	ImGui::Text("Desired state: %s", m_animationDesiredState.empty() ? "<none>" : m_animationDesiredState.c_str());
	ImGui::TextWrapped("Last transition: %s", m_animationLastTransitionDebug.empty() ? "<none>" : m_animationLastTransitionDebug.c_str());
	ImGui::Text("Last transition from: %s", m_animationLastTransitionFrom.empty() ? "<none>" : m_animationLastTransitionFrom.c_str());
	ImGui::Text("Last transition to: %s", m_animationLastTransitionTo.empty() ? "<none>" : m_animationLastTransitionTo.c_str());
	ImGui::Text("Last condition: %s %s %s",
		m_animationLastTransitionLeftOperandText.empty() ? "<none>" : m_animationLastTransitionLeftOperandText.c_str(),
		m_animationLastTransitionComparatorText.empty() ? "<none>" : m_animationLastTransitionComparatorText.c_str(),
		m_animationLastTransitionRightOperandText.empty() ? "<none>" : m_animationLastTransitionRightOperandText.c_str());
	ImGui::Text("Resolved operand values: %.3f %s %.3f",
		m_animationLastTransitionLeftValue,
		m_animationLastTransitionComparatorText.empty() ? "<none>" : m_animationLastTransitionComparatorText.c_str(),
		m_animationLastTransitionRightValue);
	ImGui::Text("Condition passed: %s", m_animationLastTransitionPassed ? "yes" : "no");
	ImGui::Text("Resolved target: %s", m_animationLastResolvedTargetState.empty() ? "<none>" : m_animationLastResolvedTargetState.c_str());
	ImGui::Text("Resolved clip index: %d", m_animationLastResolvedTargetClipIndex);
	ImGui::Text("Resolved target found: %s", m_animationLastResolvedTargetFound ? "yes" : "no");
	ImGui::Separator();
	ImGui::TextUnformatted("States:");
	ImGui::BeginChild("AnimatorStateList", ImVec2(0.0f, 120.0f), true);
	ImGui::TextUnformatted(m_animationStateListText.empty() ? "<none>" : m_animationStateListText.c_str());
	ImGui::EndChild();
		ImGui::End();
		m_showAnimationDiagnosticsWindow = open;
	}
}

bool Debug::ShowCameraCollisionDebug() const { return m_showCameraCollisionDebug; }
void Debug::SetShowCameraCollisionDebug(bool show) { m_showCameraCollisionDebug = show; }
bool Debug::ShowPhysicsDiagnosticsWindow() const { return m_showPhysicsDiagnosticsWindow; }
void Debug::SetShowPhysicsDiagnosticsWindow(bool show) { m_showPhysicsDiagnosticsWindow = show; }

void Debug::SetPhysicsDiagnostics(const glm::vec3& cameraPosition, const glm::vec3& desiredPosition, const glm::vec3& resolvedPosition, float colliderRadius, int collisionCount, const glm::vec3& collisionNormal, float penetration, const std::string& collisionObject)
{
	m_physicsCameraPosition = cameraPosition;
	m_physicsDesiredPosition = desiredPosition;
	m_physicsResolvedPosition = resolvedPosition;
	m_physicsColliderRadius = colliderRadius;
	m_physicsCollisionCount = collisionCount;
	m_physicsCollisionNormal = collisionNormal;
	m_physicsPenetration = penetration;
	m_physicsCollisionObject = collisionObject;
	if (collisionCount > 0 && (++m_physicsDiagnosticsFrame % 30u) == 0u)
	{
		LogTagged("Physics", "Camera collision count=" + std::to_string(collisionCount) +
			" object=" + (collisionObject.empty() ? std::string("<unknown>") : collisionObject) +
			" normal=(" + std::to_string(collisionNormal.x) + "," + std::to_string(collisionNormal.y) + "," + std::to_string(collisionNormal.z) + ")" +
			" penetration=" + std::to_string(penetration) +
			" cameraY=" + std::to_string(cameraPosition.y) +
			" desiredY=" + std::to_string(desiredPosition.y) +
			" resolvedY=" + std::to_string(resolvedPosition.y));
	}
}

void Debug::SetGameplayDiagnostics(const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position)
{
	// Gameplay systems push their latest state here so the debug overlay can show it without
	// reaching back into the controller or object layer every frame.
	m_controllerOwnerBound = objectName != "<unbound>";
	m_gameplayObjectName = objectName;
	m_gameplayMoveInput = moveInput;
	m_gameplayMoveSpeed = moveSpeed;
	m_gameplayDt = dt;
	m_gameplayDelta = delta;
	m_gameplayPosition = position;
}

void Debug::SetAnimationDiagnostics(const std::string& currentState, const std::string& desiredState, const std::string& lastTransitionDebug, const std::string& lastTransitionFrom, const std::string& lastTransitionTo, const std::string& lastTransitionLeftOperandText, const std::string& lastTransitionComparatorText, const std::string& lastTransitionRightOperandText, float lastTransitionLeftValue, float lastTransitionRightValue, bool lastTransitionPassed, const std::string& lastResolvedTargetState, int lastResolvedTargetClipIndex, bool lastResolvedTargetFound, const std::string& stateListText)
{
	m_animationCurrentState = currentState;
	m_animationDesiredState = desiredState;
	m_animationLastTransitionDebug = lastTransitionDebug;
	m_animationLastTransitionFrom = lastTransitionFrom;
	m_animationLastTransitionTo = lastTransitionTo;
	m_animationLastTransitionLeftOperandText = lastTransitionLeftOperandText;
	m_animationLastTransitionComparatorText = lastTransitionComparatorText;
	m_animationLastTransitionRightOperandText = lastTransitionRightOperandText;
	m_animationLastTransitionLeftValue = lastTransitionLeftValue;
	m_animationLastTransitionRightValue = lastTransitionRightValue;
	m_animationLastTransitionPassed = lastTransitionPassed;
	m_animationLastResolvedTargetState = lastResolvedTargetState;
	m_animationLastResolvedTargetClipIndex = lastResolvedTargetClipIndex;
	m_animationLastResolvedTargetFound = lastResolvedTargetFound;
	m_animationStateListText = stateListText;
}

void Debug::SetGameplayContext(const std::string& activeLevelName, std::size_t activeLevelObjects, std::size_t controllerCount, const std::string& engineMode)
{
	m_activeLevelName = activeLevelName;
	m_activeLevelObjects = activeLevelObjects;
	m_controllerCount = controllerCount;
	m_engineMode = engineMode;
}

bool Debug::ShowLogWindow() const { return m_showLogWindow; }
bool Debug::ShowStatsWindow() const { return m_showStatsWindow; }
void Debug::SetShowLogWindow(bool showLogWindow) { m_showLogWindow = showLogWindow; }
void Debug::SetShowStatsWindow(bool showStatsWindow) { m_showStatsWindow = showStatsWindow; }
bool Debug::ShowGameInputWindow() const { return m_showGameInputWindow; }
void Debug::SetShowGameInputWindow(bool show) { m_showGameInputWindow = show; }
bool Debug::ShowGameplayDiagnosticsWindow() const { return m_showGameplayDiagnosticsWindow; }
void Debug::SetShowGameplayDiagnosticsWindow(bool show) { m_showGameplayDiagnosticsWindow = show; }
bool Debug::ShowAnimationDiagnosticsWindow() const { return m_showAnimationDiagnosticsWindow; }
void Debug::SetShowAnimationDiagnosticsWindow(bool show) { m_showAnimationDiagnosticsWindow = show; }

void Debug::LogMessage(const std::string& message)
{
	LogMessage(Severity::Info, message);
}

std::string Debug::SeverityPrefix(Severity severity) const
{
	switch (severity)
	{
	case Severity::Warning: return "[WARN] ";
	case Severity::Error: return "[ERROR] ";
	case Severity::Fatal: return "[FATAL] ";
	case Severity::Info:
	default:
		return "[INFO] ";
	}
}

void Debug::LogMessage(Severity severity, const std::string& message)
{
	// Keep the log buffer bounded so the UI remains responsive even if a subsystem is noisy.
	m_logMessages.push_back(SeverityPrefix(severity) + message);
	if (m_logMessages.size() > 180)
	{
		m_logMessages.erase(m_logMessages.begin());
	}
}

void Debug::LogTagged(const std::string& tag, const std::string& message)
{
	LogTagged(Severity::Info, tag, message);
}

void Debug::LogTagged(Severity severity, const std::string& tag, const std::string& message)
{
	// Tags group related messages together without requiring a more complicated logging backend.
	LogMessage(severity, "[" + tag + "] " + message);
}

void Debug::LogOnce(const std::string& key, const std::string& message)
{
	// One-shot messages are useful for startup diagnostics and repeating states that should only be reported once.
	if (std::find(m_logOnceKeys.begin(), m_logOnceKeys.end(), key) != m_logOnceKeys.end())
	{
		return;
	}

	m_logOnceKeys.push_back(key);
	LogMessage(message);
}

void Debug::ClearLogs()
{
	m_logMessages.clear();
	m_logOnceKeys.clear();
}

void Debug::LogException(const std::string& context, const std::exception& ex)
{
	// Exceptions are normalized into a consistent error format for the log window.
	LogMessage(Severity::Error, context + ": " + ex.what());
}

bool Debug::Ensure(bool condition, const std::string& context, const std::string& message)
{
	// Use this when failure is non-fatal but still important enough to surface immediately.
	if (condition)
	{
		return true;
	}

	LogMessage(Severity::Error, context + ": " + message);
	return false;
}

bool Debug::CheckOpenGLError(const std::string& context)
{
	// glGetError is stateful, so this is a point-in-time check meant to be called around suspicious GL calls.
	GLenum error = glGetError();
	if (error == GL_NO_ERROR)
	{
		return false;
	}

	std::string message = context + ": OpenGL error 0x" + std::to_string(static_cast<unsigned int>(error));
	LogMessage(Severity::Error, message);
	return true;
}

void Debug::LogBuildInfo()
{
	// These messages are only useful at startup, when you need to confirm the binary
	// was compiled with the expected toolchain, source root, and dependency set.
#ifdef AQUANACT_SOURCE_ROOT
	LogTagged("Build", std::string("Source root: ") + AQUANACT_SOURCE_ROOT);
#endif
#ifdef _MSC_VER
	LogTagged("Build", "MSVC version: " + std::to_string(_MSC_VER));
#endif
#ifdef _MSVC_LANG
	LogTagged("Build", "MSVC language mode: " + std::to_string(_MSVC_LANG));
#endif
#ifdef _DEBUG
	LogTagged("Build", "Configuration: Debug");
#else
	LogTagged("Build", "Configuration: Release");
#endif
#ifdef AQUANACT_VCPKG_TARGET_TRIPLET
	LogTagged("Build", std::string("vcpkg triplet: ") + AQUANACT_VCPKG_TARGET_TRIPLET);
#endif
#ifdef AQUANACT_MYGUI_ENGINE_LIBRARY
	LogTagged("Dependency", std::string("MyGUI engine: ") + AQUANACT_MYGUI_ENGINE_LIBRARY);
#endif
#ifdef AQUANACT_MYGUI_OPENGL_LIBRARY
	LogTagged("Dependency", std::string("MyGUI OpenGL: ") + AQUANACT_MYGUI_OPENGL_LIBRARY);
#endif
#ifdef AQUANACT_FREETYPE_LIBRARY
	LogTagged("Dependency", std::string("FreeType: ") + AQUANACT_FREETYPE_LIBRARY);
#endif
}

void Debug::VerifyDependencies()
{
#ifndef _WIN32
	// The current implementation only probes Windows-style DLL dependencies.
	LogTagged(Severity::Warning, "Dependency", "Runtime dependency verification is only implemented for Windows.");
	return;
#else
	// LoadLibraryA here is used as a cheap runtime smoke test:
	// if the DLL cannot be loaded from the process search path, the app logs a clear warning.
	struct DependencyProbe
	{
		const char* displayName;
		const char* dllName;
	};

	const DependencyProbe probes[] =
	{
		{ "SFML system", "sfml-system-d-2.dll" },
		{ "SFML window", "sfml-window-d-2.dll" },
		{ "SFML graphics", "sfml-graphics-d-2.dll" },
		{ "SFML audio", "sfml-audio-d-2.dll" },
		{ "GLFW", "glfw3.dll" },
		{ "FreeType", "freetyped.dll" },
		{ "MyGUI OpenGL", "MyGUI.OpenGLPlatform_d.dll" }
	};

	for (const DependencyProbe& probe : probes)
	{
		HMODULE module = LoadLibraryA(probe.dllName);
		if (!module)
		{
			// This is not fatal by itself, but it means a dependency is missing from the runtime environment.
			LogDependencyHint(probe.displayName, std::string("missing runtime DLL '") + probe.dllName + "'");
			continue;
		}

		FreeLibrary(module);
	}
#endif
}

void Debug::LogDependencyHint(const std::string& dependency, const std::string& details)
{
	// Keep dependency warnings visually distinct from normal build/runtime logs.
	LogTagged(Severity::Warning, "Dependency", dependency + ": " + details);
}

void Debug::SetGridSettings(float size)
{
	if (size <= 0.0f)
	{
		size = 1.0f;
	}

	m_gridSize = size;
	m_gridSpacing = (size / 24.0f > 1.0f) ? (size / 24.0f) : 1.0f;
	m_axisLength = m_gridSize;
	RebuildAxis();
	RebuildGrid();
}

float Debug::GridSize() const
{
	return m_gridSize;
}

float Debug::GridSpacing() const
{
	return m_gridSpacing;
}

double Debug::StartupToFirstDrawMs() const
{
	return m_startupToFirstDrawMs;
}





