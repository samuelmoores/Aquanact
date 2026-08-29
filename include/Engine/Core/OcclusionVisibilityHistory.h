#pragma once

#include "Engine/Core/OcclusionBvh.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

class OcclusionVisibilityHistory {
public:
	struct Stats {
		std::size_t occludedNodes = 0;
		std::size_t occludedItems = 0;
		std::size_t staleResults = 0;
	};

	void Reset(std::uint64_t generation, std::size_t nodeCount);
	void ApplyResult(const OcclusionBvh& bvh, std::uint32_t nodeIndex,
		std::uint64_t generation, bool visible);
	bool IsOccluded(unsigned int entityId, int subMeshIndex) const;
	std::uint64_t Generation() const { return m_generation; }
	const Stats& CurrentStats() const { return m_stats; }

private:
	struct NodeState {
		std::uint8_t consecutiveOccluded = 0;
		bool occluded = false;
	};

	static std::uint64_t ItemToken(unsigned int entityId, int subMeshIndex);
	void RebuildOccludedItems(const OcclusionBvh& bvh);

	std::uint64_t m_generation = 0;
	std::vector<NodeState> m_nodes;
	std::unordered_set<std::uint64_t> m_occludedItems;
	Stats m_stats;
};
