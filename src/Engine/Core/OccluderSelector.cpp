#include "Engine/Core/OccluderSelector.h"

#include <algorithm>
#include <array>
#include <cmath>

std::vector<SelectedOccluder> OccluderSelector::Select(
	const std::vector<OcclusionItem>& items,
	const glm::mat4& viewProjection,
	int viewportWidth,
	int viewportHeight,
	const OccluderSelectionConfig& config)
{
	std::vector<SelectedOccluder> candidates;
	if (viewportWidth <= 0 || viewportHeight <= 0 || config.maximumItems == 0)
		return candidates;

	for (const OcclusionItem& item : items)
	{
		if (!item.occluderEligible ||
			!OcclusionBvh::ValidBounds(item.worldBoundsMin, item.worldBoundsMax))
			continue;

		const std::array<glm::vec3, 8> corners = {
			glm::vec3(item.worldBoundsMin.x, item.worldBoundsMin.y, item.worldBoundsMin.z),
			glm::vec3(item.worldBoundsMax.x, item.worldBoundsMin.y, item.worldBoundsMin.z),
			glm::vec3(item.worldBoundsMin.x, item.worldBoundsMax.y, item.worldBoundsMin.z),
			glm::vec3(item.worldBoundsMax.x, item.worldBoundsMax.y, item.worldBoundsMin.z),
			glm::vec3(item.worldBoundsMin.x, item.worldBoundsMin.y, item.worldBoundsMax.z),
			glm::vec3(item.worldBoundsMax.x, item.worldBoundsMin.y, item.worldBoundsMax.z),
			glm::vec3(item.worldBoundsMin.x, item.worldBoundsMax.y, item.worldBoundsMax.z),
			glm::vec3(item.worldBoundsMax.x, item.worldBoundsMax.y, item.worldBoundsMax.z)
		};

		glm::vec2 ndcMin(1.0f);
		glm::vec2 ndcMax(-1.0f);
		bool valid = true;
		for (const glm::vec3& corner : corners)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(corner, 1.0f);
			// Bounds crossing or behind the eye are skipped. Selecting fewer
			// occluders costs performance only; it cannot hide visible geometry.
			if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
				!std::isfinite(clip.w) || clip.w <= 1e-5f)
			{
				valid = false;
				break;
			}
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			ndcMin = glm::min(ndcMin, ndc);
			ndcMax = glm::max(ndcMax, ndc);
		}
		if (!valid) continue;

		ndcMin = glm::clamp(ndcMin, glm::vec2(-1.0f), glm::vec2(1.0f));
		ndcMax = glm::clamp(ndcMax, glm::vec2(-1.0f), glm::vec2(1.0f));
		const glm::vec2 ndcExtent = glm::max(ndcMax - ndcMin, glm::vec2(0.0f));
		const float projectedPixels = ndcExtent.x * 0.5f * static_cast<float>(viewportWidth) *
			ndcExtent.y * 0.5f * static_cast<float>(viewportHeight);
		if (projectedPixels < config.minimumProjectedPixels) continue;

		candidates.push_back({ item.key, projectedPixels, item.estimatedTriangles });
	}

	std::sort(candidates.begin(), candidates.end(), [](const SelectedOccluder& left,
		const SelectedOccluder& right)
		{
			if (left.projectedPixels != right.projectedPixels)
				return left.projectedPixels > right.projectedPixels;
			if (left.key.entityId != right.key.entityId)
				return left.key.entityId < right.key.entityId;
			return left.key.subMeshIndex < right.key.subMeshIndex;
		});

	std::vector<SelectedOccluder> selected;
	selected.reserve((std::min)(config.maximumItems, candidates.size()));
	std::uint64_t triangles = 0;
	for (const SelectedOccluder& candidate : candidates)
	{
		if (selected.size() >= config.maximumItems) break;
		if (candidate.estimatedTriangles > config.maximumTriangles -
			(std::min)(triangles, config.maximumTriangles))
			continue;
		selected.push_back(candidate);
		triangles += candidate.estimatedTriangles;
	}
	return selected;
}
