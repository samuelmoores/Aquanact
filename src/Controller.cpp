#include "Controller.h"

#include "Input.h"
#include "Globals.h"
#include "Debug.h"
#include "Object3D.h"

void Controller::Update(Object3D&, float dt)
{
	if (m_owner == nullptr)
	{
		gDebug.SetGameplayDiagnostics(false, "<unbound>", glm::vec3(0.0f), m_moveSpeed, dt, glm::vec3(0.0f), glm::vec3(0.0f));
		return;
	}

	const glm::vec3 moveInput = gInput.MoveInput();
	if (moveInput == glm::vec3(0.0f))
	{
		gDebug.SetGameplayDiagnostics(true, m_owner->Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), m_owner->Position());
		return;
	}

	const glm::vec3 delta = moveInput * m_moveSpeed * dt;
	m_owner->Move(delta);
	gDebug.SetGameplayDiagnostics(true, m_owner->Name(), moveInput, m_moveSpeed, dt, delta, m_owner->Position());
}
