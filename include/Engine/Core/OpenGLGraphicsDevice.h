#pragma once

#include "Engine/Core/GraphicsDevice.h"
#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/LightingManager.h"

#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class Window;
class LightingManager;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	OpenGLGraphicsDevice() = default;
	~OpenGLGraphicsDevice() override;
	void startUp(Window& window);
	void shutDown() override;

	void BeginFrame() override;
	void Clear(float r, float g, float b, float a) override;
	void EndFrame() override;
	void ConfigureDefaultState();
	// Called immediately before GUI submission so overlays do not inherit scene GL state.
	void ConfigureGuiState();
	void RenderShadowMaps(const RenderCommand* commands, std::size_t commandCount, const LightingManager& lightingManager);

	void Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager) override;

private:
	void startUp() override;
	void InitializeShadowMap();
	void ReleaseShadowMap();
	std::unique_ptr<OpenGLGraphics> m_platform;
	std::unique_ptr<class ShaderProgram> m_shadowShader;
	std::unique_ptr<class ShaderProgram> m_pointShadowShader;
	Window* m_window = nullptr;
	uint32_t m_shadowFramebuffer = 0;
	uint32_t m_shadowDepthTexture = 0;
	uint32_t m_pointShadowFramebuffer = 0;
	std::array<uint32_t, LightingManager::MaxPointLights> m_pointShadowDepthTextures{};
	std::array<float, LightingManager::MaxPointLights> m_pointShadowFarPlanes{};
	std::array<bool, LightingManager::MaxPointLights> m_pointShadowMapsReady{};
	glm::mat4 m_lightSpaceMatrix{ 1.0f };
	bool m_shadowMapReady = false;
	bool m_initialized = false;
};


