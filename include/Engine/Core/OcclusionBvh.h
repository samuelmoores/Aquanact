#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

struct OcclusionItemKey {
	unsigned int entityId = 0;
	int subMeshIndex = -1;

	bool operator==(const OcclusionItemKey&) const = default;
};

struct OcclusionItem {
	OcclusionItemKey key;
	glm::vec3 worldBoundsMin{ 0.0f };
	glm::vec3 worldBoundsMax{ 0.0f };
	std::uint64_t estimatedTriangles = 0;
	bool occlusionEligible = false;
	bool occluderEligible = false;
};

struct OcclusionBvhNode {
	static constexpr std::uint32_t InvalidIndex = (std::numeric_limits<std::uint32_t>::max)();

	glm::vec3 boundsMin{ 0.0f };
	glm::vec3 boundsMax{ 0.0f };
	std::uint32_t left = InvalidIndex;
	std::uint32_t right = InvalidIndex;
	std::uint32_t firstItem = 0;
	std::uint32_t itemCount = 0;
	std::uint32_t depth = 0;
	std::uint64_t estimatedTriangles = 0;

	bool IsLeaf() const { return left == InvalidIndex && right == InvalidIndex; }
};

class OcclusionBvh {
public:
	static constexpr std::uint32_t InvalidIndex = OcclusionBvhNode::InvalidIndex;

	void Build(std::vector<OcclusionItem> items, std::size_t targetLeafSize = 12);
	void Clear();

	bool Empty() const { return m_nodes.empty(); }
	std::uint32_t RootIndex() const { return Empty() ? InvalidIndex : 0; }
	const std::vector<OcclusionBvhNode>& Nodes() const { return m_nodes; }
	const std::vector<OcclusionItem>& Items() const { return m_items; }
	std::vector<std::uint32_t> SelectQueryNodes(
		std::uint32_t targetDepth = 4, std::size_t maximumNodes = 64) const;

	static bool ValidBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds);

private:
	std::uint32_t BuildRange(std::size_t first, std::size_t count, std::uint32_t depth);

	std::vector<OcclusionBvhNode> m_nodes;
	std::vector<OcclusionItem> m_items;
	std::size_t m_targetLeafSize = 12;
};
