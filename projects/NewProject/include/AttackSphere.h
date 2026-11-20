#pragma once

#include "Engine/Core/Component.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>

class Entity;
class Scene;
class EntityStateMachine;
class Input;
class InputManager;
class Root;

// Generated gameplay component scaffold.
// Keep the binding lists as the single source of truth for exposed data.
// Add bindable values and events here when the component needs editor metadata.
class AttackSphere final : public Component
{
public:
	AttackSphere() = default;

	const char* Name() const override { return "AttackSphere"; }
	int ExecutionOrder() const override { return 100; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override;
	void FirstFrame(Entity&) override {}
	bool Launch(Scene& scene, Entity* ignoredEntity, float speed,
		Entity* explicitTarget = nullptr, const std::string& targetBoneName = {},
		const std::string& targetInstanceName = {});
	bool Launched() const { return m_launched; }

	// Put the component's exposed value list here. This is the only place
	// that should enumerate values the editor needs to see.

private:
	enum class TravelPhase
	{
		Outbound,
		WaitingForHurt,
		ReturningToPlayer,
		AtPlayerHead
	};

	Entity* m_target = nullptr;
	Entity* m_player = nullptr;
	Scene* m_scene = nullptr;
	std::string m_targetBoneName;
	std::string m_targetInstanceName;
	std::string m_playerBoneName = "mixamorig:Head";
	glm::vec3 m_start{0.0f};
	glm::vec3 m_controlStart{0.0f};
	glm::vec3 m_controlEnd{0.0f};
	glm::vec3 m_end{0.0f};
	glm::vec3 m_startScale{1.0f};
	float m_elapsed = 0.0f;
	float m_duration = 0.0f;
	float m_speed = 0.0f;
	bool m_launched = false;
	bool m_hitTarget = false;
	bool m_observedHurtAnimation = false;
	TravelPhase m_travelPhase = TravelPhase::Outbound;
};
