#include "Engine/UI/LightingWindow.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <string>

void LightingWindow::Draw(LightingManager& lightingManager, bool& open)
{
	if (!open) return;

	EngineGuiWidgets::WindowScope window(
		"Lighting",
		&open,
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
	if (!window) return;

	bool shadowsEnabled = lightingManager.ShadowsEnabled();
	if (ImGui::Checkbox("Enable Shadows", &shadowsEnabled))
	{
		lightingManager.SetShadowsEnabled(shadowsEnabled);
	}

	ImGui::Separator();
	DirectionalLight& sunLight = lightingManager.SunLight();
	if (ImGui::CollapsingHeader("Sun Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Direction", &sunLight.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
		ImGui::ColorEdit3("Color", &sunLight.color.x);
		ImGui::DragFloat("Intensity", &sunLight.intensity, 0.001f, 0.0f, 10.0f, "%.3f");
		ImGui::DragFloat("Ambient", &sunLight.ambient, 0.001f, 0.0f, 1.0f, "%.3f");
		ImGui::Checkbox("Casts Shadow", &sunLight.castsShadows);
		if (ImGui::Button("Reset Sun"))
		{
			sunLight.direction = glm::vec3(-0.3f, -1.0f, 0.2f);
			sunLight.color = glm::vec3(1.0f);
			sunLight.intensity = 1.0f;
			sunLight.ambient = 0.5f;
			sunLight.castsShadows = true;
		}
	}

	ImGui::SeparatorText("Point Lights");
	bool showDebugSpheres = Root::Current().Debugger().ShowPointLightDebugSpheres();
	if (ImGui::Checkbox("Show Debug Spheres", &showDebugSpheres))
	{
		Root::Current().Debugger().SetShowPointLightDebugSpheres(showDebugSpheres);
	}
	std::vector<PointLight>& pointLights = lightingManager.PointLights();
	for (int i = 0; i < static_cast<int>(pointLights.size()); ++i)
	{
		PointLight& pointLight = pointLights[i];
		ImGui::PushID(i);
		const std::string header = "Point Light " + std::to_string(i + 1);
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position", &pointLight.position.x, 0.05f, -1000.0f, 1000.0f, "%.2f");
			ImGui::ColorEdit3("Color", &pointLight.color.x);
			ImGui::DragFloat("Intensity", &pointLight.intensity, 0.01f, 0.0f, 50.0f, "%.2f");
			ImGui::DragFloat("Ambient", &pointLight.ambient, 0.001f, 0.0f, 1.0f, "%.3f");
			ImGui::Checkbox("Casts Shadow", &pointLight.castsShadows);
			float radius = pointLight.radius;
			if (ImGui::DragFloat("Radius", &radius, 5.0f, 0.001f, 5000.0f, "%.2f"))
			{
				pointLight.SetRadius(radius);
			}
			ImGui::DragFloat("Radius Fade", &pointLight.radiusFade, 0.01f, 0.0f, 1.0f, "%.2f");
		}
		ImGui::PopID();
	}
}
