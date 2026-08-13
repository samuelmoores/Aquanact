#pragma once

#include <glm/glm.hpp>

#include <vector>

// One ordered position on the camera dolly path.  playerProgress identifies
// where along the player's route this camera position is reached.
struct CameraPathPoint {
	glm::vec3 position{0.0f};
	float playerProgress = 0.0f;
};

// Points are stored in travel order.  The path is open: the final point does
// not connect back to the first point.
struct CameraPathData {
	std::vector<CameraPathPoint> points;
};
