#include "FrontEndManager.h"

#include "EngineGUI.h"
#include "Window.h"
#include "Camera.h"
#include "FileManager.h"
#include "SceneManager.h"
#include "ProjectManager.h"
#include "Globals.h"

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
	if (!m_uiCreator)
	{
		m_uiCreator = std::make_unique<UICreator>();
	}

	m_engineGUI->startUp(window);
	m_uiCreator->startUp(window);
}

void FrontEndManager::shutDown()
{
	if (m_engineGUI)
	{
		m_engineGUI->shutDown();
		m_engineGUI.reset();
	}
	if (m_uiCreator)
	{
		m_uiCreator->shutDown();
		m_uiCreator.reset();
	}
}

void FrontEndManager::BeginFrame()
{
	if (m_engineGUI)
	{
		m_engineGUI->BeginFrame();
	}
	if (m_uiCreator)
	{
		m_uiCreator->BeginFrame();
	}
}

void FrontEndManager::Draw(const Camera& camera, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager)
{
	if (!IsEditorMode())
	{
		return;
	}

	if (m_mode == FrontEndMode::EngineEditor && m_engineGUI)
	{
		m_engineGUI->Draw(camera, fileManager, sceneManager, projectManager);
	}
	else if (m_mode == FrontEndMode::UICreator && m_uiCreator)
	{
		m_uiCreator->Draw(camera);
	}
}

void FrontEndManager::EndFrame()
{
	if (m_engineGUI)
	{
		m_engineGUI->EndFrame();
	}
	if (m_uiCreator)
	{
		m_uiCreator->EndFrame();
	}
}

void FrontEndManager::SetMode(FrontEndMode mode)
{
	m_mode = mode;
}

FrontEndMode FrontEndManager::FrontEndModeValue() const
{
	return m_mode;
}

void FrontEndManager::OpenUICreator()
{
	m_mode = FrontEndMode::UICreator;
}

void FrontEndManager::ReturnToEngineEditor()
{
	m_mode = FrontEndMode::EngineEditor;
}

EngineMode FrontEndManager::AppMode() const
{
	return gEngineState.Mode();
}

bool FrontEndManager::IsEditorMode() const
{
	return gEngineState.IsEditorMode();
}

bool FrontEndManager::IsGameMode() const
{
	return gEngineState.IsGameMode();
}

EngineGUI& FrontEndManager::EditorGUI()
{
	return *m_engineGUI;
}

const EngineGUI& FrontEndManager::EditorGUI() const
{
	return *m_engineGUI;
}

UICreator& FrontEndManager::Creator()
{
	return *m_uiCreator;
}

const UICreator& FrontEndManager::Creator() const
{
	return *m_uiCreator;
}
