#pragma once

#include "GraphicsDevice.h"
#include "OpenGLGraphics.h"

#include <memory>

class Window;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	OpenGLGraphicsDevice() = default;
	~OpenGLGraphicsDevice() override;
	void startUp(Window& window);
	void shutDown() override;

	void BeginFrame() override;
	void Clear(float r, float g, float b, float a) override;
	void EndFrame() override;

	void Draw(const RenderCommand& command, const Camera& camera) override;

private:
	void startUp() override;
	std::unique_ptr<OpenGLGraphics> m_platform;
	Window* m_window = nullptr;
	bool m_initialized = false;
};
