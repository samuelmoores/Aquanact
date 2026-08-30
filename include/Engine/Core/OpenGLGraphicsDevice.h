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
class Frustum;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	struct FrameStats {
		std::size_t mainDrawCalls = 0;
		std::size_t shadowDrawCalls = 0;
		std::size_t directionalShadowDrawCalls = 0;
		std::array<std::size_t, LightingManager::MaxPointLights> pointShadowDrawCalls{};
		std::uint64_t mainTriangles = 0;
		std::uint64_t shadowTriangles = 0;
		std::uint64_t directionalShadowTriangles = 0;
		std::array<std::uint64_t, LightingManager::MaxPointLights> pointShadowTriangles{};
		double mainVisibilityTestMs = 0.0;
		double shadowPassMs = 0.0;
	};
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
	void DrawCulled(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum& frustum,
		std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes);
	void DrawSelected(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager);
	void DrawSelectionOutline(const RenderCommand& command, const Camera& camera);
	const FrameStats& Stats() const { return m_frameStats; }

private:
	void startUp() override;
	void InitializeShadowMap();
	void ReleaseShadowMap();
	void DrawInternal(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum* frustum,
		std::size_t* visibleSubMeshes, std::size_t* culledSubMeshes);
	std::unique_ptr<OpenGLGraphics> m_platform;
	std::unique_ptr<class ShaderProgram> m_shadowShader;
	std::unique_ptr<class ShaderProgram> m_pointShadowShader;
	std::unique_ptr<class ShaderProgram> m_selectionOutlineShader;
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
	FrameStats m_frameStats;
};


