#pragma once
#include <GLHeaders.h>
#include <memory>

#include "Camera.h"
#include "RenderCommand.h"

class GraphicsDevice;

class RenderManager {
public:
	explicit RenderManager(GraphicsDevice& device);
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(Camera* camera);
	void Loop();

private:
	GraphicsDevice& m_device;
	std::vector<RenderCommand> commands;
};
