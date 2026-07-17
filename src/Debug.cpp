#include "Debug.h"

#include "Axis.h"
#include "EngineGUI.h"
#include "Grid.h"
#include "Camera.h"
#include "Input.h"
#include "Globals.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "FrameAllocator.h"
#include "GLHeaders.h"

#include <imgui.h>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#endif

void Debug::startUp()
{
	// The debug subsystem owns the editor overlay primitives and records
	// early build/dependency diagnostics before the rest of the app starts running.
	m_axis = new Axis(1200.0f);
	RebuildGrid();
	LogBuildInfo();
	VerifyDependencies();
}

void Debug::shutDown()
{
	// Release overlay helpers first so they cannot outlive the renderer or scene data.
	delete m_axis;
	m_axis = nullptr;
	delete m_grid;
	m_grid = nullptr;
	m_logMessages.clear();
	m_logOnceKeys.clear();
}

void Debug::RebuildGrid()
{
	// Grid settings are mutable in the editor, so rebuild the helper when size or spacing changes.
	delete m_grid;
	m_grid = new Grid(m_gridSize, m_gridSpacing);
}

void Debug::draw(const Camera& camera, const EngineGUI& gui)
{
	// This runs only in editor mode and is responsible for the visible debugging overlay.
	// It intentionally reads current frame data instead of caching renderer state itself.
	static bool firstFrame = true;
	static auto lastFrameTime = std::chrono::high_resolution_clock::now();
	const auto now = std::chrono::high_resolution_clock::now();
	if (firstFrame)
	{
		firstFrame = false;
		m_lastFps = 0.0f;
	}
	else
	{
		const std::chrono::duration<float> dt = now - lastFrameTime;
		m_lastFps = (dt.count() > 0.0f) ? (1.0f / dt.count()) : 0.0f;
	}
	lastFrameTime = now;

	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();

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

	ImGui::Begin("Debug Log");
	if (ImGui::Button("Clear"))
	{
		m_logMessages.clear();
	}
	ImGui::Separator();
	for (const std::string& message : m_logMessages)
	{
		ImGui::TextUnformatted(message.c_str());
	}
	ImGui::End();

	ImGui::Begin("Debug Stats");
	ImGui::Text("FPS: %.1f", m_lastFps);
	ImGui::Text("Scene objects: %zu", gSceneManager.Objects().size());
	ImGui::Separator();
	ImGui::Text("Render commands: %zu", gRenderManager.LastFrameCommandCount());
	ImGui::Text("Skipped objects: %zu", gRenderManager.LastFrameSkippedObjects());
	ImGui::Text("Build time: %.4f ms", gRenderManager.LastFrameBuildMs());
	ImGui::Text("Flush time: %.4f ms", gRenderManager.LastFrameFlushMs());
	ImGui::Separator();
	ImGui::Text("Frame allocator capacity: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorCapacityBytes()) / 1024.0);
	ImGui::Text("Frame allocator used: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorUsedBytes()) / 1024.0);
	ImGui::Text("Frame allocator peak: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorPeakBytes()) / 1024.0);
	ImGui::End();
}

void Debug::drawGameModeInput(const Input& input)
{
	// Game mode uses this panel to show live input and gameplay state without the editor UI.
	ImGui::Begin("Game Input");
	ImGui::Text("Window focused: %s", input.WindowFocused() ? "yes" : "no");
	ImGui::Text("Look active: %s", input.LookActive() ? "yes" : "no");
	ImGui::Text("Look became active: %s", input.LookBecameActive() ? "yes" : "no");
	const glm::vec3 move = input.MoveInput();
	const glm::vec2 mouse = input.MouseDelta();
	ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f", move.x, move.z);
	ImGui::Text("Mouse delta: %.2f, %.2f", mouse.x, mouse.y);
	ImGui::Text("Delta time: %.4f", input.DeltaTime());
	ImGui::End();

	ImGui::Begin("Gameplay Diagnostics");
	ImGui::Text("Controller registered: %s", m_controllerRegistered ? "yes" : "no");
	ImGui::Text("Object: %s", m_gameplayObjectName.empty() ? "<none>" : m_gameplayObjectName.c_str());
	ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f, %.2f", m_gameplayMoveInput.x, m_gameplayMoveInput.y, m_gameplayMoveInput.z);
	ImGui::Text("Move speed: %.2f", m_gameplayMoveSpeed);
	ImGui::Text("Delta time: %.4f", m_gameplayDt);
	ImGui::Text("Applied delta: %.3f, %.3f, %.3f", m_gameplayDelta.x, m_gameplayDelta.y, m_gameplayDelta.z);
	ImGui::Text("Position: %.3f, %.3f, %.3f", m_gameplayPosition.x, m_gameplayPosition.y, m_gameplayPosition.z);
	ImGui::End();
}

void Debug::SetGameplayDiagnostics(bool controllerRegistered, const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position)
{
	// Gameplay systems push their latest state here so the debug overlay can show it without
	// reaching back into the controller or object layer every frame.
	m_controllerRegistered = controllerRegistered;
	m_gameplayObjectName = objectName;
	m_gameplayMoveInput = moveInput;
	m_gameplayMoveSpeed = moveSpeed;
	m_gameplayDt = dt;
	m_gameplayDelta = delta;
	m_gameplayPosition = position;
}

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

void Debug::SetGridSettings(float size, float spacing)
{
	if (size <= 0.0f)
	{
		size = 1.0f;
	}

	if (spacing <= 0.0f)
	{
		spacing = 1.0f;
	}

	m_gridSize = size;
	m_gridSpacing = spacing;
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
