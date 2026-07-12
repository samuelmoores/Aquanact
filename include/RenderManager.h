#pragma once
#include <cstddef>
#include <GLHeaders.h>

#include "Camera.h"
#include "RenderCommand.h"

class GraphicsDevice;

class RenderManager {
public:
	RenderManager() = default;
	void startUp(GraphicsDevice& device);
	void shutDown();
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();

private:
	GraphicsDevice* m_device = nullptr;
	std::vector<RenderCommand> commands;
};
