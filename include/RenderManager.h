#pragma once
#include <cstddef>
#include <chrono>
#include <GLHeaders.h>

#include "FrameAllocator.h"
#include "Camera.h"
#include "RenderCommand.h"

class GraphicsDevice;
class SceneManager;

class RenderManager {
public:
	RenderManager() = default;
	void startUp(GraphicsDevice& device);
	void shutDown();
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
	GraphicsDevice* m_device = nullptr;
	FrameAllocator m_frameAllocator;
	RenderCommand* m_commands = nullptr;
	std::size_t m_commandCapacity = 0;
	std::size_t m_commandCount = 0;
	std::size_t m_lastFrameCommandCount = 0;
	std::size_t m_lastFrameSkippedObjects = 0;
	std::chrono::duration<double, std::milli> m_lastFrameBuildTime{ 0.0 };
	std::chrono::duration<double, std::milli> m_lastFrameFlushTime{ 0.0 };
};
