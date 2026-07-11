#pragma once

#include <glm/glm.hpp>

class Camera;
struct RenderCommand;

class GraphicsDevice {
public:
	virtual ~GraphicsDevice() = default;

	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual void BeginFrame() = 0;
	virtual void Clear(float r, float g, float b, float a) = 0;
	virtual void EndFrame() = 0;

	virtual void Draw(const RenderCommand& command, const Camera& camera) = 0;
};
