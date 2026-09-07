#include "Engine/Core/CameraPathData.h"

#include <algorithm>
#include <limits>
#include <cmath>

bool IsValidCameraPath(const CameraPathData& path)
{
	for (std::size_t i = 0; i < path.points.size(); ++i)
	{
		const glm::vec3& position = path.points[i].position;
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
		{
			return false;
		}
		if (i > 0 && glm::dot(position - path.points[i - 1].position, position - path.points[i - 1].position) <= 1e-8f)
		{
			return false;
		}
	}
	return true;
}

glm::vec3 EvaluateCameraPathSegment(
	const CameraPathData& path,
	std::size_t index,
	float t)
{
	// A path with no points has no meaningful position; use the origin as a
	// safe fallback for callers that evaluate an empty editor path.
	if (path.points.empty())
	{
		return glm::vec3(0.0f);
	}
	if (path.points.size() == 1)
	{
		return path.points.front().position;
	}

	// Clamp the segment index so malformed or stale editor selections cannot
	// access beyond the final pair of control points.
	const std::size_t segment = std::min(index, path.points.size() - 2);

	// Reuse the nearest endpoint for missing neighbors. This gives the first
	// and final segments stable tangents without requiring phantom points.
	const std::size_t p0Index = segment > 0 ? segment - 1 : segment;
	const std::size_t p1Index = segment;
	const std::size_t p2Index = segment + 1;
	const std::size_t p3Index = std::min(segment + 2, path.points.size() - 1);

	const glm::vec3& p0 = path.points[p0Index].position;
	const glm::vec3& p1 = path.points[p1Index].position;
	const glm::vec3& p2 = path.points[p2Index].position;
	const glm::vec3& p3 = path.points[p3Index].position;

	// Keep interpolation local to this segment even if the caller supplies a
	// slightly out-of-range parameter.
	const float u = glm::clamp(t, 0.0f, 1.0f);
	if (path.points[segment].interpolation == CameraPathInterpolation::Hold)
		return p1;
	if (path.points[segment].interpolation == CameraPathInterpolation::Linear)
		return glm::mix(p1, p2, u);
	const float u2 = u * u;
	const float u3 = u2 * u;

	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * u +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u3);
}

float CameraPathProgressAtTime(const CameraPathData& path, float timeSeconds, float durationSeconds)
{
	if (path.points.size() <= 1)
		return 0.0f;
	const float safeDuration = std::max(durationSeconds, 0.001f);
	const bool authored = path.points.back().timeSeconds >= 0.0f;
	if (!authored)
		return glm::clamp(timeSeconds / safeDuration, 0.0f, 1.0f);

	const float time = std::max(0.0f, timeSeconds);
	for (std::size_t index = 0; index + 1 < path.points.size(); ++index)
	{
		const float start = std::max(0.0f, path.points[index].timeSeconds);
		const float end = std::max(start + 0.001f, path.points[index + 1].timeSeconds);
		if (time <= end)
		{
			const float local = glm::clamp((time - start) / (end - start), 0.0f, 1.0f);
			return (static_cast<float>(index) + local) / static_cast<float>(path.points.size() - 1);
		}
	}
	return 1.0f;
}

CameraPathProjection ProjectOntoCameraPath(
	const CameraPathData& path,
	const glm::vec3& worldPosition,
	int samplesPerSegment)
{
	// The spline has no simple cheap closest-point solution, so approximate it
	// with short line segments and project the player onto each one.
	CameraPathProjection result;
	if (path.points.empty()) return result;
	if (path.points.size() == 1)
	{
		result.position = path.points.front().position;
		result.distanceSquared = glm::dot(worldPosition - result.position, worldPosition - result.position);
		result.valid = true;
		return result;
	}

	// Always use at least one sample per curve segment to keep the projection
	// valid even when a caller requests an invalid resolution.
	const int samples = std::max(samplesPerSegment, 1);
	float bestDistanceSquared = std::numeric_limits<float>::max();
	const std::size_t segmentCount = path.points.size() - 1;
	for (std::size_t segment = 0; segment < segmentCount; ++segment)
	{
		for (int sample = 0; sample < samples; ++sample)
		{
			// Each sampled edge is treated as a finite line segment. The clamped
			// edge parameter prevents the projection from leaking into neighbors.
			const float t0 = static_cast<float>(sample) / static_cast<float>(samples);
			const float t1 = static_cast<float>(sample + 1) / static_cast<float>(samples);
			const glm::vec3 a = EvaluateCameraPathSegment(path, segment, t0);
			const glm::vec3 b = EvaluateCameraPathSegment(path, segment, t1);
			const glm::vec3 edge = b - a;
			const float edgeLengthSquared = glm::dot(edge, edge);
			const float edgeT = edgeLengthSquared > 1e-8f
				? glm::clamp(glm::dot(worldPosition - a, edge) / edgeLengthSquared, 0.0f, 1.0f)
				: 0.0f;
			const glm::vec3 closest = a + edge * edgeT;
			const glm::vec3 delta = worldPosition - closest;
			const float distanceSquared = glm::dot(delta, delta);

			if (distanceSquared < bestDistanceSquared)
			{
				bestDistanceSquared = distanceSquared;
				result.position = closest;

				// Convert the winning sample location into normalized progress over
				// the ordered path, where 0 is the first point and 1 is the last.
				result.normalizedProgress = (static_cast<float>(segment) + (t0 + (t1 - t0) * edgeT)) / static_cast<float>(segmentCount);
				result.distanceSquared = distanceSquared;
				result.valid = true;
			}
		}
	}
	return result;
}
