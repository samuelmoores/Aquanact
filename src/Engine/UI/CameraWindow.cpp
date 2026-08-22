#include "Engine/UI/CameraWindow.h"

#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/CameraPathData.h"
#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/CameraPathCreator.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <cstddef>
#include <string>

void CameraWindow::Draw(CameraPathCreator& cameraPath, bool& open, bool& showCameraPath)
{
	if (!open) return;

	EngineGuiWidgets::WindowScope window(
		"Camera",
		&open,
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	if (!window) return;

	CameraPathData& path = cameraPath.Data();
	ImGui::TextUnformatted("Camera Path");
	ImGui::Checkbox("Show Camera Path", &showCameraPath);
	PathedCamera& pathedCamera = Root::Current().Render().GetPathedCamera();
	float followSharpness = pathedCamera.FollowSharpness();
	if (EngineGuiWidgets::LabeledFloat("Follow Sharpness", followSharpness, 0.1f, 0.0f, 50.0f))
	{
		pathedCamera.SetFollowSharpness(followSharpness);
	}
	int curveSamples = pathedCamera.PathSamplesPerSegment();
	if (ImGui::SliderInt("Curve Samples", &curveSamples, 4, 256))
	{
		pathedCamera.SetPathSamplesPerSegment(curveSamples);
	}

	if (ImGui::Button("Add Point"))
	{
		cameraPath.AddPoint(Root::Current().Render().GetEngineCamera().GetPosition());
	}
	ImGui::SameLine();
	const int selectedPoint = cameraPath.SelectedPoint();
	if (ImGui::Button("Remove Point") && selectedPoint >= 0)
	{
		cameraPath.RemovePoint(static_cast<std::size_t>(selectedPoint));
	}
	ImGui::Separator();
	EngineGuiWidgets::Vector3Editor("Curve Offset", m_curveTranslation, 0.1f);
	if (ImGui::Button("Move Entire Curve"))
	{
		cameraPath.TranslateAll(m_curveTranslation);
		m_curveTranslation = glm::vec3(0.0f);
	}

	for (std::size_t index = 0; index < path.points.size(); ++index)
	{
		const std::string label = "Point " + std::to_string(index + 1);
		if (ImGui::Selectable(label.c_str(), cameraPath.SelectedPoint() == static_cast<int>(index)))
		{
			cameraPath.SelectPoint(static_cast<int>(index));
		}
	}

	const int editedPoint = cameraPath.SelectedPoint();
	if (editedPoint >= 0 && editedPoint < static_cast<int>(path.points.size()))
	{
		CameraPathPoint& point = path.points[static_cast<std::size_t>(editedPoint)];
		ImGui::Separator();
		EngineGuiWidgets::Vector3Editor("Position", point.position, 0.1f);
		if (ImGui::Button("Capture Editor Camera Position"))
		{
			point.position = Root::Current().Render().GetEngineCamera().GetPosition();
		}
	}
}
