#include "Debug.h"

#include "Axis.h"
#include "Grid.h"
#include "Camera.h"
#include "Input.h"
#include <iostream>

void Debug::startUp()
{
	m_axis = new Axis(20.0f);
	m_grid = new Grid(20.0f, 1.0f);
}

void Debug::shutDown()
{
	delete m_axis;
	m_axis = nullptr;
	delete m_grid;
	m_grid = nullptr;
}

void Debug::draw(const Camera& camera)
{
	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();

	if (m_axis)
	{
		m_axis->UpdateProjection(projection);
		m_axis->draw(view);
	}

	if (m_grid)
	{
		m_grid->UpdateProjection(projection);
		m_grid->draw(view);
	}
}

void Debug::SetLoggingEnabled(bool enabled)
{
	m_loggingEnabled = enabled;
	m_logFrameCount = 0;
}

void Debug::LogFrame(const Camera& camera, const Input& input)
{
	if (!m_loggingEnabled)
	{
		return;
	}

	if (m_logFrameCount >= 180)
	{
		return;
	}

	const glm::vec3 position = camera.GetPosition();
	const glm::vec3 facing = camera.GetFacing();
	const glm::vec3 moveInput = input.MoveInput();
	const glm::vec2 mouseDelta = input.MouseDelta();

	std::cout
		<< "[debug frame " << m_logFrameCount << "] "
		<< "dt=" << input.DeltaTime()
		<< " windowFocused=" << input.WindowFocused()
		<< " lookActive=" << input.LookActive()
		<< " lookBecameActive=" << input.LookBecameActive()
		<< " mouseDelta=(" << mouseDelta.x << ", " << mouseDelta.y << ")"
		<< " moveInput=(" << moveInput.x << ", " << moveInput.y << ", " << moveInput.z << ")"
		<< " camPos=(" << position.x << ", " << position.y << ", " << position.z << ")"
		<< " camFacing=(" << facing.x << ", " << facing.y << ", " << facing.z << ")"
		<< std::endl;

	++m_logFrameCount;
}
