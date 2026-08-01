#pragma once
#include "Engine/Core/GLHeaders.h"
#include <memory>

#include "Engine/Core/Mesh.h"
#include "Engine/Core/Camera.h"

#include "Engine/Core/RenderCommand.h"


class Renderer {
public:
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(const Camera& camera);
	void Loop();



private:

	std::vector<RenderCommand> commands;

};



