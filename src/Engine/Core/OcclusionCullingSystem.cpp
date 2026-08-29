#include "Engine/Core/OcclusionCullingSystem.h"

#include "Engine/Core/Mesh.h"
#include "Engine/Core/RenderCommand.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr std::uint32_t StableFramesRequired = 2;

	bool MatrixNearlyEqual(const glm::mat4& left, const glm::mat4& right)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (std::abs(left[column][row] - right[column][row]) > 1e-5f)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool ItemKeyLess(const OcclusionItem& left, const OcclusionItem& right)
	{
		if (left.key.entityId != right.key.entityId)
			return left.key.entityId < right.key.entityId;
		return left.key.subMeshIndex < right.key.subMeshIndex;
	}

	bool BoundsNearlyEqual(const glm::vec3& left, const glm::vec3& right)
	{
		return glm::all(glm::lessThanEqual(glm::abs(left - right), glm::vec3(1e-5f)));
	}
}

namespace
{
	bool TransformBounds(const glm::vec3& localMin, const glm::vec3& localMax,
		const glm::mat4& modelMatrix, glm::vec3& worldMin, glm::vec3& worldMax)
	{
		if (!OcclusionBvh::ValidBounds(localMin, localMax)) return false;

		const std::array<glm::vec3, 8> corners = {
			glm::vec3(localMin.x, localMin.y, localMin.z),
			glm::vec3(localMax.x, localMin.y, localMin.z),
			glm::vec3(localMin.x, localMax.y, localMin.z),
			glm::vec3(localMax.x, localMax.y, localMin.z),
			glm::vec3(localMin.x, localMin.y, localMax.z),
			glm::vec3(localMax.x, localMin.y, localMax.z),
			glm::vec3(localMin.x, localMax.y, localMax.z),
			glm::vec3(localMax.x, localMax.y, localMax.z)
		};

		bool initialized = false;
		for (const glm::vec3& corner : corners)
		{
			const glm::vec4 transformed = modelMatrix * glm::vec4(corner, 1.0f);
			if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) ||
				!std::isfinite(transformed.z) || !std::isfinite(transformed.w) ||
				std::abs(transformed.w) < 1e-8f)
			{
				return false;
			}
			const glm::vec3 point = glm::vec3(transformed) / transformed.w;
			if (!initialized)
			{
				worldMin = point;
				worldMax = point;
				initialized = true;
			}
			else
			{
				worldMin = glm::min(worldMin, point);
				worldMax = glm::max(worldMax, point);
			}
		}
		return initialized && OcclusionBvh::ValidBounds(worldMin, worldMax);
	}

	OcclusionItem WholeEntityItem(const RenderCommand& command)
	{
		OcclusionItem item;
		item.key = { command.entityId, -1 };
		item.worldBoundsMin = command.worldBoundsMin;
		item.worldBoundsMax = command.worldBoundsMax;
		for (int index = 0; index < command.mesh->NumBuffers(); ++index)
		{
			item.estimatedTriangles += static_cast<std::uint64_t>(command.mesh->FacesSize(index)) / 3u;
		}
		item.occlusionEligible = !command.isSkinned;
		// Opacity and motion are not represented authoritatively yet. A candidate
		// may be tested for occlusion, but it may not write occluder depth.
		item.occluderEligible = false;
		return item;
	}
}

void OcclusionCullingSystem::Clear()
{
	m_bvh.Clear();
	m_transformHistory.clear();
	m_previousStableItems.clear();
	m_previousSignatures.clear();
	m_selectedOccluders.clear();
	m_stats = {};
	m_frame = 0;
	m_queryGeneration = 0;
	m_visibilityHistory.Reset(0, 0);
	m_hasCameraState = false;
	m_cameraMoving = false;
	m_wasCameraMoving = false;
}

void OcclusionCullingSystem::UpdateCamera(const glm::vec3& position,
	const glm::vec3& facing, const glm::mat4& projection)
{
	bool projectionChanged = false;
	for (int column = 0; column < 4 && !projectionChanged; ++column)
		for (int row = 0; row < 4; ++row)
			if (std::abs(projection[column][row] - m_lastProjection[column][row]) > 1e-5f)
			{
				projectionChanged = true;
				break;
			}

	const float translation = glm::length(position - m_previousCameraPosition);
	const glm::vec3 normalizedFacing = glm::length(facing) > 1e-5f
		? glm::normalize(facing) : m_previousCameraFacing;
	const float facingDot = glm::dot(normalizedFacing, m_previousCameraFacing);
	// Ordinary movement must not invalidate the asynchronous query generation:
	// doing so made every result stale. Instead, suppress application of historical
	// results while the camera is moving, then resume once it settles. Only a
	// projection change (resize/FOV) or the first camera frame resets history.
	m_cameraMoving = m_hasCameraState &&
		(translation > 0.01f || facingDot < 0.9999f);
	const bool discontinuity = !m_hasCameraState || projectionChanged;
	if (discontinuity)
	{
		++m_stats.cameraInvalidationCount;
		++m_queryGeneration;
		m_visibilityHistory.Reset(m_queryGeneration, m_bvh.Nodes().size());
		// Keep a generation baseline rather than comparing only adjacent frames;
		// many small movements must eventually invalidate old query results too.
		m_lastCameraPosition = position;
		m_lastCameraFacing = normalizedFacing;
		m_lastProjection = projection;
	}
	if (m_wasCameraMoving && !m_cameraMoving)
	{
		// Results generated during motion may describe a different view. Start a
		// fresh stationary generation before allowing suppression again.
		++m_queryGeneration;
		m_visibilityHistory.Reset(m_queryGeneration, m_bvh.Nodes().size());
	}
	m_previousCameraPosition = position;
	m_previousCameraFacing = normalizedFacing;
	m_wasCameraMoving = m_cameraMoving;
	m_hasCameraState = true;
}

void OcclusionCullingSystem::ApplyQueryResult(std::uint32_t nodeIndex,
	std::uint64_t generation, bool visible)
{
	m_visibilityHistory.ApplyResult(m_bvh, nodeIndex, generation, visible);
}

bool OcclusionCullingSystem::IsOccluded(unsigned int entityId, int subMeshIndex) const
{
	return !m_cameraMoving && m_visibilityHistory.IsOccluded(entityId, subMeshIndex);
}

void OcclusionCullingSystem::SelectOccluders(const glm::mat4& viewProjection,
	int viewportWidth, int viewportHeight, const OccluderSelectionConfig& config)
{
	m_selectedOccluders = OccluderSelector::Select(
		m_bvh.Items(), viewProjection, viewportWidth, viewportHeight, config);
	m_stats.selectedOccluders = m_selectedOccluders.size();
	m_stats.selectedOccluderTriangles = 0;
	for (const SelectedOccluder& occluder : m_selectedOccluders)
	{
		m_stats.selectedOccluderTriangles += occluder.estimatedTriangles;
	}
}

bool OcclusionCullingSystem::ItemsChanged(const std::vector<OcclusionItem>& items) const
{
	if (items.size() != m_previousStableItems.size()) return true;
	for (std::size_t index = 0; index < items.size(); ++index)
	{
		const OcclusionItem& left = items[index];
		const OcclusionItem& right = m_previousStableItems[index];
		if (!(left.key == right.key) ||
			!BoundsNearlyEqual(left.worldBoundsMin, right.worldBoundsMin) ||
			!BoundsNearlyEqual(left.worldBoundsMax, right.worldBoundsMax) ||
			left.estimatedTriangles != right.estimatedTriangles)
		{
			return true;
		}
	}
	return false;
}

void OcclusionCullingSystem::Update(const RenderCommand* commands,
	std::size_t commandCount, std::size_t targetLeafSize)
{
	const auto updateStart = std::chrono::high_resolution_clock::now();
	++m_frame;
	std::vector<CommandSignature> signatures;
	signatures.reserve(commandCount);
	for (std::size_t index = 0; commands && index < commandCount; ++index)
	{
		const RenderCommand& command = commands[index];
		CommandSignature signature;
		signature.entityId = command.entityId;
		signature.meshAddress = reinterpret_cast<std::uintptr_t>(command.mesh);
		signature.bufferCount = command.mesh ? command.mesh->NumBuffers() : 0;
		signature.modelMatrix = command.modelMatrix;
		signature.worldBoundsMin = command.worldBoundsMin;
		signature.worldBoundsMax = command.worldBoundsMax;
		signature.hasWorldBounds = command.hasWorldBounds;
		signature.isSkinned = command.isSkinned;
		signatures.push_back(signature);
	}
	// Render-command assembly is not required to preserve entity order. Keep the
	// cache order stable so unrelated ordering changes do not trigger a full
	// bounds extraction and BVH rebuild.
	std::sort(signatures.begin(), signatures.end(),
		[](const CommandSignature& left, const CommandSignature& right)
		{
			if (left.entityId != right.entityId) return left.entityId < right.entityId;
			if (left.meshAddress != right.meshAddress) return left.meshAddress < right.meshAddress;
			return left.bufferCount < right.bufferCount;
		});
	bool signaturesChanged = signatures.size() != m_previousSignatures.size();
	if (!signaturesChanged)
	{
		for (std::size_t index = 0; index < signatures.size(); ++index)
		{
			const CommandSignature& left = signatures[index];
			const CommandSignature& right = m_previousSignatures[index];
			const bool commonChanged = left.entityId != right.entityId ||
				left.meshAddress != right.meshAddress || left.bufferCount != right.bufferCount ||
				left.hasWorldBounds != right.hasWorldBounds || left.isSkinned != right.isSkinned;
			// Skinned commands are excluded from the BVH, so animation-driven model
			// matrices and bounds must not invalidate static occlusion geometry.
			const bool transformChanged = !left.isSkinned &&
				(!MatrixNearlyEqual(left.modelMatrix, right.modelMatrix) ||
				 !BoundsNearlyEqual(left.worldBoundsMin, right.worldBoundsMin) ||
				 !BoundsNearlyEqual(left.worldBoundsMax, right.worldBoundsMax));
			if (commonChanged || transformChanged)
			{
				signaturesChanged = true;
				break;
			}
		}
	}

	bool stabilityTransitioned = false;
	for (std::size_t index = 0; commands && index < commandCount; ++index)
	{
		const RenderCommand& command = commands[index];
		TransformHistory& history = m_transformHistory[command.entityId];
		const std::uint32_t previousStableFrames = history.stableFrames;
		if (history.lastSeenFrame != 0 && MatrixNearlyEqual(history.modelMatrix, command.modelMatrix))
		{
			history.stableFrames = (std::min)(history.stableFrames + 1, StableFramesRequired);
		}
		else
		{
			history.modelMatrix = command.modelMatrix;
			history.stableFrames = 0;
		}
		if (previousStableFrames < StableFramesRequired &&
			history.stableFrames >= StableFramesRequired)
		{
			stabilityTransitioned = true;
		}
		history.lastSeenFrame = m_frame;
	}
	m_previousSignatures = std::move(signatures);
	if (!signaturesChanged && !stabilityTransitioned)
	{
		m_stats.updateMs = std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - updateStart).count();
		return;
	}

	std::vector<OcclusionItem> candidates = BuildItems(commands, commandCount);
	m_stats.candidateItems = candidates.size();
	std::vector<OcclusionItem> stableItems;
	stableItems.reserve(candidates.size());
	for (OcclusionItem& item : candidates)
	{
		const auto history = m_transformHistory.find(item.key.entityId);
		if (history == m_transformHistory.end() ||
			history->second.stableFrames < StableFramesRequired)
		{
			continue;
		}
		item.occlusionEligible = true;
		// The current main-scene shader always writes alpha 1.0 and has no cutout
		// path. This must be revisited when transparent scene materials are added.
		item.occluderEligible = true;
		stableItems.push_back(item);
	}
	std::sort(stableItems.begin(), stableItems.end(), ItemKeyLess);
	m_stats.stableItems = stableItems.size();

	const bool rebuild = ItemsChanged(stableItems);
	if (rebuild)
	{
		const auto rebuildStart = std::chrono::high_resolution_clock::now();
		m_bvh.Build(stableItems, targetLeafSize);
		m_stats.rebuildMs = std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - rebuildStart).count();
		++m_stats.rebuildCount;
		++m_queryGeneration;
		m_visibilityHistory.Reset(m_queryGeneration, m_bvh.Nodes().size());
		m_previousStableItems = std::move(stableItems);
	}
	else
	{
		m_stats.rebuildMs = 0.0;
	}

	for (auto history = m_transformHistory.begin(); history != m_transformHistory.end();)
	{
		if (history->second.lastSeenFrame != m_frame)
			history = m_transformHistory.erase(history);
		else
			++history;
	}

	m_stats.nodes = m_bvh.Nodes().size();
	m_stats.leaves = 0;
	m_stats.maxDepth = 0;
	for (const OcclusionBvhNode& node : m_bvh.Nodes())
	{
		if (node.IsLeaf()) ++m_stats.leaves;
		m_stats.maxDepth = (std::max)(m_stats.maxDepth, static_cast<std::size_t>(node.depth));
	}
	m_stats.updateMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - updateStart).count();
}

std::vector<OcclusionItem> OcclusionCullingSystem::BuildItems(
	const RenderCommand* commands, std::size_t commandCount)
{
	std::vector<OcclusionItem> items;
	if (!commands || commandCount == 0) return items;

	for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
	{
		const RenderCommand& command = commands[commandIndex];
		if (!command.mesh || command.isSkinned) continue;

		const int bufferCount = (std::max)(command.mesh->NumBuffers(), 0);
		bool addedSubMesh = false;
		for (int subMeshIndex = 0; subMeshIndex < bufferCount; ++subMeshIndex)
		{
			glm::vec3 worldMin(0.0f);
			glm::vec3 worldMax(0.0f);
			if (!TransformBounds(command.mesh->SubMeshMinBounds(subMeshIndex),
				command.mesh->SubMeshMaxBounds(subMeshIndex), command.modelMatrix,
				worldMin, worldMax))
			{
				continue;
			}

			OcclusionItem item;
			item.key = { command.entityId, subMeshIndex };
			item.worldBoundsMin = worldMin;
			item.worldBoundsMax = worldMax;
			item.estimatedTriangles = static_cast<std::uint64_t>(
				command.mesh->FacesSize(subMeshIndex)) / 3u;
			item.occlusionEligible = true;
			item.occluderEligible = false;
			items.push_back(item);
			addedSubMesh = true;
		}

		if (!addedSubMesh && command.hasWorldBounds &&
			OcclusionBvh::ValidBounds(command.worldBoundsMin, command.worldBoundsMax))
		{
			items.push_back(WholeEntityItem(command));
		}
	}
	return items;
}
