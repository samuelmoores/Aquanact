#include "Engine/UI/ParticleSystemWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/ParticleSystem.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/WeatherSystem.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/RenderManager.h"

#include <imgui.h>

#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

void ParticleSystemWindow::Draw(SceneManager& sceneManager, unsigned int selectedEntityId, bool& open) const
{
	if (!open) return;
	if (!ImGui::Begin("Particle System", &open))
	{
		ImGui::End();
		return;
	}

	Scene* activeLevel = sceneManager.ActiveLevel();
	if (!activeLevel)
	{
		ImGui::TextDisabled("Open a scene to configure weather.");
		ImGui::End();
		return;
	}

	ImGui::SeparatorText("Scene Weather");
	WeatherSystem& weather = activeLevel->Weather();
	WeatherSettings settings = weather.Settings();
	bool settingsChanged = false;

	int weatherType = static_cast<int>(settings.type);
	const char* weatherTypes[] = {"Rain", "Snow"};
	if (ImGui::Combo("Type", &weatherType, weatherTypes, IM_ARRAYSIZE(weatherTypes)))
	{
		weather.ApplyPreset(static_cast<WeatherType>(weatherType));
		settings = weather.Settings();
	}
	settingsChanged |= ImGui::Checkbox("Enabled", &settings.enabled);

	ImGui::SeparatorText("Density and Motion");
	settingsChanged |= ImGui::SliderFloat(
		"Density", &settings.density, 0.0f, WeatherSystem::MaximumDensity, "%.2fx");
	settingsChanged |= ImGui::DragFloat(
		"Fall Speed", &settings.fallSpeed, 0.1f, 0.0f, 200.0f, "%.1f units/sec");
	settingsChanged |= ImGui::DragFloat2(
		"Wind Velocity (World X/Z)", &settings.windVelocity[0], 0.05f, -200.0f, 200.0f, "%.2f");
	settingsChanged |= ImGui::DragFloat(
		"Velocity Spread", &settings.velocitySpread, 0.01f, 0.0f, 1000.0f);
	settingsChanged |= ImGui::DragFloat3(
		"Gravity", &settings.gravity[0], 0.05f, -1000.0f, 1000.0f, "%.2f");

	ImGui::SeparatorText("Camera-Following World Volume");
	settingsChanged |= ImGui::DragFloat(
		"Spawn Padding Above View", &settings.spawnHeightAboveCamera,
		0.5f, 0.0f, 2000.0f, "%.1f units");
	settingsChanged |= ImGui::DragFloat(
		"Recycle Padding Below View", &settings.fallDistanceBelowCamera,
		0.5f, 0.0f, 2000.0f, "%.1f units");
	settingsChanged |= ImGui::DragFloat(
		"Coverage Depth", &settings.coverageDepth,
		1.0f, 1.0f, 2000.0f, "%.0f units");
	settingsChanged |= ImGui::DragFloat(
		"Minimum Near Width", &settings.minimumNearWidth,
		1.0f, 1.0f, 4000.0f, "%.0f units");
	ImGui::TextDisabled("Rain remains in world space; the camera frustum only controls off-screen replenishment.");

	ImGui::SeparatorText("Appearance");
	settingsChanged |= ImGui::DragFloat(
		"Start Size", &settings.startSize, 0.01f, 0.001f, 100.0f);
	settingsChanged |= ImGui::DragFloat(
		"End Size", &settings.endSize, 0.01f, 0.0f, 100.0f);
	ImGui::SeparatorText("Random Rain Splashes");
	settingsChanged |= ImGui::Checkbox("Enable Splashes", &settings.splashesEnabled);
	settingsChanged |= ImGui::DragFloat(
		"Splash Size", &settings.splashSize, 0.01f, 0.001f, 10.0f, "%.2f units");
	settingsChanged |= ImGui::DragFloat(
		"Splash Density", &settings.splashDensity, 0.5f, 0.0f, 1000.0f, "%.1f splashes/sec");
	settingsChanged |= ImGui::DragFloat(
		"Splash Brightness", &settings.splashBrightness, 0.01f, 0.0f, 5.0f, "%.2fx");
	settingsChanged |= ImGui::SliderFloat(
		"Splash Opacity", &settings.splashOpacity, 0.0f, 1.0f, "%.2f");
	const float previousSplashFrameDuration = settings.splashFrameDuration;
	settingsChanged |= ImGui::DragFloat(
		"Splash Frame Duration", &settings.splashFrameDuration, 0.05f, 0.05f, 10.0f, "%.2f sec/frame");
	const bool splashFrameDurationChanged = settings.splashFrameDuration != previousSplashFrameDuration;
	std::vector<std::string> splashFiles;
	std::error_code fileError;
	for (const auto& entry : std::filesystem::directory_iterator("spritesheets", fileError))
	{
		if (!entry.is_regular_file(fileError)) continue;
		const auto extension = entry.path().extension().string();
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
			splashFiles.push_back(entry.path().filename().string());
	}
	std::sort(splashFiles.begin(), splashFiles.end());
	if (!splashFiles.empty())
	{
		if (std::find(splashFiles.begin(), splashFiles.end(), settings.splashTexturePath) == splashFiles.end())
			settings.splashTexturePath = splashFiles.front();
		if (ImGui::BeginCombo("Splash Sprite Sheet", settings.splashTexturePath.c_str()))
		{
			for (const std::string& file : splashFiles)
			{
				const bool selected = file == settings.splashTexturePath;
				if (ImGui::Selectable(file.c_str(), selected))
				{
					if (Root::Current().Render().SetSplashTexture((std::filesystem::path("spritesheets") / file).string()))
					{
						settings.splashTexturePath = file;
						settingsChanged = true;
						weather.Restart();
					}
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
	else
	{
		ImGui::TextDisabled("No sprite sheets found in spritesheets.");
	}

	if (settings.type == WeatherType::Rain)
	{
		static unsigned int selectedGroundEntityId = 0;
		const char* selectedGroundEntityName = "Select entity...";
		for (const auto& object : activeLevel->Objects())
		{
			if (object && object->Id() == selectedGroundEntityId)
			{
				selectedGroundEntityName = object->Name().c_str();
				break;
			}
		}

		if (ImGui::BeginCombo("Ground Entity", selectedGroundEntityName))
		{
			for (const auto& object : activeLevel->Objects())
			{
				if (!object || !object->GetMesh()) continue;
				const bool selected = object->Id() == selectedGroundEntityId;
				if (ImGui::Selectable(object->Name().c_str(), selected))
					selectedGroundEntityId = object->Id();
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Ground Entity") && selectedGroundEntityId != 0 &&
			std::find(settings.groundEntityIds.begin(), settings.groundEntityIds.end(), selectedGroundEntityId) ==
				settings.groundEntityIds.end())
		{
			settings.groundEntityIds.push_back(selectedGroundEntityId);
			settingsChanged = true;
		}

		for (std::size_t index = 0; index < settings.groundEntityIds.size(); ++index)
		{
			const unsigned int entityId = settings.groundEntityIds[index];
			const Entity* groundEntity = nullptr;
			for (const auto& object : activeLevel->Objects())
				if (object && object->Id() == entityId) { groundEntity = object.get(); break; }
			if (!groundEntity) continue;
			ImGui::PushID(static_cast<int>(entityId));
			ImGui::BulletText("%s", groundEntity->Name().c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove"))
			{
				settings.groundEntityIds.erase(settings.groundEntityIds.begin() + static_cast<std::ptrdiff_t>(index));
				settingsChanged = true;
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::TextDisabled("Splashes spawn on the selected entities' world-space top surfaces.");
	}
	settingsChanged |= ImGui::ColorEdit4("Start Color", &settings.startColor[0]);
	settingsChanged |= ImGui::ColorEdit4("End Color", &settings.endColor[0]);

	if (settingsChanged)
		weather.SetSettings(settings);
	if (splashFrameDurationChanged)
		weather.Restart();
	if (ImGui::Button("Restart Weather"))
		weather.Restart();
	ImGui::SameLine();
	ImGui::TextDisabled("Live particles: %zu", weather.Particles().size());

	ImGui::SeparatorText("Entity Particle Effects");
	const ParticleSystem* selectedSystem = nullptr;
	std::size_t emitterCount = 0;
	std::size_t totalParticleCount = 0;
	std::size_t totalCapacity = 0;
	for (const auto& object : activeLevel->Objects())
	{
		if (!object) continue;
		const ParticleSystem* particles = object->GetComponent<ParticleSystem>();
		if (!particles) continue;
		++emitterCount;
		totalParticleCount += particles->Particles().size();
		totalCapacity += static_cast<std::size_t>(particles->MaxParticles());
		if (object->Id() == selectedEntityId) selectedSystem = particles;
	}
	ImGui::Text("Active emitters: %zu", emitterCount);
	ImGui::Text("Live particles: %zu / %zu", totalParticleCount, totalCapacity);
	if (!selectedSystem)
	{
		ImGui::TextDisabled("Select an entity with a ParticleSystem to inspect its coordinate effect.");
		ImGui::End();
		return;
	}

	ImGui::SeparatorText("Selected Emitter");
	ImGui::Text("Particles: %zu / %d", selectedSystem->Particles().size(), selectedSystem->MaxParticles());
	ImGui::Text("Emission rate: %.2f / sec", selectedSystem->EmissionRate());
	ImGui::Text("Lifetime: %.2f sec", selectedSystem->Lifetime());
	ImGui::Text("Size: %.2f -> %.2f", selectedSystem->ParticleSize(), selectedSystem->EndParticleSize());
	const char* shape = "Point";
	if (selectedSystem->EmissionShape() == ParticleEmissionShape::Sphere) shape = "Sphere";
	else if (selectedSystem->EmissionShape() == ParticleEmissionShape::Box) shape = "Box";
	ImGui::Text("Shape: %s", shape);
	const glm::vec3 initialVelocity = selectedSystem->InitialVelocity();
	const glm::vec3 gravity = selectedSystem->Gravity();
	ImGui::Text("Initial velocity: (%.2f, %.2f, %.2f)", initialVelocity.x, initialVelocity.y, initialVelocity.z);
	ImGui::Text("Velocity spread: %.2f", selectedSystem->VelocitySpread());
	ImGui::Text("Gravity: (%.2f, %.2f, %.2f)", gravity.x, gravity.y, gravity.z);
	ImGui::Text("Blend: %s", selectedSystem->BlendMode() == ParticleBlendMode::Additive ? "additive" : "alpha");
	ImGui::Text("Space: %s", selectedSystem->SimulationSpace() == ParticleSimulationSpace::Local ? "local" : "world");
	ImGui::Text("Looping: %s", selectedSystem->Looping() ? "yes" : "no");

	if (!selectedSystem->Particles().empty())
	{
		const ParticleInstance& particle = selectedSystem->Particles().front();
		ImGui::SeparatorText("First Live Particle");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", particle.position.x, particle.position.y, particle.position.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", particle.velocity.x, particle.velocity.y, particle.velocity.z);
		ImGui::Text("Age: %.2f / %.2f sec", particle.age, particle.lifetime);
		ImGui::Text("Color: (%.2f, %.2f, %.2f, %.2f)", particle.color.r, particle.color.g, particle.color.b, particle.color.a);
	}
	else
	{
		ImGui::TextDisabled("No live particles. Try Burst in the Entity window.");
	}

	ImGui::End();
}
