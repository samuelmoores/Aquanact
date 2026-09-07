#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <cstddef>

enum class CameraPathInterpolation
{
	Smooth = 0,
	Linear = 1,
	Hold = 2
};

// One ordered position on the camera dolly path.
struct CameraPathPoint {
	glm::vec3 position{0.0f};
	// When enabled, runtime aims this point at the active player. When disabled,
	// the authored facing vector below controls the camera rotation.
	bool lookAtPlayer = true;
	glm::vec3 facing{0.0f, 0.0f, 1.0f};
	// Negative means legacy/evenly-spaced timing. Cutscene points use authored
	// seconds when this is non-negative.
	float timeSeconds = -1.0f;
	CameraPathInterpolation interpolation = CameraPathInterpolation::Smooth;
	bool island = false;
	float triggerRadius = 90.0f;
	glm::vec3 triggerPosition{0.0f};
};

// Points are stored in travel order.  The path is open: the final point does
// not connect back to the first point.
struct CameraPathData {
	std::vector<CameraPathPoint> points;
};

bool IsValidCameraPath(const CameraPathData& path);

struct CameraPathProjection {
	glm::vec3 position{0.0f};
	float normalizedProgress = 0.0f;
	float distanceSquared = 0.0f;
	bool valid = false;
};

// Evaluates a smooth Catmull-Rom segment between points[index] and
// points[index + 1]. Neighboring endpoints are clamped at the path ends.
glm::vec3 EvaluateCameraPathSegment(
	const CameraPathData& path,
	std::size_t index,
	float t);

// Converts authored cutscene time into the normalized path progress consumed by
// PathedCamera. Legacy paths without point times remain evenly distributed.
float CameraPathProgressAtTime(const CameraPathData& path, float timeSeconds, float durationSeconds);

CameraPathProjection ProjectOntoCameraPath(
	const CameraPathData& path,
	const glm::vec3& worldPosition,
	int samplesPerSegment = 32);
