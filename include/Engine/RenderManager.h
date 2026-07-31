#pragma once
#include <cstddef>
#include <chrono>
#include <memory>
#include <Engine/GLHeaders.h>

#include "Engine/FrameAllocator.h"
#include "Engine/Camera.h"
#include "Engine/EngineCamera.h"
#include "Engine/GameCamera.h"
#include "Engine/CameraManager.h"
#include "Engine/RenderCommand.h"
#include "Engine/OpenGLGraphicsDevice.h"
#include "Engine/LightingManager.h"
#include "Engine/ProjectStateData.h"

class Window;
class LevelManager;
class FrontEndManager;
class FileManager;
class ProjectManager;
class Debug;
class Input;
class EngineState;

class RenderManager {
public:
	RenderManager() = default;
	~RenderManager();
	void startUp(Window& window);
	void shutDown();
	// Const and non-const accessors let callers read the camera from const code
	// while still allowing mutable access where the renderer owns the state.
	EngineCamera& GetEngineCamera();
	const EngineCamera& GetEngineCamera() const;
	GameCamera& GetGameCamera();
	const GameCamera& GetGameCamera() const;
	void SetEditorMode();
	void SetGameMode();
	void SetActiveCamera(Camera& camera);
	Camera& ActiveCamera();
	const Camera& ActiveCamera() const;
	LightingManager& Lights() { return *m_lightingManager; }
	const LightingManager& Lights() const { return *m_lightingManager; }
	void ApplyProjectState(const ProjectStateData::RenderStateData& renderState);
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop(FrontEndManager& frontEndManager, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager, Debug& debug, Input& input, Window& window, EngineState& engineState);
	void UpdateCameraPhase(const Input& input, const EngineState& engineState);

	std::size_t LastFrameCommandCount() const;
	std::size_t LastFrameSkippedObjects() const;
	double LastFrameBuildMs() const;
	double LastFrameFlushMs() const;
	double LastFrameDebugOverlayMs() const;
	double LastFrameEditorGuiMs() const;
	double LastFrameUiCreatorMs() const;
	double LastFrameRuntimeGuiMs() const;
	std::size_t FrameAllocatorCapacityBytes() const;
	std::size_t FrameAllocatorUsedBytes() const;
	std::size_t FrameAllocatorPeakBytes() const;

private:
	void ResetFrameState();
	void BuildRenderCommands(FrontEndManager& frontEndManager, LevelManager& levelManager, EngineState& engineState);
	void DrawEditorFrame(FrontEndManager& frontEndManager, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager, Debug& debug);
	void DrawRuntimeFrame(FrontEndManager& frontEndManager, Debug& debug, Input& input);
	void DrawFrame(FrontEndManager& frontEndManager, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager, Debug& debug, Input& input, EngineState& engineState);
	bool ShouldPreviewMainMenu(const FrontEndManager& frontEndManager, EngineState& engineState) const;
	void ApplyCameraMode(const EngineState& engineState);
	void BeginFrame();
	void PresentFrame(Window& window);

	std::unique_ptr<EngineCamera> m_engineCamera;
	std::unique_ptr<GameCamera> m_gameCamera;
	CameraManager m_cameraManager;
	OpenGLGraphicsDevice m_device;
	FrameAllocator m_frameAllocator;
	RenderCommand* m_commands = nullptr;
	std::unique_ptr<LightingManager> m_lightingManager = nullptr;

	//debug
	std::size_t m_commandCapacity = 0;
	std::size_t m_commandCount = 0;
	std::size_t m_lastFrameCommandCount = 0;
	std::size_t m_lastFrameSkippedObjects = 0;
	std::chrono::duration<double, std::milli> m_lastFrameBuildTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameDebugOverlayTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameEditorGuiTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameUiCreatorTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameRuntimeGuiTime{ 0.0 };
};


