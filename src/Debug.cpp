#include "Debug.h"

#include "Axis.h"
#include "EngineGUI.h"
#include "Grid.h"
#include "Camera.h"
#include "Input.h"

#include <imgui.h>
#include <sstream>

void Debug::startUp()
{
	m_axis = new Axis(1200.0f);
	RebuildGrid();
}

void Debug::shutDown()
{
	delete m_axis;
	m_axis = nullptr;
	delete m_grid;
	m_grid = nullptr;
	m_logMessages.clear();
}

void Debug::RebuildGrid()
{
	delete m_grid;
	m_grid = new Grid(m_gridSize, m_gridSpacing);
}

void Debug::draw(const Camera& camera, const EngineGUI& gui)
{
	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();

	if (m_axis && gui.ShowAxis())
	{
		m_axis->UpdateProjection(projection);
		m_axis->draw(view);
	}

	if (m_grid && gui.ShowGrid())
	{
		m_grid->UpdateProjection(projection);
		m_grid->draw(view, !gui.ShowAxis());
	}

	if (m_loggingEnabled)
	{
		ImGui::Begin("Debug Log");
		if (ImGui::Button("Clear"))
		{
			m_logMessages.clear();
			m_logFrameCount = 0;
		}
		ImGui::Separator();
		for (const std::string& message : m_logMessages)
		{
			ImGui::TextUnformatted(message.c_str());
		}
		ImGui::End();
	}
}

void Debug::LogMessage(const std::string& message)
{
	if (!m_loggingEnabled)
	{
		return;
	}

	m_logMessages.push_back(message);
	if (m_logMessages.size() > 180)
	{
		m_logMessages.erase(m_logMessages.begin());
	}
}

void Debug::SetLoggingEnabled(bool enabled)
{
	m_loggingEnabled = enabled;
	m_logFrameCount = 0;
}

void Debug::SetGridSettings(float size, float spacing)
{
	if (size <= 0.0f)
	{
		size = 1.0f;
	}

	if (spacing <= 0.0f)
	{
		spacing = 1.0f;
	}

	m_gridSize = size;
	m_gridSpacing = spacing;
	RebuildGrid();
}

float Debug::GridSize() const
{
	return m_gridSize;
}

float Debug::GridSpacing() const
{
	return m_gridSpacing;
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

	std::ostringstream oss;
	oss
		<< "[debug frame " << m_logFrameCount << "] "
		<< "dt=" << input.DeltaTime()
		<< " windowFocused=" << input.WindowFocused()
		<< " lookActive=" << input.LookActive()
		<< " lookBecameActive=" << input.LookBecameActive()
		<< " mouseDelta=(" << mouseDelta.x << ", " << mouseDelta.y << ")"
		<< " moveInput=(" << moveInput.x << ", " << moveInput.y << ", " << moveInput.z << ")"
		<< " camPos=(" << position.x << ", " << position.y << ", " << position.z << ")"
		<< " camFacing=(" << facing.x << ", " << facing.y << ", " << facing.z << ")";

	LogMessage(oss.str());

	++m_logFrameCount;
}
