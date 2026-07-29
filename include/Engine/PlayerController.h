#pragma once

#include "Engine/Controller.h"

class Input;

class PlayerController final : public Controller
{
public:
	PlayerController() = default;

	const char* Name() const override { return "PlayerController"; }
	void startUp(Entity& owner) override;
	void Update(Entity& owner, float dt) override;

	void SetInputDevice(const Input& input) { m_inputDevice = &input; }
	const Input* InputDevice() const { return m_inputDevice; }
	float TurnSpeed() const { return m_turnSpeed; }
	void SetTurnSpeed(float turnSpeed) { m_turnSpeed = turnSpeed; }

private:
	static float WrapAngle(float angle);
	static float ShortestAngleDelta(float from, float to);

	const Input* m_inputDevice = nullptr;
	float m_turnSpeed = 8.0f;
};
