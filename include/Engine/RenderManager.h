#pragma once
#include <cstddef>
#include <chrono>
#include <memory>
#include <Engine/GLHeaders.h>

#include "Engine/FrameAllocator.h"
#include "Engine/Camera.h"
#include "Engine/EngineCamera.h"
#include "Engine/GameCamera.h"
#include "Engine/RenderCommand.h"
#include "Engine/OpenGLGraphicsDevice.h"
#include "Engine/LightingManager.h"

class Window;
class SceneManager;

class RenderManager {
public:
	RenderManager() = default;
	~RenderManager();
	void startUp(Window& window);
	void shutDown();
	// Const and non-const accessors let callers read the camera from const code
	// while still allowing mutable access where the renderer owns the state.
	EngineCamera& GetEngineCamera() { return *m_engineCamera; }
	const EngineCamera& GetEngineCamera() const { return *m_engineCamera; }
	GameCamera& GetGameCamera() { return *m_gameCamera; }
	const GameCamera& GetGameCamera() const { return *m_gameCamera; }
	void SetActiveCamera(Camera& camera) { m_activeCamera = &camera; }
	Camera& ActiveCamera() { return *m_activeCamera; }
	const Camera& ActiveCamera() const { return *m_activeCamera; }
	LightingManager& Lights() { return *m_lightingManager; }
	const LightingManager& Lights() const { return *m_lightingManager; }
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();

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
	std::unique_ptr<EngineCamera> m_engineCamera;
	std::unique_ptr<GameCamera> m_gameCamera;
	Camera* m_activeCamera = nullptr;
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


