#include "Debug.h"

#include "Axis.h"
#include "Camera.h"

void Debug::startUp()
{
	m_axis = new Axis(10.0f);
}

void Debug::shutDown()
{
	delete m_axis;
	m_axis = nullptr;
}

void Debug::draw(const Camera& camera)
{
	if (!m_axis) {
		return;
	}

	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();

	m_axis->UpdateProjection(projection);

	m_axis->draw(view);
}
