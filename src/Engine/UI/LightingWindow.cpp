#include "Engine/UI/LightingWindow.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <string>

void LightingWindow::Draw(LightingManager& lightingManager, bool& open,
	int& selectedPointLightIndex, int& selectedSpotLightIndex)
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
	int pointLightToDelete = -1;
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
			if (ImGui::Button("Delete Light"))
			{
				pointLightToDelete = i;
			}
		}
		ImGui::PopID();
	}

	if (pointLightToDelete >= 0 && pointLightToDelete < static_cast<int>(pointLights.size()))
	{
		pointLights.erase(pointLights.begin() + pointLightToDelete);
		if (selectedPointLightIndex == pointLightToDelete)
		{
			selectedPointLightIndex = -1;
		}
		else if (selectedPointLightIndex > pointLightToDelete)
		{
			--selectedPointLightIndex;
		}
	}

	ImGui::SeparatorText("Spot Lights");
	std::vector<SpotLight>& spotLights = lightingManager.SpotLights();
	if (spotLights.size() < static_cast<std::size_t>(LightingManager::MaxSpotLights)
		&& ImGui::Button("Add Spot Light"))
		lightingManager.AddSpotLight();
	int spotLightToDelete = -1;
	for (int i = 0; i < static_cast<int>(spotLights.size()); ++i)
	{
		SpotLight& spotLight = spotLights[i];
		ImGui::PushID(1000 + i);
		const std::string header = "Spot Light " + std::to_string(i + 1);
		if (ImGui::CollapsingHeader(header.c_str()))
		{
			ImGui::DragFloat3("Position", &spotLight.position.x, 0.05f, -1000.0f, 1000.0f, "%.2f");
			ImGui::DragFloat3("Direction", &spotLight.direction.x, 0.01f, -1.0f, 1.0f, "%.3f");
			if (glm::length(spotLight.direction) < 0.001f)
				spotLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
			else
				spotLight.direction = glm::normalize(spotLight.direction);
			ImGui::ColorEdit3("Color", &spotLight.color.x);
			ImGui::DragFloat("Intensity", &spotLight.intensity, 0.01f, 0.0f, 50.0f, "%.2f");
			ImGui::DragFloat("Ambient", &spotLight.ambient, 0.001f, 0.0f, 1.0f, "%.3f");
			ImGui::Checkbox("Casts Shadow", &spotLight.castsShadows);
			float radius = spotLight.radius;
			if (ImGui::DragFloat("Radius", &radius, 5.0f, 0.001f, 5000.0f, "%.2f"))
				spotLight.SetRadius(radius);
			ImGui::DragFloat("Radius Fade", &spotLight.radiusFade, 0.01f, 0.0f, 1.0f, "%.2f");
			float innerAngle = spotLight.innerAngle;
			float outerAngle = spotLight.outerAngle;
			const bool innerAngleChanged = ImGui::SliderFloat("Inner Cone", &innerAngle, 0.1f, 88.0f, "%.1f deg");
			const bool outerAngleChanged = ImGui::SliderFloat("Outer Cone", &outerAngle, 0.2f, 89.0f, "%.1f deg");
			if (innerAngleChanged || outerAngleChanged)
				spotLight.SetAngles(innerAngle, outerAngle);
			if (ImGui::Button("Delete Spot Light"))
				spotLightToDelete = i;
		}
		ImGui::PopID();
	}
	if (spotLightToDelete >= 0 && spotLightToDelete < static_cast<int>(spotLights.size()))
	{
		spotLights.erase(spotLights.begin() + spotLightToDelete);
		if (selectedSpotLightIndex == spotLightToDelete)
			selectedSpotLightIndex = -1;
		else if (selectedSpotLightIndex > spotLightToDelete)
			--selectedSpotLightIndex;
	}
}
