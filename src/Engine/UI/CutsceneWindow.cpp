#include "Engine/UI/CutsceneWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/CutsceneAnimator.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/Animator.h"
#include "Engine/Core/Root.h"

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

	void EnsureCutsceneAnimators(Scene& scene)
	{
		for (const auto& object : scene.Objects())
		{
			if (object && object->GetMesh() && object->GetMesh()->Skinned())
			{
				object->RemoveComponent<EntityStateMachine>();
				if (!object->GetCutsceneAnimator())
					object->AddComponent<CutsceneAnimator>(object->GetMesh());
			}
		}
	}

	void EvaluateTimeline(Scene& scene, float time)
	{
		EnsureCutsceneAnimators(scene);
		const float duration = std::max(0.1f, scene.Cutscene().duration);
	scene.CameraSystem().SetPlayerProgress(CameraPathProgressAtTime(scene.CameraSystem().Path(), time, duration));
	scene.CameraSystem().SnapToPath();
		for (CutsceneAnimationTrack& track : scene.Cutscene().animationTracks)
		{
			if (time < track.startTime)
				continue;
			Entity* entity = FindEntity(scene, track.entityId);
			CutsceneAnimator* cutsceneAnimator = entity ? entity->GetCutsceneAnimator() : nullptr;
			Animator* animator = cutsceneAnimator ? cutsceneAnimator->GetAnimator() : nullptr;
			if (!animator || !cutsceneAnimator)
				continue;
			const int clipIndex = cutsceneAnimator->FindAnimationIndex(track.animationName);
			if (clipIndex < 0)
				continue;
			track.animationName = cutsceneAnimator->AnimationNames()[static_cast<std::size_t>(clipIndex)];
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
	{
		if (Root::HasCurrent()) Root::Current().SetCutscenePreviewCamera(false);
		return;
	}

	Scene* scene = scenes.ActiveLevel();
	if (!scene || scenes.SceneKindFor(scene->Name()) != SceneManager::SceneKind::Cutscene)
	{
		if (Root::HasCurrent()) Root::Current().SetCutscenePreviewCamera(false);
		return;
	}
	EnsureCutsceneAnimators(*scene);
	Root::Current().SetCutscenePreviewCamera(!m_engineCamera);

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
	if (ImGui::Checkbox("Engine Camera", &m_engineCamera))
		Root::Current().SetCutscenePreviewCamera(!m_engineCamera);
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
	ImGui::TextUnformatted("Camera Timing");
	const CameraPathData& cameraPath = scene->CameraSystem().Path();
	for (std::size_t index = 0; index < cameraPath.points.size(); ++index)
	{
		const std::string label = "Camera Point " + std::to_string(index + 1);
		if (ImGui::Selectable(label.c_str(), m_selectedCameraPoint == static_cast<int>(index)))
			m_selectedCameraPoint = static_cast<int>(index);
	}
	if (m_selectedCameraPoint >= 0 && m_selectedCameraPoint < static_cast<int>(cameraPath.points.size()))
	{
		CameraPathPoint& point = scene->CameraSystem().MutablePath().points[static_cast<std::size_t>(m_selectedCameraPoint)];
		float pointTime = point.timeSeconds >= 0.0f
			? point.timeSeconds
			: duration * static_cast<float>(m_selectedCameraPoint) / static_cast<float>(std::max<std::size_t>(cameraPath.points.size() - 1, 1));
		if (ImGui::DragFloat("Point Time", &pointTime, 0.05f, 0.0f, duration))
		{
			point.timeSeconds = pointTime;
			changed = true;
		}
		const char* interpolationLabel = point.interpolation == CameraPathInterpolation::Linear ? "Linear" : point.interpolation == CameraPathInterpolation::Hold ? "Hold" : "Smooth";
		if (ImGui::BeginCombo("Movement", interpolationLabel))
		{
			const CameraPathInterpolation modes[] = { CameraPathInterpolation::Smooth, CameraPathInterpolation::Linear, CameraPathInterpolation::Hold };
			for (CameraPathInterpolation mode : modes)
			{
				const char* label = mode == CameraPathInterpolation::Linear ? "Linear" : mode == CameraPathInterpolation::Hold ? "Hold" : "Smooth";
				const bool selected = point.interpolation == mode;
				if (ImGui::Selectable(label, selected))
				{
					point.interpolation = mode;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Add Animation Track"))
	{
		for (const auto& object : scene->Objects())
		{
			if (object && object->GetCutsceneAnimator() && !object->GetCutsceneAnimator()->AnimationNames().empty())
			{
				CutsceneAnimationTrack track;
				track.entityId = object->Id();
				track.animationName = object->GetCutsceneAnimator()->AnimationNames().front();
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
					if (!candidate || !candidate->GetCutsceneAnimator() || candidate->GetCutsceneAnimator()->AnimationNames().empty()) continue;
					const bool selected = candidate->Id() == track.entityId;
					if (ImGui::Selectable(candidate->Name().c_str(), selected))
					{
						track.entityId = candidate->Id();
						track.animationName = candidate->GetCutsceneAnimator()->AnimationNames().front();
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
		}
		CutsceneAnimator* machine = entity ? entity->GetCutsceneAnimator() : nullptr;
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
		if (ImGui::Button("Delete Track"))
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
