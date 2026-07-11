#pragma once
#include <GLHeaders.h>
#include <memory>

#include "Mesh.h"
#include "Camera.h"

#include "RenderCommand.h"


class Renderer {
public:
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();



private:

	std::vector<RenderCommand> commands;

};
