#pragma once
#include <cstddef>
#include <chrono>
#include <memory>
#include "Engine/Core/GLHeaders.h"

#include "Engine/Core/FrameAllocator.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/CameraManager.h"
#include "Engine/Core/RenderCommand.h"
#include "Engine/Core/OpenGLGraphicsDevice.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/OcclusionCullingSystem.h"
#include "Engine/Core/ProjectStateData.h"

class Window;
class SceneManager;
class FrontEndManager;
class FileManager;
class ProjectManager;
class Debug;
class Input;
class EngineState;

class RenderManager {
public:
	enum class CameraMode
	{
		ThirdPerson,
	};

	RenderManager() = default;
	~RenderManager();
	void startUp(Window& window);
	void shutDown();
	// Const and non-const accessors let callers read the camera from const code
	// while still allowing mutable access where the renderer owns the state.
	EngineCamera& GetEngineCamera();
	const EngineCamera& GetEngineCamera() const;
	PathedCamera& GetPathedCamera();
	const PathedCamera& GetPathedCamera() const;
	void SetEditorMode();
	void SetGameMode();
	void SetCameraMode(CameraMode mode);
	CameraMode CameraModeValue() const { return m_cameraMode; }
	void SetActiveCamera(Camera& camera);
	Camera& ActiveCamera();
	const Camera& ActiveCamera() const;
	LightingManager& Lights() { return *m_lightingManager; }
	const LightingManager& Lights() const { return *m_lightingManager; }
	void ApplyProjectState(const ProjectStateData::RenderStateData& renderState);
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera, unsigned int selectedEntityId = 0);
	void Loop(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug, Input& input, Window& window, EngineState& engineState);
	void UpdateCameraPhase(const Input& input, const EngineState& engineState);
	// Clear non-owning entity references before a scene destroys its entities.
	void ClearPathedCameraTarget();

	std::size_t LastFrameCommandCount() const;
	std::size_t LastFrameSkippedObjects() const;
	std::size_t LastFrameFrustumCulledObjects() const;
	std::size_t LastFrameOcclusionCulledObjects() const { return m_lastFrameOcclusionCulledObjects; }
	std::size_t LastFrameDrawCallsSaved() const;
	double LastFrameBuildMs() const;
	double LastFrameFlushMs() const;
	double LastFrameFlushCandidateCountMs() const;
	double LastFrameFlushShadowMs() const;
	double LastFrameFlushOccluderPrepassMs() const;
	double LastFrameFlushFrustumSetupMs() const;
	double LastFrameFlushMainPassMs() const;
	double LastFrameFlushCleanupMs() const;
	double LastFrameDebugOverlayMs() const;
	double LastFrameEditorGuiMs() const;
	double LastFrameUiCreatorMs() const;
	double LastFrameRuntimeGuiMs() const;
	std::size_t FrameAllocatorCapacityBytes() const;
	std::size_t FrameAllocatorUsedBytes() const;
	std::size_t FrameAllocatorPeakBytes() const;
	const OpenGLGraphicsDevice::FrameStats& LastFrameGraphicsStats() const { return m_device.Stats(); }
	bool OcclusionCullingEnabled() const { return m_occlusionCullingEnabled; }
	void SetOcclusionCullingEnabled(bool enabled);
	bool OcclusionQueriesEnabled() const { return m_occlusionQueriesEnabled; }
	void SetOcclusionQueriesEnabled(bool enabled) { m_occlusionQueriesEnabled = enabled; }
	bool OcclusionMasksEnabled() const { return m_occlusionMasksEnabled; }
	void SetOcclusionMasksEnabled(bool enabled) { m_occlusionMasksEnabled = enabled; }
	const OcclusionCullingSystem::Stats& OcclusionStats() const { return m_occlusionCulling.CurrentStats(); }
	const OcclusionVisibilityHistory::Stats& OcclusionVisibilityStats() const
	{
		return m_occlusionCulling.VisibilityStats();
	}
	std::uint64_t OcclusionQueryGeneration() const { return m_occlusionCulling.QueryGeneration(); }
	bool OcclusionCameraMoving() const { return m_occlusionCulling.CameraMoving(); }

private:
	void ResetFrameState();
	void BuildRenderCommands(FrontEndManager& frontEndManager, SceneManager& SceneManager, EngineState& engineState);
	void DrawEditorFrame(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug);
	void DrawRuntimeFrame(FrontEndManager& frontEndManager, Debug& debug, Input& input);
	void DrawFrame(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug, Input& input, EngineState& engineState);
	bool ShouldPreviewMainMenu(const FrontEndManager& frontEndManager, EngineState& engineState) const;
	void ApplyCameraMode(const EngineState& engineState);
	void BeginFrame();
	void PresentFrame(Window& window);

	std::unique_ptr<EngineCamera> m_engineCamera;
	std::unique_ptr<PathedCamera> m_gameCamera;
	Entity* m_cameraTarget = nullptr;
	glm::vec3 m_cameraLastTargetPosition{0.0f};
	float m_cameraPlayerProgress = 0.0f;
	bool m_hasCameraTargetPosition = false;
	bool m_engineCameraPathInitialized = false;
	CameraManager m_cameraManager;
	CameraMode m_cameraMode = CameraMode::ThirdPerson;
	OpenGLGraphicsDevice m_device;
	FrameAllocator m_frameAllocator;
	RenderCommand* m_commands = nullptr;
	std::unique_ptr<LightingManager> m_lightingManager = nullptr;
	OcclusionCullingSystem m_occlusionCulling;
	bool m_occlusionCullingEnabled = false;
	bool m_occlusionQueriesEnabled = true;
	bool m_occlusionMasksEnabled = true;

	//debug
	std::size_t m_commandCapacity = 0;
	std::size_t m_commandCount = 0;
	std::size_t m_lastFrameCommandCount = 0;
	std::size_t m_lastFrameSkippedObjects = 0;
	std::size_t m_lastFrameFrustumCulledObjects = 0;
	std::size_t m_lastFrameOcclusionCulledObjects = 0;
	std::size_t m_lastFrameDrawCallsSaved = 0;
	std::chrono::duration<double, std::milli> m_lastFrameBuildTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushCandidateCountTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushShadowTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushOccluderPrepassTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushFrustumSetupTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushMainPassTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushCleanupTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameDebugOverlayTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameEditorGuiTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameUiCreatorTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameRuntimeGuiTime{ 0.0 };
};




