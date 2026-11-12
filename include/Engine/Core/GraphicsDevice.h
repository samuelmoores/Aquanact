#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class Frustum;
class LightingManager;
class Window;
struct RenderCommand;
struct ParticleRenderCommand;

enum class GraphicsBackend
{
	OpenGL
};

class GraphicsDevice {
public:
	struct FrameStats {
		static constexpr std::size_t MaxPointLights = 8;
		std::size_t mainDrawCalls = 0;
		std::size_t shadowDrawCalls = 0;
		std::size_t directionalShadowDrawCalls = 0;
		std::array<std::size_t, MaxPointLights> pointShadowDrawCalls{};
		std::uint64_t mainTriangles = 0;
		std::uint64_t shadowTriangles = 0;
		std::uint64_t directionalShadowTriangles = 0;
		std::array<std::uint64_t, MaxPointLights> pointShadowTriangles{};
		double mainVisibilityTestMs = 0.0;
		double shadowPassMs = 0.0;
	};

	virtual ~GraphicsDevice() = default;

	virtual void startUp(Window& window) = 0;
	virtual void shutDown() = 0;

	virtual void ConfigureDefaultState() = 0;
	virtual void ConfigureGuiState() = 0;
	virtual void BeginFrame() = 0;
	virtual void Clear(float r, float g, float b, float a) = 0;
	virtual void EndFrame() = 0;
	virtual bool SetSplashTexture(const std::string& path) = 0;
	virtual void RenderShadowMaps(const RenderCommand* commands, std::size_t commandCount,
		const LightingManager& lightingManager) = 0;

	virtual void Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager) = 0;
	virtual void DrawParticles(const ParticleRenderCommand* commands, std::size_t commandCount,
		const Camera& camera, const LightingManager& lightingManager) = 0;
	virtual void DrawCulled(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum& frustum,
		std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes) = 0;
	virtual const FrameStats& Stats() const = 0;
};

std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(GraphicsBackend backend);

