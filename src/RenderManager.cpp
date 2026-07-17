#include "RenderManager.h"
#include "FrontEndManager.h"
#include "Globals.h"
#include "Debug.h"
#include "EngineCamera.h"
#include "GameCamera.h"
#include "Window.h"
#include "Object3D.h"
#include "SceneManager.h"
#include "Input.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <algorithm>

void RenderManager::startUp(Window& window)
{
	m_frameAllocator.ResetCapacity(1024 * 1024);
	m_commands = nullptr;
	m_commandCapacity = 0;
	m_commandCount = 0;
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
	m_lastFrameBuildTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameFlushTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameDebugOverlayTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameEditorGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
	if (!m_engineCamera)
	{
		m_engineCamera = std::make_unique<EngineCamera>();
	}
	if (!m_gameCamera)
	{
		m_gameCamera = std::make_unique<GameCamera>();
	}
	m_engineCamera->startUp();
	m_gameCamera->startUp();
	m_activeCamera = m_engineCamera.get();
	m_device.startUp(window);
}

RenderManager::~RenderManager()
{
	shutDown();
}

void RenderManager::shutDown()
{
	m_device.shutDown();
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
	m_commands = nullptr;
	m_commandCapacity = 0;
	m_commandCount = 0;
	m_frameAllocator.Reset();
}

void RenderManager::Submit(const RenderCommand& command)
{
	if (m_commandCount >= m_commandCapacity)
	{
		// Keep track of how much capacity we had before the growth so we can copy
		// the already-submitted commands into the new buffer.
		const std::size_t previousCapacity = m_commandCapacity;

		// Grow exponentially to avoid resizing on every extra object. If this is the
		// first allocation, start with a small fixed minimum so tiny scenes still work.
		const std::size_t requiredCapacity = std::max<std::size_t>(previousCapacity == 0 ? 64 : previousCapacity * 2, m_commandCount + 1);

		// Preserve a pointer to the old buffer before replacing the arena contents.
		// The old commands are still valid for copying until the new buffer is filled.
		RenderCommand* oldCommands = m_commands;

		// Grow the arena if needed, but keep the allocations that were already made
		// this frame. That is what makes this a true bump allocator: we only move the
		// top of the arena forward and never compact individual allocations.
		m_frameAllocator.Reserve(requiredCapacity * sizeof(RenderCommand));

		// Allocate the new command buffer from the frame allocator and update the
		// tracked capacity so later Submit calls know how much room is available.
		m_commands = static_cast<RenderCommand*>(m_frameAllocator.AllocateArray<RenderCommand>(requiredCapacity));
		m_commandCapacity = requiredCapacity;

		// Copy all commands that were already submitted this frame into the new buffer.
		// The render loop builds commands incrementally, so growth must preserve prior work.
		for (std::size_t i = 0; i < m_commandCount; ++i)
		{
			m_commands[i] = oldCommands[i];
		}
	}

	// Store the next command in the current frame buffer and advance the write cursor.
	m_commands[m_commandCount++] = command;
}

void RenderManager::Flush(const Camera& camera)
{
	const auto flushStart = std::chrono::high_resolution_clock::now();
	m_lastFrameCommandCount = m_commandCount;
	for (std::size_t i = 0; i < m_commandCount; ++i) {
		const RenderCommand& command = m_commands[i];
		m_device.Draw(command, camera);
	}

	m_commandCount = 0;
	const auto flushEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameFlushTime = flushEnd - flushStart;
}

void RenderManager::Loop()
{
	// Reset per-frame counters and rebuild the command buffer from scratch.
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
	m_frameAllocator.Reset();
	m_commands = nullptr;
	if (m_commandCapacity > 0)
	{
		// Reuse the existing command capacity so we avoid reallocating every frame.
		m_commands = static_cast<RenderCommand*>(m_frameAllocator.Allocate(sizeof(RenderCommand) * m_commandCapacity, alignof(RenderCommand)));
	}
	if (!m_commands && m_commandCapacity != 0)
	{
		m_commandCapacity = 0;
	}
	const auto buildStart = std::chrono::high_resolution_clock::now();

	// Start a new editor frame on the active graphics device.
	// MyGUI uses fixed-function GL state for its overlay pass, so re-apply the
	// engine's baseline state here to prevent GUI rendering from leaking depth or
	// blend state into the next 3D frame.
	m_device.ConfigureDefaultState();
	m_device.Clear(0.0f, 0.0f, 0.0f, 0.0f);
	m_device.BeginFrame();

	// Build render commands from the current scene.
	for (const auto& object : gSceneManager.Objects())
	{
		if (!object || !object->GetMesh() || !object->GetShader())
		{
			++m_lastFrameSkippedObjects;
			continue;
		}

		Submit(RenderCommand{
			object->GetMesh(),
			object->GetShader(),
			object->BuildModelMatrix(),
			object->skinned()
		});
	}
	const auto buildEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameBuildTime = buildEnd - buildStart;

	// Submit the built commands using the currently selected camera.
	Flush(ActiveCamera());

	// Draw ImGui overlays after the 3D pass.
	if (gEngineState.IsEditorMode())
	{
		const auto debugStart = std::chrono::high_resolution_clock::now();
		gFrontEndManager.BeginFrame();
		// Draw editor overlays from the same camera as the scene so preview modes stay consistent.
		// Use active camera since the game camera can be used as a preview in editor mode
		gDebug.draw(ActiveCamera(), gFrontEndManager.EditorGUI());
		const auto debugEnd = std::chrono::high_resolution_clock::now();
		m_lastFrameDebugOverlayTime = debugEnd - debugStart;

		const auto editorGuiStart = std::chrono::high_resolution_clock::now();
		gFrontEndManager.Draw(*m_engineCamera, gFileManager, gSceneManager, gProjectManager);
		const auto editorGuiEnd = std::chrono::high_resolution_clock::now();
		m_lastFrameEditorGuiTime = editorGuiEnd - editorGuiStart;

		if (gFrontEndManager.FrontEndModeValue() == FrontEndMode::GameGUICreator)
		{
			m_lastFrameUiCreatorTime = m_lastFrameEditorGuiTime;
			m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
		}
		else
		{
			m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
			m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
		}
		gFrontEndManager.EndFrame();
	}
	else
	{
		const auto debugStart = std::chrono::high_resolution_clock::now();
		gFrontEndManager.BeginFrame();
		gDebug.drawGameModeInput(gInput);
		// The runtime manager keeps the first placed asset active, so the render
		// loop only needs to draw the already-selected GameGUI instance.
		gFrontEndManager.RuntimeGUI().Draw();
		gFrontEndManager.RuntimeGUI().DrawDiagnosticsWindow();
		const auto debugEnd = std::chrono::high_resolution_clock::now();
		m_lastFrameDebugOverlayTime = debugEnd - debugStart;
		m_lastFrameEditorGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
		m_lastFrameUiCreatorTime = std::chrono::duration<double, std::milli>{ 0.0 };
		m_lastFrameRuntimeGuiTime = std::chrono::duration<double, std::milli>{ 0.0 };
		gFrontEndManager.EndFrame();
	}

	// Present the frame and process window events.
	m_device.EndFrame();
	gWindow.PollEvents();
}

std::size_t RenderManager::LastFrameCommandCount() const
{
	return m_lastFrameCommandCount;
}

std::size_t RenderManager::LastFrameSkippedObjects() const
{
	return m_lastFrameSkippedObjects;
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
