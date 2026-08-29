#pragma once

#include "Engine/Core/OcclusionBvh.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

struct OccluderSelectionConfig {
	float minimumProjectedPixels = 4096.0f;
	std::size_t maximumItems = 64;
	std::uint64_t maximumTriangles = 250000;
};

struct SelectedOccluder {
	OcclusionItemKey key;
	float projectedPixels = 0.0f;
	std::uint64_t estimatedTriangles = 0;
};

class OccluderSelector {
public:
	static std::vector<SelectedOccluder> Select(
		const std::vector<OcclusionItem>& items,
		const glm::mat4& viewProjection,
		int viewportWidth,
		int viewportHeight,
		const OccluderSelectionConfig& config = {});
};
