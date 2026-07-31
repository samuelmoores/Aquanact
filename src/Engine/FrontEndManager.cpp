#include "Engine/FrontEndManager.h"

#include "Engine/EngineGUI.h"
#include "Engine/Window.h"
#include "Engine/Camera.h"
#include "Engine/FileManager.h"
#include "Engine/LevelManager.h"
#include "Engine/ProjectManager.h"
#include "Engine/Root.h"
#include <imgui.h>

FrontEndManager::~FrontEndManager()
{
	shutDown();
}

// make sure this FrontEndManager is intentionally the owner of all three UI domains
void FrontEndManager::startUp(Window& window)
{
	// both engine and game need the game ui
	if (!m_gameGUI)
	{
		m_gameGUI = std::make_unique<GameGUIManager>();
	}

	// only the engine needs the engine and ui creator ui
	if (Root::Current().State().IsEditorMode())
	{
		if (!m_engineGUI)
		{
			m_engineGUI = std::make_unique<EngineGUI>();
		}
		if (!m_uiCreator)
		{
			m_uiCreator = std::make_unique<GameGUICreator>();
		}
		m_engineGUI->startUp(window);
		m_uiCreator->startUp(window);
	}
	else
	{
		// only the built game needs to turn off game debugging
		Root::Current().GameModeDebugFlag() = false;
	}

	// both need the game ui
	m_gameGUI->startUp(window);
}

void FrontEndManager::shutDown()
{
	if (m_engineGUI)
	{
		m_engineGUI->shutDown();
		m_engineGUI.reset();
	}
	if (m_gameGUI)
	{
		m_gameGUI->shutDown();
		m_gameGUI.reset();
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
	if (m_gameGUI)
	{
		m_gameGUI->BeginFrame();
	}
	if (m_uiCreator)
	{
		m_uiCreator->BeginFrame();
	}
}

void FrontEndManager::DrawEngineGUI(const Camera& camera, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager)
{
	if (IsEditorMode() && m_mode == FrontEndMode::EngineEditor && m_engineGUI)
	{
		m_engineGUI->Draw(camera, fileManager, levelManager, projectManager);
	}
}

void FrontEndManager::DrawRuntimeGUI()
{
	if (m_gameGUI && IsGameMode())
	{
		m_gameGUI->Draw();
		m_gameGUI->DrawDiagnosticsWindow();
	}
}

void FrontEndManager::DrawCreatorGUI(const Camera& camera)
{
	if (IsEditorMode() && m_mode == FrontEndMode::GameGUICreator && m_uiCreator)
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
	if (m_gameGUI)
	{
		m_gameGUI->EndFrame();
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

void FrontEndManager::OpenGameGUICreator()
{
	if (m_engineGUI && m_uiCreator)
	{
		m_uiCreator->CaptureEditorViewState(m_engineGUI->ShowAxis(), m_engineGUI->ShowGrid());
		m_engineGUI->SetShowAxis(false);
		m_engineGUI->SetShowGrid(false);
	}
	m_mode = FrontEndMode::GameGUICreator;
}

void FrontEndManager::ApplyProjectState(
	bool editorShowAxis,
	bool editorShowGrid,
	const std::vector<std::string>& sceneAssets,
	const std::string& activeAssetName,
	const std::string& imguiLayout)
{
	if (m_engineGUI)
	{
		m_engineGUI->SetShowAxis(editorShowAxis);
		m_engineGUI->SetShowGrid(editorShowGrid);
	}
	if (m_gameGUI)
	{
		m_gameGUI->ApplyProjectState(sceneAssets, activeAssetName);
	}
	if (!imguiLayout.empty())
	{
		ImGui::LoadIniSettingsFromMemory(imguiLayout.c_str(), imguiLayout.size());
	}
}

void FrontEndManager::ReturnToEngineGUIEditor()
{
	if (m_uiCreator)
	{
		m_uiCreator->RestoreEditorViewState();
	}
	m_mode = FrontEndMode::EngineEditor;
}

EngineMode FrontEndManager::AppMode() const
{
	return Root::Current().State().Mode();
}

bool FrontEndManager::IsEditorMode() const
{
	return Root::Current().State().IsEditorMode();
}

bool FrontEndManager::IsGameMode() const
{
	return Root::Current().State().IsGameMode();
}

EngineGUI& FrontEndManager::EditorGUI()
{
	return *m_engineGUI;
}

const EngineGUI& FrontEndManager::EditorGUI() const
{
	return *m_engineGUI;
}

GameGUIManager& FrontEndManager::RuntimeGUI()
{
	return *m_gameGUI;
}

const GameGUIManager& FrontEndManager::RuntimeGUI() const
{
	return *m_gameGUI;
}

GameGUICreator& FrontEndManager::Creator()
{
	return *m_uiCreator;
}

const GameGUICreator& FrontEndManager::Creator() const
{
	return *m_uiCreator;
}

