#include "CharacterEditor.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/Line.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "PlayerController.h"
#include "TriggerSphereDresser.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	std::vector<LineVertex3D> MakeCharacterEditorSphereVertices(const glm::vec3& color)
	{
		std::vector<LineVertex3D> vertices;
		constexpr int segments = 24;
		constexpr float pi = 3.14159265358979323846f;
		for (int segment = 0; segment < segments; ++segment)
		{
			const float a = 2.0f * pi * static_cast<float>(segment) / segments;
			const float b = 2.0f * pi * static_cast<float>(segment + 1) / segments;
			const glm::vec3 points[3][2] = {
				{{std::cos(a), std::sin(a), 0.0f}, {std::cos(b), std::sin(b), 0.0f}},
				{{std::cos(a), 0.0f, std::sin(a)}, {std::cos(b), 0.0f, std::sin(b)}},
				{{0.0f, std::cos(a), std::sin(a)}, {0.0f, std::cos(b), std::sin(b)}}
			};
			for (const auto& line : points)
				for (const glm::vec3& point : line)
					vertices.push_back({point.x, point.y, point.z, color.r, color.g, color.b});
		}
		return vertices;
	}

	void BasisFromEulerDegrees(const glm::vec3& rotationDegrees, glm::vec3& facing, glm::vec3& up)
	{
		const float pitch = glm::radians(rotationDegrees.x);
		const float yaw = glm::radians(rotationDegrees.y);
		const float roll = glm::radians(rotationDegrees.z);
		facing = glm::normalize(glm::vec3(
			std::sin(yaw) * std::cos(pitch),
			std::sin(pitch),
			std::cos(yaw) * std::cos(pitch)));
		const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), facing));
		const glm::vec3 unrolledUp = glm::normalize(glm::cross(facing, right));
		up = unrolledUp * std::cos(roll) + glm::cross(facing, unrolledUp) * std::sin(roll);
	}
}

CharacterEditor::~CharacterEditor()
{
	delete m_positionDebugSphere;
	delete m_cameraDebugSphere;
}

glm::mat4 CharacterEditorPreviewCamera::GetProjectionMatrix() const
{
	return Root::Current().Render().GetPathedCamera().GetProjectionMatrix();
}

void CharacterEditorPreviewCamera::SetPose(
	const glm::vec3& position, const glm::vec3& facing, const glm::vec3& up)
{
	m_position = position;
	m_facing = glm::dot(facing, facing) > 1e-8f
		? glm::normalize(facing) : glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 safeUp = up - m_facing * glm::dot(up, m_facing);
	if (glm::dot(safeUp, safeUp) <= 1e-8f)
		safeUp = std::abs(glm::dot(m_facing, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.999f
			? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
	else
		safeUp = glm::normalize(safeUp);
	m_view = glm::lookAt(m_position, m_position + m_facing, safeUp);
}

void CharacterEditor::startUp(Entity& owner)
{
	(void)owner;
}

void CharacterEditor::Update(Entity& owner, float dt)
{
	(void)owner;
	(void)dt;
	const bool ePressed = Root::Current().InputRef().KeyDown(GLFW_KEY_E);
	const bool eJustPressed = ePressed && !m_previousEPressed;
	m_previousEPressed = ePressed;

	Scene* scene = Root::Current().Scenes().ActiveLevel();
	if (!scene)
	{
		if (m_open)
			Leave();
		m_player = nullptr;
		m_playerController = nullptr;
		m_dresserTrigger = nullptr;
		m_insideDresserTrigger = false;
		return;
	}

	m_player = nullptr;
	m_playerController = nullptr;
	m_dresserTrigger = nullptr;
	for (const auto& entity : scene->Objects())
	{
		if (!entity)
			continue;
		if (!m_playerController)
		{
			if (PlayerController* controller = entity->GetComponent<PlayerController>())
			{
				m_player = entity.get();
				m_playerController = controller;
			}
		}
		if (!m_dresserTrigger && entity->GetComponent<TriggerSphereDresser>())
			m_dresserTrigger = entity.get();
	}
	if (!m_player || !m_playerController || !m_dresserTrigger)
	{
		if (m_open)
			Leave();
		m_insideDresserTrigger = false;
		return;
	}

	const TriggerSphereDresser* trigger = m_dresserTrigger->GetComponent<TriggerSphereDresser>();
	const auto overlapping = PhysicsWorld::Instance().QuerySphere(
		m_dresserTrigger->WorldPosition(), trigger->Radius(), m_dresserTrigger);
	m_insideDresserTrigger = std::find(overlapping.begin(), overlapping.end(), m_player) != overlapping.end();

	if (m_open)
	{
		m_playerController->SetMovementLocked(true);
		FacePlayerToCamera(*m_player);
		ApplyEditorCamera();
		if (eJustPressed)
			Leave();
		return;
	}
	if (m_insideDresserTrigger && eJustPressed)
		Enter(*m_player);
}

void CharacterEditor::Enter(Entity& player)
{
	if (!m_dresserTrigger || !m_playerController)
		return;

	m_previousMovementLock = m_playerController->MovementLocked();

	const glm::vec3 targetPosition = DestinationPosition(player);
	player.Translate(targetPosition - player.WorldPosition());
	player.SetLinearVelocity(glm::vec3(0.0f));
	PhysicsWorld::Instance().Update(player);
	FacePlayerToCamera(player);
	m_playerController->SetMovementLocked(true);
	m_open = true;
	DispatchBindableValueChanged("IsOpen");
	ApplyEditorCamera();
}

void CharacterEditor::FacePlayerToCamera(Entity& player)
{
	glm::vec3 towardCamera = m_cameraPosition - player.WorldCenterPosition();
	towardCamera.y = 0.0f;
	if (glm::dot(towardCamera, towardCamera) <= 1e-8f)
		return;

	const glm::vec3 rotation = player.Rotation();
	player.SetRotation(glm::vec3(rotation.x, std::atan2(towardCamera.x, towardCamera.z), rotation.z));
}

glm::vec3 CharacterEditor::DestinationPosition(Entity& player) const
{
	if (!m_dresserTrigger)
		return player.WorldPosition();

	const glm::vec3 localOffset(m_playerPositionOffset.x, 0.0f, m_playerPositionOffset.y);
	glm::vec3 worldOffset = glm::vec3(m_dresserTrigger->BuildModelMatrix() * glm::vec4(localOffset, 0.0f));
	worldOffset.y = 0.0f;
	glm::vec3 horizontalOffset = m_dresserTrigger->WorldPosition() + worldOffset - player.WorldPosition();
	horizontalOffset.y = 0.0f;

	glm::vec3 destination = player.WorldPosition() + horizontalOffset;
	constexpr float maximumGroundSnapDistance = 10000.0f;
	const Physics::SweepCollision ground = PhysicsWorld::Instance().SweepEntity(
		player, glm::vec3(0.0f, -maximumGroundSnapDistance, 0.0f), horizontalOffset);
	if (ground.hit && ground.normal.y > 0.25f)
		destination.y -= maximumGroundSnapDistance * glm::clamp(ground.time, 0.0f, 1.0f);
	return destination;
}

void CharacterEditor::ApplyEditorCamera()
{
	glm::vec3 facing;
	glm::vec3 up;
	BasisFromEulerDegrees(m_cameraRotationDegrees, facing, up);
	m_previewCamera.SetPose(m_cameraPosition, facing, up);
}

Camera* CharacterEditor::EditorPreviewCamera()
{
	if (m_open && Root::Current().State().IsEditorMode())
	{
		m_playerController = nullptr;
		m_player = nullptr;
		if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
			for (const auto& entity : scene->Objects())
				if (entity && (m_playerController = entity->GetComponent<PlayerController>()))
				{
					m_player = entity.get();
					break;
				}
		Leave();
	}
	if (!m_previewEnabled)
		return nullptr;
	ApplyEditorCamera();
	return &m_previewCamera;
}

Camera* CharacterEditor::RuntimeCameraOverride()
{
	if (!m_open)
		return nullptr;
	ApplyEditorCamera();
	return &m_previewCamera;
}

bool CharacterEditor::GetEditorCameraPose(glm::vec3& position, glm::vec3& facing, glm::vec3& up) const
{
	position = m_cameraPosition;
	BasisFromEulerDegrees(m_cameraRotationDegrees, facing, up);
	return true;
}

void CharacterEditor::CaptureEngineCameraView()
{
	const EngineCamera& camera = Root::Current().Render().GetEngineCamera();
	m_cameraPosition = camera.GetPosition();
	const glm::vec3 facing = glm::normalize(camera.GetFacing());
	m_cameraRotationDegrees.x = glm::degrees(std::asin(glm::clamp(facing.y, -1.0f, 1.0f)));
	m_cameraRotationDegrees.y = glm::degrees(std::atan2(facing.x, facing.z));
	m_cameraRotationDegrees.z = 0.0f;
}

void CharacterEditor::DrawDebug(const Camera& camera)
{
	if (!m_dresserTrigger)
		if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
			for (const auto& entity : scene->Objects())
				if (entity && entity->GetComponent<TriggerSphereDresser>())
				{
					m_dresserTrigger = entity.get();
					break;
				}
	if (!m_dresserTrigger)
		return;
	if (!m_player)
		if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
			for (const auto& entity : scene->Objects())
				if (entity && entity->GetComponent<PlayerController>())
				{
					m_player = entity.get();
					break;
				}
	const glm::vec3 playerPosition = m_player ? DestinationPosition(*m_player) : m_dresserTrigger->WorldPosition();
	if (m_showPositionDebugSphere)
	{
		if (!m_positionDebugSphere)
			m_positionDebugSphere = new Line(MakeCharacterEditorSphereVertices(glm::vec3(0.2f, 1.0f, 0.35f)));
		m_positionDebugSphere->UpdateProjection(camera.GetProjectionMatrix());
		m_positionDebugSphere->draw(camera.GetViewMatrix(),
			glm::translate(glm::mat4(1.0f), playerPosition) *
			glm::scale(glm::mat4(1.0f), glm::vec3(24.0f)));
	}
	if (m_showCameraDebugSphere)
	{
		if (!m_cameraDebugSphere)
			m_cameraDebugSphere = new Line(MakeCharacterEditorSphereVertices(glm::vec3(0.0f, 0.65f, 1.0f)));
		m_cameraDebugSphere->UpdateProjection(camera.GetProjectionMatrix());
		m_cameraDebugSphere->draw(camera.GetViewMatrix(),
			glm::translate(glm::mat4(1.0f), m_cameraPosition) *
			glm::scale(glm::mat4(1.0f), glm::vec3(18.0f)));
	}
}

void CharacterEditor::Leave()
{
	if (m_playerController)
		m_playerController->SetMovementLocked(m_previousMovementLock);
	const bool wasOpen = m_open;
	m_open = false;
	if (wasOpen)
		DispatchBindableValueChanged("IsOpen");
}
