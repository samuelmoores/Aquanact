#pragma once

#include "Engine/Core/Camera.h"
#include "Engine/Core/CameraPathData.h"
#include "GLFW/glfw3.h"

class Entity;

class PathedCamera final : public Camera {
public:
	PathedCamera() = default;
	~PathedCamera() override = default;

	void startUp() override;
	void shutDown() override;
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;

	void SetPose(const glm::vec3& position, const glm::vec3& facing);
	void SetPath(const CameraPathData& path);
	const CameraPathData& Path() const { return m_path; }
	void SetTarget(Entity* target);
	Entity* Target() const { return m_target; }
	void SetPlayerProgress(float progress);
	float PlayerProgress() const { return m_playerProgress; }
	float ClosestPathDistance(const glm::vec3& worldPosition) const;
	void SetPathSamplesPerSegment(int samples);
	int PathSamplesPerSegment() const { return m_pathSamplesPerSegment; }
	void Update(float deltaTime);
	void SetFollowSharpness(float sharpness);
	float FollowSharpness() const { return m_followSharpness; }

private:
	void RebuildView();
	void FaceTarget();

	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;
	GLFWwindow* m_window = nullptr;
	glm::mat4 m_viewMatrix{1.0f};
	glm::vec3 m_position{0.0f, 0.0f, -10.0f};
	glm::vec3 m_facing{0.0f, 0.0f, 1.0f};
	glm::vec3 m_up{0.0f, 1.0f, 0.0f};
	CameraPathData m_path;
	Entity* m_target = nullptr;
	float m_playerProgress = 0.0f;
	float m_followSharpness = 8.0f;
	int m_pathSamplesPerSegment = 32;
};
