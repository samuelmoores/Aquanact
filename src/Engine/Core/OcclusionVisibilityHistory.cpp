#include "Engine/Core/OcclusionVisibilityHistory.h"

#include <algorithm>

std::uint64_t OcclusionVisibilityHistory::ItemToken(
	unsigned int entityId, int subMeshIndex)
{
	return (static_cast<std::uint64_t>(entityId) << 32u) |
		static_cast<std::uint32_t>(subMeshIndex + 1);
}

void OcclusionVisibilityHistory::Reset(std::uint64_t generation, std::size_t nodeCount)
{
	m_generation = generation;
	m_nodes.assign(nodeCount, {});
	m_occludedItems.clear();
	m_stats.occludedNodes = 0;
	m_stats.occludedItems = 0;
	// Keep staleResults cumulative so diagnostics reveal late GPU responses.
}

void OcclusionVisibilityHistory::ApplyResult(const OcclusionBvh& bvh,
	std::uint32_t nodeIndex, std::uint64_t generation, bool visible)
{
	if (generation != m_generation || nodeIndex >= m_nodes.size() ||
		nodeIndex >= bvh.Nodes().size())
	{
		++m_stats.staleResults;
		return;
	}

	NodeState& state = m_nodes[nodeIndex];
	if (visible)
	{
		state.consecutiveOccluded = 0;
		state.occluded = false;
	}
	else
	{
		state.consecutiveOccluded = (std::min)(
			static_cast<unsigned int>(state.consecutiveOccluded) + 1u, 2u);
		state.occluded = state.consecutiveOccluded >= 2;
	}
	RebuildOccludedItems(bvh);
}

void OcclusionVisibilityHistory::RebuildOccludedItems(const OcclusionBvh& bvh)
{
	m_occludedItems.clear();
	m_stats.occludedNodes = 0;
	const auto& nodes = bvh.Nodes();
	const auto& items = bvh.Items();
	for (std::size_t nodeIndex = 0; nodeIndex < m_nodes.size() && nodeIndex < nodes.size(); ++nodeIndex)
	{
		if (!m_nodes[nodeIndex].occluded) continue;
		++m_stats.occludedNodes;
		const OcclusionBvhNode& node = nodes[nodeIndex];
		for (std::size_t itemIndex = node.firstItem;
			itemIndex < static_cast<std::size_t>(node.firstItem) + node.itemCount &&
			itemIndex < items.size(); ++itemIndex)
		{
			m_occludedItems.insert(ItemToken(
				items[itemIndex].key.entityId, items[itemIndex].key.subMeshIndex));
		}
	}
	m_stats.occludedItems = m_occludedItems.size();
}

bool OcclusionVisibilityHistory::IsOccluded(
	unsigned int entityId, int subMeshIndex) const
{
	return m_occludedItems.contains(ItemToken(entityId, subMeshIndex)) ||
		m_occludedItems.contains(ItemToken(entityId, -1));
}
