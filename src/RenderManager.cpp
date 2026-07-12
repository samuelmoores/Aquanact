#include "RenderManager.h"
#include "Globals.h"
#include "GraphicsDevice.h"
#include "EngineCamera.h"
#include "Debug.h"
#include "EngineGUI.h"
#include "Window.h"
#include <iomanip>
#include <iostream>

void RenderManager::startUp(GraphicsDevice& device)
{
	m_device = &device;
	commands.clear();
}

void RenderManager::shutDown()
{
	commands.clear();
}

void RenderManager::Submit(const RenderCommand& command)
{
	commands.push_back(command);
}

void printMatrixRender(const aiMatrix4x4& m)
{
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "[ "
		<< std::setw(9) << m.a1 << " "
		<< std::setw(9) << m.a2 << " "
		<< std::setw(9) << m.a3 << " "
		<< std::setw(9) << m.a4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.b1 << " "
		<< std::setw(9) << m.b2 << " "
		<< std::setw(9) << m.b3 << " "
		<< std::setw(9) << m.b4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.c1 << " "
		<< std::setw(9) << m.c2 << " "
		<< std::setw(9) << m.c3 << " "
		<< std::setw(9) << m.c4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.d1 << " "
		<< std::setw(9) << m.d2 << " "
		<< std::setw(9) << m.d3 << " "
		<< std::setw(9) << m.d4 << " ]\n";
	std::cout << "----------------------------------------------\n";
}

void RenderManager::Flush(const Camera& camera)
{
	for (const RenderCommand& command : commands) {
		m_device->Draw(command, camera);
	}

	commands.clear();
}

void RenderManager::Loop()
{
	m_device->Clear(0.0f, 0.0f, 0.0f, 0.0f);
	m_device->BeginFrame();
	Flush(gEngineCamera);
	gEngineGUI.BeginFrame();
	gDebug.draw(gEngineCamera, gEngineGUI);
	gEngineGUI.Draw(gEngineCamera, gFileManager);
	gEngineGUI.EndFrame();
	m_device->EndFrame();
	gWindow.PollEvents();
}
