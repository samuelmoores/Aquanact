#pragma once

#include "GraphicsDevice.h"

class Window;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	explicit OpenGLGraphicsDevice(Window& window);
	~OpenGLGraphicsDevice() override;

	void Initialize() override;
	void Shutdown() override;

	void BeginFrame() override;
	void Clear(float r, float g, float b, float a) override;
	void EndFrame() override;

	void Draw(const RenderCommand& command, const Camera& camera) override;

private:
	Window& m_window;
	bool m_initialized = false;
};
