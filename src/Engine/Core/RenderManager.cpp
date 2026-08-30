#include "Engine/Core/RenderManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateData.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Frustum.h"
#include "Game/PlayerController.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace
{
	bool CameraPathsEqual(const CameraPathData& left, const CameraPathData& right)
	{
		if (left.points.size() != right.points.size()) return false;
		for (std::size_t i = 0; i < left.points.size(); ++i)
		{
			const glm::vec3 delta = left.points[i].position - right.points[i].position;
			if (glm::dot(delta, delta) > 1e-8f) return false;
		}
		return true;
	}
}
#include <filesystem>
#include <memory>
#include <vector>

namespace
{
}

void RenderManager::startUp(Window& window)
{
	m_frameAllocator.ResetCapacity(1024 * 1024);
	m_commands = nullptr;

	//Debug
	m_commandCapacity = 0;
	m_commandCount = 0;
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
	m_lastFrameFrustumCulledObjects = 0;
	m_lastFrameDrawCallsSaved = 0;
	m_lastFrameBuildTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameFlushTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameDebugOverlayTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameEditorGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };

	//Cameras
	if (!m_gameCamera)
	{
		m_gameCamera = std::make_unique<PathedCamera>();
		m_gameCamera->startUp();
	}

	if (!m_engineCamera)
	{
		m_engineCamera = std::make_unique<EngineCamera>();
	}
	m_engineCamera->startUp();
	m_cameraManager.startUp(*m_engineCamera, *m_gameCamera);
	if (Root::Current().State().IsGameMode())
	{
		m_cameraManager.SetGameMode(*m_gameCamera);
	}

	//Graphics Device
	m_device.startUp(window);

	//Lighting
	if (!m_lightingManager)
	{
		m_lightingManager = std::make_unique<LightingManager>();
	}
	m_lightingManager->startUp();
}

EngineCamera& RenderManager::GetEngineCamera()
{
	return *m_engineCamera;
}

const EngineCamera& RenderManager::GetEngineCamera() const
{
	return *m_engineCamera;
}

PathedCamera& RenderManager::GetPathedCamera()
{
	return *m_gameCamera;
}

const PathedCamera& RenderManager::GetPathedCamera() const
{
	return *m_gameCamera;
}

void RenderManager::SetEditorMode()
{
	m_cameraManager.SetEditorMode(*m_engineCamera);
}

void RenderManager::SetGameMode()
{
	m_cameraManager.SetGameMode(*m_gameCamera);
}

void RenderManager::SetCameraMode(CameraMode mode)
{
	m_cameraMode = mode;
}

void RenderManager::SetActiveCamera(Camera& camera)
{
	m_cameraManager.SetActiveCamera(camera);
}

void RenderManager::ClearPathedCameraTarget()
{
	m_cameraTarget = nullptr;
	m_cameraLastTargetPosition = glm::vec3(0.0f);
	m_cameraPlayerProgress = 0.0f;
	m_hasCameraTargetPosition = false;
	if (m_gameCamera)
	{
		m_gameCamera->SetTarget(nullptr);
	}
}

RenderManager::~RenderManager()
{
	shutDown();
}

void RenderManager::shutDown()
{
	//-----Reverse order from startUp------

	//Lighting
	if (m_lightingManager)
	{
		m_lightingManager->shutDown();
		m_lightingManager.reset();
	}

	if (m_gameCamera)
	{
	}

	//Graphics
	m_device.shutDown();

	//Camera
	if (m_engineCamera)
	{
		m_engineCamera->shutDown();
		m_engineCamera.reset();
	}
	if (m_gameCamera)
	{
		m_gameCamera->shutDown();
		m_gameCamera.reset();
	}
	m_cameraManager.shutDown();
	m_commands = nullptr;
	m_commandCapacity = 0;
	m_commandCount = 0;
	m_frameAllocator.Reset();
}

void RenderManager::ApplyProjectState(const ProjectStateData::RenderStateData& renderState)
{
	m_engineCamera->SetMoveSpeed(renderState.engineCameraMoveSpeed);
	m_engineCamera->SetLookSensitivity(renderState.engineCameraLookSensitivity);
	m_gameCamera->SetPose(renderState.gameCameraPosition, renderState.gameCameraFacing);
	m_gameCamera->SetPath(renderState.cameraPath);
	m_engineCameraPathInitialized = false;
	m_gameCamera->SetFollowSharpness(renderState.pathedCameraFollowSharpness);
	// Curve sampling is intentionally fixed for predictable camera behavior.
	m_gameCamera->SetPathSamplesPerSegment(16);
	Root::Current().FrontEnd().EditorGUI().CameraPath().Data() = renderState.cameraPath;
	Root::Current().FrontEnd().EditorGUI().SetShowCameraPath(renderState.showCameraPath);
	if (!renderState.cameraPath.points.empty())
	{
		glm::vec3 facing = m_engineCamera->GetFacing();
		if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
		{
			for (const auto& object : scene->Objects())
			{
				if (object && object->GetComponent<PlayerController>())
				{
					const glm::vec3 towardPlayer = object->WorldCenterPosition() - renderState.cameraPath.points.front().position;
					if (glm::dot(towardPlayer, towardPlayer) > 1e-8f)
						facing = glm::normalize(towardPlayer);
					break;
				}
			}
		}
		m_engineCamera->SetPose(renderState.cameraPath.points.front().position, facing);
	}
	m_lightingManager->SunLight().direction = renderState.sunLight.direction;
	m_lightingManager->SunLight().color = renderState.sunLight.color;
	m_lightingManager->SunLight().intensity = renderState.sunLight.intensity;
	m_lightingManager->SunLight().ambient = renderState.sunLight.ambient;
	m_lightingManager->SunLight().castsShadows = renderState.sunLight.castsShadows;
	m_lightingManager->SetShadowsEnabled(renderState.sunLight.shadowsEnabled);
	m_lightingManager->PointLights().clear();
	for (const auto& pointLightData : renderState.pointLights)
	{
		PointLight& pointLight = m_lightingManager->AddPointLight();
		pointLight.position = pointLightData.position;
		pointLight.color = pointLightData.color;
		pointLight.intensity = pointLightData.intensity;
		pointLight.ambient = pointLightData.ambient;
		pointLight.SetRadius(pointLightData.radius);
		pointLight.radiusFade = pointLightData.radiusFade;
		pointLight.constant = pointLightData.constant;
		pointLight.linear = pointLightData.linear;
		pointLight.quadratic = pointLightData.quadratic;
		pointLight.castsShadows = pointLightData.castsShadows;
	}
}

void RenderManager::ApplyCameraMode(const EngineState& engineState)
{
	if (engineState.IsGameMode() ||
		(engineState.IsEditorMode() && Root::Current().FrontEnd().FrontEndModeValue() == FrontEndMode::GameGUICreator))
	{
		// The editor owns the authored path. Synchronize it when entering the
		// pathed-camera mode so point edits made after project load are used by
		// gameplay without resetting the camera every frame.
		const CameraPathData& authoredPath = Root::Current().FrontEnd().EditorGUI().CameraPath().Data();
		if (!CameraPathsEqual(authoredPath, m_gameCamera->Path()))
			m_gameCamera->SetPath(authoredPath);
		m_cameraManager.SetGameMode(*m_gameCamera);
	}
	else
	{
		m_cameraManager.SetEditorMode(*m_engineCamera);
	}
}

void RenderManager::BeginFrame()
{
	m_device.ConfigureDefaultState();
	m_device.Clear(0.0f, 0.0f, 0.0f, 0.0f);
	m_device.BeginFrame();
}

void RenderManager::PresentFrame(Window& window)
{
	(void)window;
	m_device.EndFrame();
}

void RenderManager::UpdateCameraPhase(const Input& input, const EngineState& engineState)
{
	ApplyCameraMode(engineState);
	if (engineState.IsGameMode() &&
		Root::Current().Gameplay().State() ==
			GameplayManager::GameState::Playing)
	{
		Entity* target = nullptr;
		if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
		{
			for (const auto& object : scene->Objects())
			{
				if (object && object->GetComponent<PlayerController>())
				{
					target = object.get();
					break;
				}
			}
		}
		if (target != m_cameraTarget)
		{
			m_cameraTarget = target;
		}
		m_gameCamera->SetTarget(target);
		if (target)
		{
			const glm::vec3 position = target->WorldCenterPosition();
			m_cameraPlayerProgress = m_gameCamera->ClosestPathDistance(position);
			m_gameCamera->SetPlayerProgress(m_cameraPlayerProgress);
		}
		m_gameCamera->Update(input.Frame().deltaTime);
	}
	if (engineState.IsEditorMode())
	{
		if (!m_engineCameraPathInitialized)
		{
			const CameraPathData& path = m_gameCamera->Path();
			if (!path.points.empty())
			{
				glm::vec3 facing = m_engineCamera->GetFacing();
				if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
				{
					for (const auto& object : scene->Objects())
					{
						if (object && object->GetComponent<PlayerController>())
						{
							const glm::vec3 direction = object->WorldCenterPosition() - path.points.front().position;
							if (glm::dot(direction, direction) > 1e-8f)
							{
								facing = glm::normalize(direction);
							}
							break;
						}
					}
				}
				m_engineCamera->SetPose(path.points.front().position, facing);
				m_engineCameraPathInitialized = true;
			}
		}
		m_cameraManager.Update(input);
	}
}

void RenderManager::ResetFrameState()
{
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
	m_lastFrameFrustumCulledObjects = 0;
	m_lastFrameDrawCallsSaved = 0;
	m_frameAllocator.Reset();
	m_commands = nullptr;
	if (m_commandCapacity > 0)
	{
		m_commands = static_cast<RenderCommand*>(m_frameAllocator.Allocate(sizeof(RenderCommand) * m_commandCapacity, alignof(RenderCommand)));
	}
	if (!m_commands && m_commandCapacity != 0)
	{
		m_commandCapacity = 0;
	}
}

bool RenderManager::ShouldPreviewMainMenu(const FrontEndManager& frontEndManager, EngineState& engineState) const
{
	return engineState.IsEditorMode() &&
		frontEndManager.FrontEndModeValue() == FrontEndMode::GameGUICreator &&
		frontEndManager.Creator().IsMainMenuSelected();
}

void RenderManager::BuildRenderCommands(FrontEndManager& frontEndManager, SceneManager& SceneManager, EngineState& engineState)
{
	(void)frontEndManager;
	(void)engineState;
	if (ShouldPreviewMainMenu(frontEndManager, engineState))
	{
		return;
	}

	const Scene* activeLevel = SceneManager.ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyObjects;
	const auto& objects = activeLevel ? activeLevel->Objects() : emptyObjects;
	for (const auto& object : objects)
	{
		if (!object || !object->GetMesh() || !object->GetShader())
		{
			++m_lastFrameSkippedObjects;
			continue;
		}

		glm::vec3 worldBoundsMin(0.0f);
		glm::vec3 worldBoundsMax(0.0f);
		const bool hasWorldBounds = object->WorldAABB(worldBoundsMin, worldBoundsMax);
		Submit(RenderCommand{ object->GetMesh(), object->GetShader(),
			object->BuildModelMatrix(), object->skinned(), object->Id(),
			worldBoundsMin, worldBoundsMax, hasWorldBounds, -1 });
	}
}

void RenderManager::DrawEditorFrame(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug)
{
	frontEndManager.BeginFrame();
	const auto debugStart = std::chrono::high_resolution_clock::now();
	debug.draw(ActiveCamera(), frontEndManager.EditorGUI());
	const auto debugEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameDebugOverlayTime = debugEnd - debugStart;

	const auto editorGuiStart = std::chrono::high_resolution_clock::now();
	frontEndManager.DrawEngineGUI(*m_engineCamera, fileManager, SceneManager, projectManager);
	frontEndManager.DrawCreatorGUI(*m_engineCamera);
	if (frontEndManager.FrontEndModeValue() == FrontEndMode::GameGUICreator)
	{
		frontEndManager.DrawRuntimePreviewGUI();
	}
	const auto editorGuiEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameEditorGuiTime = editorGuiEnd - editorGuiStart;

	if (frontEndManager.FrontEndModeValue() == FrontEndMode::GameGUICreator)
	{
		m_lastFrameUiCreatorTime = m_lastFrameEditorGuiTime;
		m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	}
	else
	{
		m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
		m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	}
	frontEndManager.EndFrame();
}

void RenderManager::DrawRuntimeFrame(FrontEndManager& frontEndManager, Debug& debug, Input& input)
{
	frontEndManager.BeginFrame();
	const auto debugStart = std::chrono::high_resolution_clock::now();
	if (Root::Current().GameModeDebugFlag())
	{
		debug.DrawPhysicsBoundingVolumes(ActiveCamera());
		debug.drawGameModeInput(input);
	}
	frontEndManager.DrawRuntimeGUI();
	const auto debugEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameDebugOverlayTime = debugEnd - debugStart;
	m_lastFrameEditorGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	frontEndManager.EndFrame();
}

void RenderManager::DrawFrame(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug, Input& input, EngineState& engineState)
{
	if (engineState.IsEditorMode())
	{
		DrawEditorFrame(frontEndManager, fileManager, SceneManager, projectManager, debug);
		return;
	}

	DrawRuntimeFrame(frontEndManager, debug, input);
}

Camera& RenderManager::ActiveCamera()
{
	return m_cameraManager.ActiveCamera();
}

const Camera& RenderManager::ActiveCamera() const
{
	return m_cameraManager.ActiveCamera();
}

void RenderManager::Submit(const RenderCommand& command)
{
	if (m_commandCount >= m_commandCapacity)
	{
		// Keep track of how much capacity we had before the growth so we can copy
		// the already-submitted commands into the new buffer.
		const std::size_t previousCapacity = m_commandCapacity;

		// Grow exponentially to avoid resizing on every extra object. If this is the
		// first allocation, start with a small fixed minimum so tiny levels still work.
		const std::size_t requiredCapacity = std::max<std::size_t>(previousCapacity == 0 ? 64 : previousCapacity * 2, m_commandCount + 1);

		// Reserve() may delete the old arena buffer, so copy the live commands before
		// growing it. Reading m_commands after Reserve() would be a use-after-free.
		std::vector<RenderCommand> previousCommands;
		previousCommands.reserve(m_commandCount);
		for (std::size_t i = 0; i < m_commandCount; ++i)
		{
			previousCommands.push_back(m_commands[i]);
		}

		m_frameAllocator.Reserve(requiredCapacity * sizeof(RenderCommand));

		// Allocate the new command buffer from the frame allocator and update the
		// tracked capacity so later Submit calls know how much room is available.
		m_commands = static_cast<RenderCommand*>(m_frameAllocator.AllocateArray<RenderCommand>(requiredCapacity));
		m_commandCapacity = requiredCapacity;

		// Copy all commands that were already submitted this frame into the new buffer.
		// The render loop builds commands incrementally, so growth must preserve prior work.
		for (std::size_t i = 0; i < m_commandCount; ++i)
		{
			m_commands[i] = previousCommands[i];
		}
	}

	// Store the next command in the current frame buffer and advance the write cursor.
	m_commands[m_commandCount++] = command;
}

void RenderManager::Flush(const Camera& camera, unsigned int /*selectedEntityId*/)
{
	const auto flushStart = std::chrono::high_resolution_clock::now();
	m_lastFrameCommandCount = 0;

	for (std::size_t i = 0; i < m_commandCount; ++i)
	{
		m_lastFrameCommandCount += static_cast<std::size_t>(
			std::max(m_commands[i].mesh->NumBuffers(), 0));
	}

	m_device.RenderShadowMaps(m_commands, m_commandCount, *m_lightingManager);

	const Frustum frustum = Frustum::FromViewProjection(
		camera.GetProjectionMatrix() * camera.GetViewMatrix());

	for (std::size_t i = 0; i < m_commandCount; ++i) 
	{
		const RenderCommand& command = m_commands[i];

		// Commands without trustworthy bounds remain visible. Shadow maps receive
		// the full list above because off-camera entities can cast visible shadows.
		if (command.hasWorldBounds &&
			!frustum.IntersectsAabb(command.worldBoundsMin, command.worldBoundsMax))
		{
			const std::size_t subMeshCount = static_cast<std::size_t>(
				std::max(command.mesh->NumBuffers(), 0));

			m_lastFrameFrustumCulledObjects += subMeshCount;
			m_lastFrameDrawCallsSaved += subMeshCount;

			continue;
		}

		if (command.isSkinned)
		{
			m_device.Draw(command, camera, *m_lightingManager);
		}
		else
		{
			std::size_t visibleSubMeshes = 0;
			std::size_t culledSubMeshes = 0;

			m_device.DrawCulled(command, camera, *m_lightingManager, frustum,
				visibleSubMeshes, culledSubMeshes);

			m_lastFrameFrustumCulledObjects += culledSubMeshes;
			m_lastFrameDrawCallsSaved += culledSubMeshes;
		}
	}

	m_commandCount = 0;
	const auto flushEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameFlushTime = flushEnd - flushStart;
}

void RenderManager::Loop(FrontEndManager& frontEndManager, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager, Debug& debug, Input& input, Window& window, EngineState& engineState)
{
	ResetFrameState();
	const auto buildStart = std::chrono::high_resolution_clock::now();
	BeginFrame();

	// Editor scenes do not run gameplay components, but their selected initial
	// animation still needs to advance. Previewing only the animator keeps Griff
	// in Idle without applying gravity, input, or state transitions in the editor.
	if (engineState.IsEditorMode() && !ShouldPreviewMainMenu(frontEndManager, engineState))
	{
		bool animationDiagnosticsPublished = false;
		if (Scene* activeLevel = SceneManager.ActiveLevel())
		{
			for (const std::unique_ptr<Entity>& object : activeLevel->Objects())
			{
				if (object)
				{
					if (EntityStateMachine* stateMachine = object->GetEntityState())
					{
						if (animationDiagnosticsPublished || stateMachine->States().empty() || stateMachine->CurrentState().empty())
							continue;
						stateMachine->UpdateEditorPreview(input.Frame().deltaTime);
						std::string stateListText;
						for (const auto& state : stateMachine->States())
						{
							const std::string animationLabel = state.animationName.empty()
								? "<none>"
								: std::filesystem::path(state.animationName).filename().string();
							stateListText += state.name + " -> animation " + animationLabel + "\n";
						}
						Root::Current().Debugger().SetAnimationDiagnostics(
							stateMachine->CurrentState(),
							stateMachine->DesiredState(),
							stateMachine->LastTransitionDebug(),
							stateMachine->LastTransitionFrom(),
							stateMachine->LastTransitionTo(),
							stateMachine->LastTransitionLeftOperandText(),
							stateMachine->LastTransitionComparatorText(),
							stateMachine->LastTransitionRightOperandText(),
							stateMachine->LastTransitionLeftValue(),
							stateMachine->LastTransitionRightValue(),
							stateMachine->LastTransitionPassed(),
							stateMachine->LastResolvedTargetState(),
							stateMachine->LastResolvedTargetClipIndex(),
							stateMachine->LastResolvedTargetFound(),
							stateListText);
						animationDiagnosticsPublished = true;
					}
				}
			}
		}
	}

	{
		FrameProfiler::Scope scope(Root::Current().Profiler(), "RenderCommands");
		BuildRenderCommands(frontEndManager, SceneManager, engineState);
	}
	const auto buildEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameBuildTime = buildEnd - buildStart;

	{
		FrameProfiler::Scope scope(Root::Current().Profiler(), "Camera");
		UpdateCameraPhase(input, engineState);
	}
	{
		FrameProfiler::Scope scope(Root::Current().Profiler(), "Flush");
	unsigned int selectedEntityId = 0;
	if (engineState.IsEditorMode() && frontEndManager.FrontEndModeValue() == FrontEndMode::EngineEditor)
		selectedEntityId = frontEndManager.SelectedEditorEntityId();
	Flush(ActiveCamera(), selectedEntityId);
	}

	{
		FrameProfiler::Scope scope(Root::Current().Profiler(), "Frontend");
		DrawFrame(frontEndManager, fileManager, SceneManager, projectManager, debug, input, engineState);
	}

	{
		FrameProfiler::Scope scope(Root::Current().Profiler(), "Present");
		PresentFrame(window);
	}
}

std::size_t RenderManager::LastFrameCommandCount() const
{
	return m_lastFrameCommandCount;
}

std::size_t RenderManager::LastFrameSkippedObjects() const
{
	return m_lastFrameSkippedObjects;
}

std::size_t RenderManager::LastFrameFrustumCulledObjects() const
{
	return m_lastFrameFrustumCulledObjects;
}

std::size_t RenderManager::LastFrameDrawCallsSaved() const
{
	return m_lastFrameDrawCallsSaved;
}

double RenderManager::LastFrameBuildMs() const
{
	return m_lastFrameBuildTime.count();
}

double RenderManager::LastFrameFlushMs() const
{
	return m_lastFrameFlushTime.count();
}

double RenderManager::LastFrameDebugOverlayMs() const
{
	return m_lastFrameDebugOverlayTime.count();
}

double RenderManager::LastFrameEditorGuiMs() const
{
	return m_lastFrameEditorGuiTime.count();
}

double RenderManager::LastFrameUiCreatorMs() const
{
	return m_lastFrameUiCreatorTime.count();
}

double RenderManager::LastFrameRuntimeGuiMs() const
{
	return m_lastFrameRuntimeGuiTime.count();
}

std::size_t RenderManager::FrameAllocatorCapacityBytes() const
{
	return m_frameAllocator.CapacityBytes();
}

std::size_t RenderManager::FrameAllocatorUsedBytes() const
{
	return m_frameAllocator.UsedBytes();
}

std::size_t RenderManager::FrameAllocatorPeakBytes() const
{
	return m_frameAllocator.PeakBytes();
}





