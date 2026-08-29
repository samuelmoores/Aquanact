#pragma once

#include "Engine/Core/GraphicsDevice.h"
#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/OcclusionBvh.h"

#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Window;
class LightingManager;
class Frustum;
struct SelectedOccluder;

class OpenGLGraphicsDevice final : public GraphicsDevice {
public:
	struct OcclusionQueryResult {
		std::uint32_t nodeIndex = OcclusionBvh::InvalidIndex;
		std::uint64_t generation = 0;
		bool visible = true;
	};

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
		double occluderPrepassMs = 0.0;
		std::size_t occluderPrepassDrawCalls = 0;
		std::uint64_t occluderPrepassTriangles = 0;
		std::size_t occlusionQueriesIssued = 0;
		std::size_t occlusionQueriesPending = 0;
		std::size_t occlusionQueryResults = 0;
		std::size_t occlusionVisibleResults = 0;
		std::size_t occlusionOccludedResults = 0;
		double occlusionQueryPollMs = 0.0;
		double occlusionQueryIssueMs = 0.0;
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
	void RenderOccluderDepth(const RenderCommand* commands, std::size_t commandCount,
		const std::vector<SelectedOccluder>& occluders, const Camera& camera);
	void PollOcclusionQueries(std::vector<OcclusionQueryResult>& results);
	void IssueOcclusionQueries(const OcclusionBvh& bvh,
		const std::vector<std::uint32_t>& nodeIndices, std::uint64_t generation,
		const Camera& camera);

	void Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager) override;
	void DrawCulled(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum& frustum,
		std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes,
		const std::vector<std::uint8_t>* occlusionVisibility = nullptr,
		std::size_t* occlusionCulledSubMeshes = nullptr);
	void DrawSelected(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager);
	void DrawSelectionOutline(const RenderCommand& command, const Camera& camera);
	const FrameStats& Stats() const { return m_frameStats; }
	int ViewportWidth() const;
	int ViewportHeight() const;

private:
	void startUp() override;
	void InitializeShadowMap();
	void ReleaseShadowMap();
	void InitializeOcclusionQueries();
	void ReleaseOcclusionQueries();
	void DrawInternal(const RenderCommand& command, const Camera& camera,
		const LightingManager& lightingManager, const Frustum* frustum,
		std::size_t* visibleSubMeshes, std::size_t* culledSubMeshes,
		const std::vector<std::uint8_t>* occlusionVisibility = nullptr,
		std::size_t* occlusionCulledSubMeshes = nullptr);
	std::unique_ptr<OpenGLGraphics> m_platform;
	std::unique_ptr<class ShaderProgram> m_shadowShader;
	std::unique_ptr<class ShaderProgram> m_pointShadowShader;
	std::unique_ptr<class ShaderProgram> m_selectionOutlineShader;
	std::unique_ptr<class ShaderProgram> m_occlusionBoundsShader;
	struct QuerySlot {
		std::uint32_t id = 0;
		std::uint32_t nodeIndex = OcclusionBvh::InvalidIndex;
		std::uint64_t generation = 0;
		bool pending = false;
	};
	std::vector<QuerySlot> m_occlusionQueryPool;
	std::uint32_t m_occlusionBoundsVao = 0;
	std::uint32_t m_occlusionBoundsVbo = 0;
	std::uint32_t m_occlusionBoundsEbo = 0;
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


