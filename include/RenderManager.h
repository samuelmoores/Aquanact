#pragma once
#include <cstddef>
#include <chrono>
#include <memory>
#include <GLHeaders.h>

#include "FrameAllocator.h"
#include "Camera.h"
#include "EngineCamera.h"
#include "GameCamera.h"
#include "RenderCommand.h"
#include "OpenGLGraphicsDevice.h"

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
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();

	std::size_t LastFrameCommandCount() const;
	std::size_t LastFrameSkippedObjects() const;
	double LastFrameBuildMs() const;
	double LastFrameFlushMs() const;
	std::size_t FrameAllocatorCapacityBytes() const;
	std::size_t FrameAllocatorUsedBytes() const;
	std::size_t FrameAllocatorPeakBytes() const;

private:
	std::unique_ptr<EngineCamera> m_engineCamera;
	std::unique_ptr<GameCamera> m_gameCamera;
	OpenGLGraphicsDevice m_device;
	FrameAllocator m_frameAllocator;
	RenderCommand* m_commands = nullptr;
	std::size_t m_commandCapacity = 0;
	std::size_t m_commandCount = 0;
	std::size_t m_lastFrameCommandCount = 0;
	std::size_t m_lastFrameSkippedObjects = 0;
	std::chrono::duration<double, std::milli> m_lastFrameBuildTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushTime{ 0.0 };
};
