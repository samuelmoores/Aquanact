#include "FrontEndManager.h"

#include "EngineGUI.h"
#include "Window.h"
#include "Camera.h"
#include "FileManager.h"
#include "SceneManager.h"
#include "ProjectManager.h"

FrontEndManager::~FrontEndManager()
{
	shutDown();
}

void FrontEndManager::startUp(Window& window)
{
	if (!m_engineGUI)
	{
		m_engineGUI = std::make_unique<EngineGUI>();
	}

	m_engineGUI->startUp(window);
}

void FrontEndManager::shutDown()
{
	if (m_engineGUI)
	{
		m_engineGUI->shutDown();
		m_engineGUI.reset();
	}
}

void FrontEndManager::BeginFrame()
{
	if (m_mode == FrontEndMode::Editor && m_engineGUI)
	{
		m_engineGUI->BeginFrame();
	}
}

void FrontEndManager::Draw(const Camera& camera, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager)
{
	if (m_mode == FrontEndMode::Editor && m_engineGUI)
	{
		m_engineGUI->Draw(camera, fileManager, sceneManager, projectManager);
	}
}

void FrontEndManager::EndFrame()
{
	if (m_mode == FrontEndMode::Editor && m_engineGUI)
	{
		m_engineGUI->EndFrame();
	}
}

void FrontEndManager::SetMode(FrontEndMode mode)
{
	m_mode = mode;
}

FrontEndMode FrontEndManager::Mode() const
{
	return m_mode;
}

bool FrontEndManager::IsEditorMode() const
{
	return m_mode == FrontEndMode::Editor;
}

bool FrontEndManager::IsGameMode() const
{
	return m_mode == FrontEndMode::Game;
}

EngineGUI& FrontEndManager::EditorGUI()
{
	return *m_engineGUI;
}

const EngineGUI& FrontEndManager::EditorGUI() const
{
	return *m_engineGUI;
}
