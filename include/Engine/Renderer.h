#pragma once
#include <Engine/GLHeaders.h>
#include <memory>

#include "Engine/Mesh.h"
#include "Engine/Camera.h"

#include "Engine/RenderCommand.h"


class Renderer {
public:
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();



private:

	std::vector<RenderCommand> commands;

};


