#include "Engine/UI/CutsceneWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/Animator.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
	Entity* FindEntity(Scene& scene, unsigned int id)
	{
		for (const auto& object : scene.Objects())
		{
			if (object && object->Id() == id)
				return object.get();
		}
		return nullptr;
	}

	int AnimationIndex(const EntityStateMachine& machine, const std::string& name)
	{
		const auto& names = machine.AnimationNames();
		for (std::size_t i = 0; i < names.size(); ++i)
		{
			if (names[i] == name)
				return static_cast<int>(i);
		}
		return -1;
	}

	void EvaluateTimeline(Scene& scene, float time)
	{
		const float duration = std::max(0.1f, scene.Cutscene().duration);
		scene.CameraSystem().SetPlayerProgress(std::clamp(time / duration, 0.0f, 1.0f));
		for (const CutsceneAnimationTrack& track : scene.Cutscene().animationTracks)
		{
			if (time < track.startTime)
				continue;
			Entity* entity = FindEntity(scene, track.entityId);
			EntityStateMachine* stateMachine = entity ? entity->GetEntityState() : nullptr;
			Animator* animator = stateMachine ? stateMachine->GetAnimator() : nullptr;
			if (!animator || !stateMachine)
				continue;
			const int clipIndex = AnimationIndex(*stateMachine, track.animationName);
			if (clipIndex < 0)
				continue;
			const float localTime = std::max(0.0f, time - track.startTime) * std::max(0.01f, track.speed);
			const bool active = localTime <= track.duration;
			animator->EvaluateClipAt(clipIndex, active ? localTime : track.duration, track.loop && active);
		}
	}

	void Save(ProjectManager& projects, SceneManager& scenes)
	{
		if (!projects.CurrentProjectPath().empty())
			projects.SaveProject(projects.CurrentProjectPath(), scenes);
	}
}

void CutsceneWindow::Draw(SceneManager& scenes, ProjectManager& projects, bool& open)
{
	if (!open)
		return;

	Scene* scene = scenes.ActiveLevel();
	if (!scene || scenes.SceneKindFor(scene->Name()) != SceneManager::SceneKind::Cutscene)
		return;

	if (m_time > scene->Cutscene().duration)
		m_time = scene->Cutscene().duration;
	if (m_playing)
	{
		m_time += ImGui::GetIO().DeltaTime;
		if (m_time >= scene->Cutscene().duration)
		{
			m_time = scene->Cutscene().duration;
			m_playing = false;
		}
	}
	EvaluateTimeline(*scene, m_time);

	ImGui::Begin("Cutscene Timeline", &open);
	ImGui::Text("Scene: %s", scene->Name().c_str());
	bool changed = false;
	float duration = std::max(0.1f, scene->Cutscene().duration);
	if (ImGui::DragFloat("Duration", &duration, 0.05f, 0.1f, 3600.0f, "%.2f s"))
	{
		scene->Cutscene().duration = duration;
		m_time = std::min(m_time, duration);
		changed = true;
	}
	if (ImGui::Button(m_playing ? "Pause" : "Play"))
	{
		if (!m_playing && m_time >= duration)
			m_time = 0.0f;
		m_playing = !m_playing;
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		m_playing = false;
		m_time = 0.0f;
	}
	ImGui::SameLine();
	if (ImGui::SliderFloat("Time", &m_time, 0.0f, duration, "%.2f s"))
	{
		m_playing = false;
		EvaluateTimeline(*scene, m_time);
	}

	ImGui::Separator();
	if (ImGui::Button("Add Animation Track"))
	{
		for (const auto& object : scene->Objects())
		{
			if (object && object->GetEntityState() && !object->GetEntityState()->AnimationNames().empty())
			{
				CutsceneAnimationTrack track;
				track.entityId = object->Id();
				track.animationName = object->GetEntityState()->AnimationNames().front();
				track.duration = duration;
				scene->Cutscene().animationTracks.push_back(std::move(track));
				m_selectedTrack = static_cast<int>(scene->Cutscene().animationTracks.size()) - 1;
				changed = true;
				break;
			}
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Tracks: %zu", scene->Cutscene().animationTracks.size());

	for (std::size_t i = 0; i < scene->Cutscene().animationTracks.size(); ++i)
	{
		CutsceneAnimationTrack& track = scene->Cutscene().animationTracks[i];
		Entity* entity = FindEntity(*scene, track.entityId);
		const std::string label = "Track " + std::to_string(i + 1) + " - " + (entity ? entity->Name() : "Missing entity");
		if (ImGui::Selectable(label.c_str(), m_selectedTrack == static_cast<int>(i)))
			m_selectedTrack = static_cast<int>(i);
	}

	if (m_selectedTrack >= 0 && m_selectedTrack < static_cast<int>(scene->Cutscene().animationTracks.size()))
	{
		CutsceneAnimationTrack& track = scene->Cutscene().animationTracks[static_cast<std::size_t>(m_selectedTrack)];
		Entity* entity = FindEntity(*scene, track.entityId);
		if (entity)
		{
			if (ImGui::BeginCombo("Character", entity->Name().c_str()))
			{
				for (const auto& candidate : scene->Objects())
				{
					if (!candidate || !candidate->GetEntityState() || candidate->GetEntityState()->AnimationNames().empty()) continue;
					const bool selected = candidate->Id() == track.entityId;
					if (ImGui::Selectable(candidate->Name().c_str(), selected))
					{
						track.entityId = candidate->Id();
						track.animationName = candidate->GetEntityState()->AnimationNames().front();
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
		}
		EntityStateMachine* machine = entity ? entity->GetEntityState() : nullptr;
		if (machine && ImGui::BeginCombo("Animation", track.animationName.c_str()))
		{
			for (const std::string& name : machine->AnimationNames())
			{
				const bool selected = track.animationName == name;
				if (ImGui::Selectable(name.c_str(), selected))
				{
					track.animationName = name;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		changed |= ImGui::DragFloat("Start", &track.startTime, 0.05f, 0.0f, duration);
		changed |= ImGui::DragFloat("Length", &track.duration, 0.05f, 0.01f, 3600.0f);
		changed |= ImGui::DragFloat("Speed", &track.speed, 0.01f, 0.01f, 10.0f);
		changed |= ImGui::Checkbox("Loop", &track.loop);
		if (ImGui::Button("Remove Track"))
		{
			scene->Cutscene().animationTracks.erase(scene->Cutscene().animationTracks.begin() + m_selectedTrack);
			m_selectedTrack = -1;
			changed = true;
		}
	}
	ImGui::End();

	if (changed)
	{
		EvaluateTimeline(*scene, m_time);
		Save(projects, scenes);
	}
}
