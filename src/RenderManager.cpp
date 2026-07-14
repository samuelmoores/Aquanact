#include "RenderManager.h"
#include "Globals.h"
#include "GraphicsDevice.h"
#include "EngineCamera.h"
#include "Debug.h"
#include "EngineGUI.h"
#include "Window.h"
#include "Object3D.h"
#include "SceneManager.h"
#include "Input.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <algorithm>

void RenderManager::startUp(GraphicsDevice& device)
{
	m_device = &device;
	m_frameAllocator.ResetCapacity(1024 * 1024);
	m_commands = nullptr;
	m_commandCapacity = 0;
	m_commandCount = 0;
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
	m_lastFrameBuildTime = std::chrono::duration<double, std::milli>{ 0.0 };
	m_lastFrameFlushTime = std::chrono::duration<double, std::milli>{ 0.0 };
}

void RenderManager::shutDown()
{
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
		m_device->Draw(command, camera);
	}

	m_commandCount = 0;
	const auto flushEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameFlushTime = flushEnd - flushStart;
}

void RenderManager::Loop()
{
	m_lastFrameCommandCount = 0;
	m_lastFrameSkippedObjects = 0;
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
	const auto buildStart = std::chrono::high_resolution_clock::now();
	m_device->Clear(0.0f, 0.0f, 0.0f, 0.0f);
	m_device->BeginFrame();
	for (const auto& object : gSceneManager.Objects())
	{
		if (!object || !object->GetMesh() || !object->GetShader())
		{
			++m_lastFrameSkippedObjects;
			continue;
		}

		object->UpdateComponents(gInput.DeltaTime());

		Submit(RenderCommand{
			object->GetMesh(),
			object->GetShader(),
			object->BuildModelMatrix(),
			object->skinned()
		});
	}
	const auto buildEnd = std::chrono::high_resolution_clock::now();
	m_lastFrameBuildTime = buildEnd - buildStart;
	Flush(gEngineCamera);
	gEngineGUI.BeginFrame();
	gDebug.draw(gEngineCamera, gEngineGUI);
	gEngineGUI.Draw(gEngineCamera, gFileManager, gSceneManager, gProjectManager);
	gEngineGUI.EndFrame();
	m_device->EndFrame();
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
