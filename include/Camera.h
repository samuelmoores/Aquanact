#pragma once

#include <glm/glm.hpp>

class Camera {
public:
	virtual ~Camera() = default;

	virtual void startUp() = 0;
	virtual void shutDown() = 0;
	virtual glm::mat4 GetProjectionMatrix() const = 0;
	virtual glm::mat4 GetViewMatrix() const = 0;
	virtual glm::vec3 GetPosition() const = 0;
	virtual glm::vec3 GetFacing() const = 0;
};
