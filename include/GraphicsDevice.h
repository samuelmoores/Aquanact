#pragma once

#include <glm/glm.hpp>

class Camera;
class LightingManager;
struct RenderCommand;

class GraphicsDevice {
public:
	virtual ~GraphicsDevice() = default;

	virtual void startUp() = 0;
	virtual void shutDown() = 0;

	virtual void BeginFrame() = 0;
	virtual void Clear(float r, float g, float b, float a) = 0;
	virtual void EndFrame() = 0;

	virtual void Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager) = 0;
};
