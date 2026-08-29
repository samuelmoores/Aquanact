#pragma once

#include "Engine/Core/OcclusionBvh.h"
#include "Engine/Core/OccluderSelector.h"
#include "Engine/Core/OcclusionVisibilityHistory.h"

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct RenderCommand;

class OcclusionCullingSystem {
public:
	struct Stats {
		std::size_t candidateItems = 0;
		std::size_t stableItems = 0;
		std::size_t nodes = 0;
		std::size_t leaves = 0;
		std::size_t maxDepth = 0;
		std::size_t selectedOccluders = 0;
		std::uint64_t selectedOccluderTriangles = 0;
		std::uint64_t rebuildCount = 0;
		std::uint64_t cameraInvalidationCount = 0;
		double updateMs = 0.0;
		double rebuildMs = 0.0;
	};

	// Builds conservative static-geometry candidates from the render commands.
	// This does not make visibility decisions or issue OpenGL queries.
	static std::vector<OcclusionItem> BuildItems(
		const RenderCommand* commands, std::size_t commandCount);

	void Update(const RenderCommand* commands, std::size_t commandCount,
		std::size_t targetLeafSize = 12);
	void Clear();
	void SelectOccluders(const glm::mat4& viewProjection, int viewportWidth,
		int viewportHeight, const OccluderSelectionConfig& config = {});
	void UpdateCamera(const glm::vec3& position, const glm::vec3& facing,
		const glm::mat4& projection);
	void ApplyQueryResult(std::uint32_t nodeIndex, std::uint64_t generation, bool visible);
	bool IsOccluded(unsigned int entityId, int subMeshIndex) const;
	std::uint64_t QueryGeneration() const { return m_queryGeneration; }
	bool CameraMoving() const { return m_cameraMoving; }
	bool ShouldIssueQueries() const { return (m_frame % 4u) == 0u; }
	const OcclusionVisibilityHistory::Stats& VisibilityStats() const
	{
		return m_visibilityHistory.CurrentStats();
	}
	const OcclusionBvh& Bvh() const { return m_bvh; }
	const std::vector<SelectedOccluder>& SelectedOccluders() const { return m_selectedOccluders; }
	const Stats& CurrentStats() const { return m_stats; }

private:
	struct TransformHistory {
		glm::mat4 modelMatrix{ 1.0f };
		std::uint32_t stableFrames = 0;
		std::uint64_t lastSeenFrame = 0;
	};
	struct CommandSignature {
		unsigned int entityId = 0;
		std::uintptr_t meshAddress = 0;
		int bufferCount = 0;
		glm::mat4 modelMatrix{ 1.0f };
		glm::vec3 worldBoundsMin{ 0.0f };
		glm::vec3 worldBoundsMax{ 0.0f };
		bool hasWorldBounds = false;
		bool isSkinned = false;
	};

	bool ItemsChanged(const std::vector<OcclusionItem>& items) const;

	OcclusionBvh m_bvh;
	std::unordered_map<unsigned int, TransformHistory> m_transformHistory;
	std::vector<OcclusionItem> m_previousStableItems;
	std::vector<CommandSignature> m_previousSignatures;
	std::vector<SelectedOccluder> m_selectedOccluders;
	Stats m_stats;
	std::uint64_t m_frame = 0;
	OcclusionVisibilityHistory m_visibilityHistory;
	std::uint64_t m_queryGeneration = 0;
	glm::vec3 m_lastCameraPosition{ 0.0f };
	glm::vec3 m_lastCameraFacing{ 0.0f, 0.0f, -1.0f };
	glm::vec3 m_previousCameraPosition{ 0.0f };
	glm::vec3 m_previousCameraFacing{ 0.0f, 0.0f, -1.0f };
	glm::mat4 m_lastProjection{ 1.0f };
	bool m_hasCameraState = false;
	bool m_cameraMoving = false;
	bool m_wasCameraMoving = false;
};
