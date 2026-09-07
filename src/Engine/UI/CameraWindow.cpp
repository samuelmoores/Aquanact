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
#include <array>
#include <cmath>
#include <string>

namespace
{
	struct FollowSharpnessPreset
	{
		const char* label;
		float value;
	};

	constexpr std::array<FollowSharpnessPreset, 4> kFollowSharpnessPresets = {{
		{ "None", 0.0f },
		{ "Weak", 2.0f },
		{ "Moderate", 8.0f },
		{ "Strong", 20.0f }
	}};

	int ClosestFollowSharpnessPreset(float value)
	{
		int closest = 0;
		float closestDistance = std::abs(value - kFollowSharpnessPresets[0].value);
		for (int index = 1; index < static_cast<int>(kFollowSharpnessPresets.size()); ++index)
		{
			const float distance = std::abs(value - kFollowSharpnessPresets[index].value);
			if (distance < closestDistance)
			{
				closest = index;
				closestDistance = distance;
			}
		}
		return closest;
	}
}

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
	int followSharpnessPreset = ClosestFollowSharpnessPreset(pathedCamera.FollowSharpness());
	if (ImGui::BeginCombo("Follow Sharpness", kFollowSharpnessPresets[followSharpnessPreset].label))
	{
		for (int index = 0; index < static_cast<int>(kFollowSharpnessPresets.size()); ++index)
		{
			const bool selected = followSharpnessPreset == index;
			if (ImGui::Selectable(kFollowSharpnessPresets[index].label, selected))
			{
				followSharpnessPreset = index;
				pathedCamera.SetFollowSharpness(kFollowSharpnessPresets[index].value);
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
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
		ImGui::Checkbox("Look At Player", &point.lookAtPlayer);
		if (!point.lookAtPlayer)
		{
			EngineGuiWidgets::Vector3Editor("Facing", point.facing, 0.01f);
			if (ImGui::Button("Capture Editor Camera Rotation"))
			{
				point.facing = Root::Current().Render().GetEngineCamera().GetFacing();
			}
		}
		ImGui::Checkbox("Island", &point.island);
		if (point.island)
		{
			bool cameraSelected = !cameraPath.TriggerSelected();
			bool triggerSelected = cameraPath.TriggerSelected();
			if (ImGui::Checkbox("Camera", &cameraSelected) && cameraSelected)
			{
				cameraPath.SelectCamera();
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Trigger", &triggerSelected) && triggerSelected)
			{
				cameraPath.SelectTrigger();
			}
			EngineGuiWidgets::Vector3Editor("Trigger Position", point.triggerPosition, 0.1f);
			float triggerRadius = point.triggerRadius;
			if (EngineGuiWidgets::LabeledFloat("Trigger Radius", triggerRadius, 0.1f, 0.0f, 10000.0f))
			{
				point.triggerRadius = triggerRadius;
			}
		}
		if (ImGui::Button("Capture Editor Camera Position"))
		{
			point.position = Root::Current().Render().GetEngineCamera().GetPosition();
		}
	}
}
