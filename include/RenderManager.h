#pragma once
#include <GLHeaders.h>
#include <memory>

#include "Mesh.h"
#include "Camera.h"
#include "RenderCommand.h"

class RenderManager {
public:
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(Camera* camera);
	void Loop();

private:
	std::vector<RenderCommand> commands;
};
