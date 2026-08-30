#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <cstddef>

// One ordered position on the camera dolly path.
struct CameraPathPoint {
	glm::vec3 position{0.0f};
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

CameraPathProjection ProjectOntoCameraPath(
	const CameraPathData& path,
	const glm::vec3& worldPosition,
	int samplesPerSegment = 32);
