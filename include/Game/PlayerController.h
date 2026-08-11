#pragma once

#include "Engine/Core/Controller.h"

class Input;
class InputManager;
class EntityStateMachine;

// Game-specific input controller for the player-controlled entity.
class PlayerController final : public Controller
{
public:
	PlayerController() = default;

	// Component identity and runtime entry points.
	const char* Name() const override { return "PlayerController"; }
	void startUp(Entity& owner) override;
	void FirstFrame(Entity& owner) override;
	void Update(Entity& owner, float dt) override;

	// Editor-facing tuning values.
	float TurnSpeed() const { return m_turnSpeed; }
	void SetTurnSpeed(float turnSpeed) { m_turnSpeed = turnSpeed; }
	bool WantsToMove() const { return m_wantsToMove; }

	// Bindable player-controller settings shown in the editor.
	// Keep this list limited to stable gameplay tuning values that are useful to inspect or edit.
	#define PLAYER_CONTROLLER_BINDABLES(VALUE, FUNCTION) \
		FUNCTION(IsMoving) \
		FUNCTION(WantsToMove) \
		FUNCTION(IsGrounded) \
		VALUE(m_moveSpeed) \
		VALUE(m_turnSpeed) 
	AQUA_DECLARE_BINDABLES(PLAYER_CONTROLLER_BINDABLES)
	#undef PLAYER_CONTROLLER_BINDABLES

	// Cached references to the systems this controller reads from every frame.
	void SetInputDevice(const Input& input) { m_inputDevice = &input; }
	const Input* InputDevice() const { return m_inputDevice; }
	void SetInputActions(const InputManager& inputActions) { m_inputActions = &inputActions; }
	const InputManager* InputActions() const { return m_inputActions; }

private:
	// Shared helpers for movement and facing math.
	static float WrapAngle(float angle);
	static float ShortestAngleDelta(float from, float to);
	void Move(Entity& owner, const glm::vec2& move2D, float dt);

	// External input sources.
	const Input* m_inputDevice = nullptr;
	const InputManager* m_inputActions = nullptr;
	EntityStateMachine* m_entityState = nullptr;
	bool m_wantsToMove = false;

	// Player-specific tuning.
	float m_turnSpeed = 8.0f;
};

