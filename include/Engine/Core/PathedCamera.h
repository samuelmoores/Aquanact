#pragma once

#include "Engine/Core/Camera.h"
#include "Engine/Core/CameraPathData.h"
#include "GLFW/glfw3.h"

class Entity;

class PathedCamera final : public Camera {
public:
	// Lifecycle: acquire and release the window resource used for projection.
	PathedCamera() = default;
	~PathedCamera() override = default;
	void startUp() override;
	void shutDown() override;

	// Camera interface: provide the matrices and orientation consumed by rendering.
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;

	// Pose and path authoring: directly set the camera pose or replace its path.
	void SetPose(const glm::vec3& position, const glm::vec3& facing);
	void SetPath(const CameraPathData& path);
	const CameraPathData& Path() const { return m_path; }
	CameraPathData& MutablePath() { return m_path; }

	// Targeting: identify the entity the camera should face while it follows.
	void SetTarget(Entity* target);
	Entity* Target() const { return m_target; }
	void SetOverridePosition(const glm::vec3& position);
	void ClearOverridePosition();
	bool HasOverridePosition() const { return m_hasOverridePosition; }

	// Path-follow state: control progress and the quality of closest-point queries.
	void SetPlayerProgress(float progress);
	float PlayerProgress() const { return m_playerProgress; }
	float ClosestPathDistance(const glm::vec3& worldPosition) const;
	void SetPathSamplesPerSegment(int samples);
	int PathSamplesPerSegment() const { return m_pathSamplesPerSegment; }

	// Runtime follow behavior: advance toward the path position and tune smoothing.
	void Update(float deltaTime);
	void SetFollowSharpness(float sharpness);
	float FollowSharpness() const { return m_followSharpness; }

private:
	// Private helpers: update orientation toward the target and rebuild the view matrix.
	void FaceTarget();
	void ApplyAuthoredFacing(const glm::vec3& facing);
	void RebuildView();

	// Projection configuration.
	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;

	// Runtime camera pose and cached rendering state.
	GLFWwindow* m_window = nullptr;
	glm::mat4 m_viewMatrix{1.0f};
	glm::vec3 m_position{0.0f, 0.0f, -10.0f};
	glm::vec3 m_facing{0.0f, 0.0f, 1.0f};
	glm::vec3 m_up{0.0f, 1.0f, 0.0f};

	// Authored path and the entity used for camera-facing behavior.
	CameraPathData m_path;
	Entity* m_target = nullptr;
	glm::vec3 m_overridePosition{0.0f};
	bool m_hasOverridePosition = false;

	// Follow controls: normalized player progress, smoothing, and fixed sampling.
	float m_playerProgress = 0.0f;
	float m_followSharpness = 8.0f;
	int m_pathSamplesPerSegment = 16;
};
