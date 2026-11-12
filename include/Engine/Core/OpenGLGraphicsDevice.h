#pragma once

#include "Engine/Core/GraphicsDevice.h"
#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/LightingManager.h"

#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class Window;
class LightingManager;
class Frustum;
struct ParticleRenderCommand;
class ShaderProgram;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	OpenGLGraphicsDevice() = default;
	~OpenGLGraphicsDevice() override;
	void startUp(Window& window) override;
	void shutDown() override;

	void ConfigureDefaultState() override;
	void ConfigureGuiState() override;
	void BeginFrame() override;
	void Clear(float r, float g, float b, float a) override;
	void EndFrame() override;
	// Called immediately before GUI submission so overlays do not inherit scene GL state.
	bool SetSplashTexture(const std::string& path) override;
	void RenderShadowMaps(const RenderCommand* commands, std::size_t commandCount,
		const LightingManager& lightingManager) override;

	void Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager) override;
	void DrawParticles(const ParticleRenderCommand* commands, std::size_t commandCount,
		const Camera& camera, const LightingManager& lightingManager) override;
	void DrawCulled(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum& frustum,
		std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes) override;
	void DrawSelected(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager);
	void DrawSelectionOutline(const RenderCommand& command, const Camera& camera);
	const FrameStats& Stats() const override { return m_frameStats; }

private:
	void startUp();
	void InitializeShadowMap();
	void ReleaseShadowMap();
	void DrawInternal(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum* frustum,
		std::size_t* visibleSubMeshes, std::size_t* culledSubMeshes);
	std::unique_ptr<OpenGLGraphics> m_platform;
	std::unique_ptr<class ShaderProgram> m_shadowShader;
	std::unique_ptr<class ShaderProgram> m_pointShadowShader;
	std::unique_ptr<class ShaderProgram> m_selectionOutlineShader;
	std::unique_ptr<class ShaderProgram> m_particleShader;
	Window* m_window = nullptr;
	uint32_t m_shadowFramebuffer = 0;
	uint32_t m_shadowDepthTexture = 0;
	uint32_t m_pointShadowFramebuffer = 0;
	std::array<uint32_t, GraphicsDevice::FrameStats::MaxPointLights> m_pointShadowDepthTextures{};
	std::array<float, LightingManager::MaxPointLights> m_pointShadowFarPlanes{};
	std::array<bool, GraphicsDevice::FrameStats::MaxPointLights> m_pointShadowMapsReady{};
	glm::mat4 m_lightSpaceMatrix{ 1.0f };
	bool m_shadowMapReady = false;
	bool m_initialized = false;
	uint32_t m_particleVao = 0;
	uint32_t m_particleVbo = 0;
	int32_t m_particleModelUniform = -1;
	int32_t m_particleViewUniform = -1;
	int32_t m_particleProjectionUniform = -1;
	int32_t m_particleViewportSizeUniform = -1;
	int32_t m_particleVisualShapeUniform = -1;
	int32_t m_particleSplashTextureUniform = -1;
	int32_t m_particleSplashBrightnessUniform = -1;
	int32_t m_particleSplashOpacityUniform = -1;
	uint32_t m_particleSplashTexture = 0;
	GraphicsDevice::FrameStats m_frameStats;
};


