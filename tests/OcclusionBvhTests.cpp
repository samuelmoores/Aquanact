#include "Engine/Core/OcclusionBvh.h"
#include "Engine/Core/OccluderSelector.h"
#include "Engine/Core/OcclusionVisibilityHistory.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
	OcclusionItem Item(unsigned int entityId, int subMeshIndex,
		const glm::vec3& minBounds, const glm::vec3& maxBounds,
		std::uint64_t triangles = 1)
	{
		OcclusionItem item;
		item.key = { entityId, subMeshIndex };
		item.worldBoundsMin = minBounds;
		item.worldBoundsMax = maxBounds;
		item.estimatedTriangles = triangles;
		item.occlusionEligible = true;
		return item;
	}

	bool Contains(const OcclusionBvhNode& node, const OcclusionItem& item)
	{
		return node.boundsMin.x <= item.worldBoundsMin.x &&
			node.boundsMin.y <= item.worldBoundsMin.y &&
			node.boundsMin.z <= item.worldBoundsMin.z &&
			node.boundsMax.x >= item.worldBoundsMax.x &&
			node.boundsMax.y >= item.worldBoundsMax.y &&
			node.boundsMax.z >= item.worldBoundsMax.z;
	}

	void ValidateNode(const OcclusionBvh& bvh, std::uint32_t nodeIndex)
	{
		const auto& nodes = bvh.Nodes();
		const auto& items = bvh.Items();
		const OcclusionBvhNode& node = nodes[nodeIndex];
		assert(node.itemCount > 0);
		assert(static_cast<std::size_t>(node.firstItem) + node.itemCount <= items.size());

		std::uint64_t triangleCount = 0;
		for (std::size_t index = node.firstItem;
			index < static_cast<std::size_t>(node.firstItem) + node.itemCount; ++index)
		{
			assert(Contains(node, items[index]));
			triangleCount += items[index].estimatedTriangles;
		}
		assert(triangleCount == node.estimatedTriangles);

		if (node.IsLeaf()) return;
		assert(node.left != OcclusionBvh::InvalidIndex);
		assert(node.right != OcclusionBvh::InvalidIndex);
		assert(nodes[node.left].depth == node.depth + 1);
		assert(nodes[node.right].depth == node.depth + 1);
		assert(nodes[node.left].firstItem == node.firstItem);
		assert(nodes[node.left].itemCount + nodes[node.right].itemCount == node.itemCount);
		assert(nodes[node.right].firstItem == node.firstItem + nodes[node.left].itemCount);
		ValidateNode(bvh, node.left);
		ValidateNode(bvh, node.right);
	}
}

int main()
{
	OcclusionBvh empty;
	empty.Build({});
	assert(empty.Empty());
	assert(empty.RootIndex() == OcclusionBvh::InvalidIndex);

	OcclusionBvh one;
	one.Build({ Item(7, 0, { -1.0f, -2.0f, -3.0f }, { 1.0f, 2.0f, 3.0f }, 12) });
	assert(one.Nodes().size() == 1);
	assert(one.Nodes()[0].IsLeaf());
	assert(one.Nodes()[0].estimatedTriangles == 12);

	std::vector<OcclusionItem> items;
	for (unsigned int index = 0; index < 31; ++index)
	{
		const float x = static_cast<float>((index * 7) % 13);
		const float y = static_cast<float>((index * 3) % 5);
		items.push_back(Item(100 - index, static_cast<int>(index),
			{ x, y, -1.0f }, { x + 0.5f, y + 1.0f, 1.0f }, index + 1));
	}

	OcclusionBvh tree;
	tree.Build(items, 4);
	assert(!tree.Empty());
	assert(tree.Items().size() == items.size());
	ValidateNode(tree, tree.RootIndex());
	const auto queryNodes = tree.SelectQueryNodes(2, 3);
	assert(queryNodes.size() == 3);
	for (const std::uint32_t nodeIndex : queryNodes)
	{
		assert(nodeIndex < tree.Nodes().size());
		assert(tree.Nodes()[nodeIndex].IsLeaf() || tree.Nodes()[nodeIndex].depth == 2);
	}

	// Identical centers still split deterministically by stable item key.
	std::vector<OcclusionItem> identical;
	for (unsigned int index = 0; index < 9; ++index)
	{
		identical.push_back(Item(20 - index, 0, glm::vec3(-1.0f), glm::vec3(1.0f)));
	}
	OcclusionBvh identicalA;
	OcclusionBvh identicalB;
	identicalA.Build(identical, 2);
	identicalB.Build(identical, 2);
	assert(identicalA.Items().size() == identicalB.Items().size());
	for (std::size_t index = 0; index < identicalA.Items().size(); ++index)
	{
		assert(identicalA.Items()[index].key == identicalB.Items()[index].key);
	}
	ValidateNode(identicalA, identicalA.RootIndex());

	// Invalid and reversed bounds are discarded instead of poisoning the tree.
	const float nan = (std::numeric_limits<float>::quiet_NaN)();
	OcclusionBvh filtered;
	filtered.Build({
		Item(1, 0, glm::vec3(2.0f), glm::vec3(1.0f)),
		Item(2, 0, glm::vec3(nan, 0.0f, 0.0f), glm::vec3(1.0f)),
		Item(3, 0, glm::vec3(0.0f), glm::vec3(1.0f))
	});
	assert(filtered.Items().size() == 1);
	assert(filtered.Items()[0].key.entityId == 3);

	OcclusionItem large = Item(10, 0, glm::vec3(-0.5f, -0.5f, 0.0f),
		glm::vec3(0.5f, 0.5f, 0.1f), 50);
	large.occluderEligible = true;
	OcclusionItem small = Item(11, 0, glm::vec3(-0.05f, -0.05f, 0.0f),
		glm::vec3(0.05f, 0.05f, 0.1f), 2);
	small.occluderEligible = true;
	OcclusionItem ineligible = large;
	ineligible.key.entityId = 12;
	ineligible.occluderEligible = false;

	OccluderSelectionConfig selectionConfig;
	selectionConfig.minimumProjectedPixels = 100.0f;
	selectionConfig.maximumItems = 4;
	selectionConfig.maximumTriangles = 100;
	const auto selected = OccluderSelector::Select(
		{ small, ineligible, large }, glm::mat4(1.0f), 100, 100, selectionConfig);
	assert(selected.size() == 1);
	assert(selected[0].key.entityId == large.key.entityId);
	assert(std::abs(selected[0].projectedPixels - 2500.0f) < 0.01f);

	selectionConfig.maximumTriangles = 49;
	assert(OccluderSelector::Select(
		{ large }, glm::mat4(1.0f), 100, 100, selectionConfig).empty());
	assert(OccluderSelector::Select(
		{ large }, glm::mat4(1.0f), 0, 100, selectionConfig).empty());

	OcclusionBvh visibilityTree;
	visibilityTree.Build({
		Item(30, 0, glm::vec3(-1.0f), glm::vec3(0.0f)),
		Item(31, 2, glm::vec3(0.0f), glm::vec3(1.0f))
	}, 8);
	OcclusionVisibilityHistory visibility;
	visibility.Reset(5, visibilityTree.Nodes().size());
	visibility.ApplyResult(visibilityTree, visibilityTree.RootIndex(), 5, false);
	assert(!visibility.IsOccluded(30, 0));
	visibility.ApplyResult(visibilityTree, visibilityTree.RootIndex(), 5, false);
	assert(visibility.IsOccluded(30, 0));
	assert(visibility.IsOccluded(31, 2));
	assert(visibility.CurrentStats().occludedNodes == 1);
	visibility.ApplyResult(visibilityTree, visibilityTree.RootIndex(), 5, true);
	assert(!visibility.IsOccluded(30, 0));
	visibility.ApplyResult(visibilityTree, visibilityTree.RootIndex(), 4, false);
	assert(visibility.CurrentStats().staleResults == 1);

	return 0;
}
