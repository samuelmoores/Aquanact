#include "Debug.h"

#include "Axis.h"
#include "EngineGUI.h"
#include "Grid.h"
#include "Camera.h"
#include "Globals.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "FrameAllocator.h"

#include <imgui.h>
#include <chrono>

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
	static bool firstFrame = true;
	static auto lastFrameTime = std::chrono::high_resolution_clock::now();
	const auto now = std::chrono::high_resolution_clock::now();
	if (firstFrame)
	{
		firstFrame = false;
		m_lastFps = 0.0f;
	}
	else
	{
		const std::chrono::duration<float> dt = now - lastFrameTime;
		m_lastFps = (dt.count() > 0.0f) ? (1.0f / dt.count()) : 0.0f;
	}
	lastFrameTime = now;

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

	ImGui::Begin("Debug Log");
	if (ImGui::Button("Clear"))
	{
		m_logMessages.clear();
	}
	ImGui::Separator();
	for (const std::string& message : m_logMessages)
	{
		ImGui::TextUnformatted(message.c_str());
	}
	ImGui::End();

	ImGui::Begin("Debug Stats");
	ImGui::Text("FPS: %.1f", m_lastFps);
	ImGui::Text("Scene objects: %zu", gSceneManager.Objects().size());
	ImGui::Separator();
	ImGui::Text("Render commands: %zu", gRenderManager.LastFrameCommandCount());
	ImGui::Text("Skipped objects: %zu", gRenderManager.LastFrameSkippedObjects());
	ImGui::Text("Build time: %.4f ms", gRenderManager.LastFrameBuildMs());
	ImGui::Text("Flush time: %.4f ms", gRenderManager.LastFrameFlushMs());
	ImGui::Separator();
	ImGui::Text("Frame allocator capacity: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorCapacityBytes()) / 1024.0);
	ImGui::Text("Frame allocator used: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorUsedBytes()) / 1024.0);
	ImGui::Text("Frame allocator peak: %.2f KB", static_cast<double>(gRenderManager.FrameAllocatorPeakBytes()) / 1024.0);
	ImGui::End();
}

void Debug::LogMessage(const std::string& message)
{
	m_logMessages.push_back(message);
	if (m_logMessages.size() > 180)
	{
		m_logMessages.erase(m_logMessages.begin());
	}
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
