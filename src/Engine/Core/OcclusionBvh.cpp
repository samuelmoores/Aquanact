#include "Engine/Core/OcclusionBvh.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool KeyLess(const OcclusionItem& left, const OcclusionItem& right)
	{
		if (left.key.entityId != right.key.entityId)
		{
			return left.key.entityId < right.key.entityId;
		}
		return left.key.subMeshIndex < right.key.subMeshIndex;
	}

	glm::vec3 Center(const OcclusionItem& item)
	{
		return (item.worldBoundsMin + item.worldBoundsMax) * 0.5f;
	}
}

bool OcclusionBvh::ValidBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds)
{
	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(minBounds[axis]) || !std::isfinite(maxBounds[axis]) ||
			minBounds[axis] > maxBounds[axis])
		{
			return false;
		}
	}
	return true;
}

void OcclusionBvh::Clear()
{
	m_nodes.clear();
	m_items.clear();
}

void OcclusionBvh::Build(std::vector<OcclusionItem> items, std::size_t targetLeafSize)
{
	Clear();
	m_targetLeafSize = (std::max)(targetLeafSize, std::size_t{ 1 });

	items.erase(std::remove_if(items.begin(), items.end(), [](const OcclusionItem& item)
		{
			return !ValidBounds(item.worldBoundsMin, item.worldBoundsMax);
		}), items.end());
	std::sort(items.begin(), items.end(), KeyLess);
	m_items = std::move(items);

	if (m_items.empty())
	{
		return;
	}

	// A binary tree holding N items uses fewer than 2N nodes. Reserving up front
	// also keeps node indices stable while recursive construction appends children.
	m_nodes.reserve(m_items.size() * 2);
	BuildRange(0, m_items.size(), 0);
}

std::vector<std::uint32_t> OcclusionBvh::SelectQueryNodes(
	std::uint32_t targetDepth, std::size_t maximumNodes) const
{
	std::vector<std::uint32_t> selected;
	if (m_nodes.empty() || maximumNodes == 0) return selected;

	std::vector<std::uint32_t> pending{ RootIndex() };
	for (std::size_t pendingIndex = 0; pendingIndex < pending.size(); ++pendingIndex)
	{
		const std::uint32_t nodeIndex = pending[pendingIndex];
		const OcclusionBvhNode& node = m_nodes[nodeIndex];
		if (node.IsLeaf() || node.depth >= targetDepth)
		{
			selected.push_back(nodeIndex);
			continue;
		}
		pending.push_back(node.left);
		pending.push_back(node.right);
	}

	std::sort(selected.begin(), selected.end(), [this](std::uint32_t left, std::uint32_t right)
		{
			if (m_nodes[left].estimatedTriangles != m_nodes[right].estimatedTriangles)
				return m_nodes[left].estimatedTriangles > m_nodes[right].estimatedTriangles;
			return left < right;
		});
	if (selected.size() > maximumNodes) selected.resize(maximumNodes);
	return selected;
}

std::uint32_t OcclusionBvh::BuildRange(std::size_t first, std::size_t count, std::uint32_t depth)
{
	OcclusionBvhNode node;
	node.firstItem = static_cast<std::uint32_t>(first);
	node.itemCount = static_cast<std::uint32_t>(count);
	node.depth = depth;
	node.boundsMin = m_items[first].worldBoundsMin;
	node.boundsMax = m_items[first].worldBoundsMax;

	glm::vec3 centerMin = Center(m_items[first]);
	glm::vec3 centerMax = centerMin;
	for (std::size_t index = first; index < first + count; ++index)
	{
		const OcclusionItem& item = m_items[index];
		node.boundsMin = glm::min(node.boundsMin, item.worldBoundsMin);
		node.boundsMax = glm::max(node.boundsMax, item.worldBoundsMax);
		centerMin = glm::min(centerMin, Center(item));
		centerMax = glm::max(centerMax, Center(item));
		node.estimatedTriangles += item.estimatedTriangles;
	}

	const std::uint32_t nodeIndex = static_cast<std::uint32_t>(m_nodes.size());
	m_nodes.push_back(node);
	if (count <= m_targetLeafSize)
	{
		return nodeIndex;
	}

	const glm::vec3 extent = centerMax - centerMin;
	int splitAxis = 0;
	if (extent.y > extent.x) splitAxis = 1;
	if (extent.z > extent[splitAxis]) splitAxis = 2;

	const std::size_t middle = first + count / 2;
	std::nth_element(m_items.begin() + static_cast<std::ptrdiff_t>(first),
		m_items.begin() + static_cast<std::ptrdiff_t>(middle),
		m_items.begin() + static_cast<std::ptrdiff_t>(first + count),
		[splitAxis](const OcclusionItem& left, const OcclusionItem& right)
		{
			const float leftCenter = Center(left)[splitAxis];
			const float rightCenter = Center(right)[splitAxis];
			if (leftCenter != rightCenter) return leftCenter < rightCenter;
			return KeyLess(left, right);
		});

	const std::size_t leftCount = middle - first;
	const std::size_t rightCount = count - leftCount;
	const std::uint32_t left = BuildRange(first, leftCount, depth + 1);
	const std::uint32_t right = BuildRange(middle, rightCount, depth + 1);
	m_nodes[nodeIndex].left = left;
	m_nodes[nodeIndex].right = right;
	return nodeIndex;
}
