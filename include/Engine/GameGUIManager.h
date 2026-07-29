#pragma once

#include "Engine/GameGUIAsset.h"

#include <memory>
#include <string>
#include <vector>

class Window;

class GameGUI;

class GameGUIManager {
public:
	enum class UIMode
	{
		MainMenu,
		GameplayHUD,
		PauseMenu,
		PlayerUI,
		Custom
	};

	GameGUIManager();
	~GameGUIManager();

	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw();
	void EndFrame();

	void LoadUIAsset(const GameGUIAsset& asset);
	bool AddSceneAsset(const std::string& name);
	void RemoveSceneAsset(std::size_t index);
	void SetSceneAssets(const std::vector<std::string>& names);
	void UnloadUIAsset(const std::string& name);
	bool ActivateAsset(const std::string& name);
	std::vector<std::string> AssetNames() const;
	const std::vector<std::string>& SceneAssets() const;
	void DrawEditorWindow();
	void DrawDiagnosticsWindow();
	void DrawReturnButton();
	bool ShowEditorWindow() const;
	void SetShowEditorWindow(bool showEditorWindow);
	void SetUIMode(UIMode mode);
	UIMode Mode() const;
	void ShowMainMenu();
	void ShowGameplayHUD();
	void ShowPauseMenu();
	void ShowPlayerUI();
	void HideAll();
	void LogAction(const std::string& message);
	void RecordClick(const std::string& message);
	void RecordButtonClick(const std::string& assetName, const std::string& widgetName, GameGUIActionType action);
	void AppendProjectState(std::string& contents) const;
	void ApplyProjectState(const std::vector<std::string>& sceneAssets, const std::string& activeAssetName);
	void ClearUI();
	std::size_t LoadedAssetCount() const;
	std::size_t PlacedAssetCount() const;
	std::string ActiveAssetName() const;
	bool HasRuntime() const;

private:
	void ApplyActiveAsset();
	void ApplyMode();
	static const char* AssetNameForMode(UIMode mode);

	std::unique_ptr<GameGUI> m_runtime;
	std::vector<GameGUIAsset> m_assets;
	std::vector<std::string> m_sceneAssets;
	std::vector<std::string> m_actionLog;
	std::string m_lastClickMessage;
	std::string m_lastButtonAssetName;
	std::string m_lastButtonWidgetName;
	std::string m_lastButtonActionName;
	std::size_t m_buttonClickCount = 0;
	int m_activeAssetIndex = -1;
	bool m_showEditorWindow = false;
	UIMode m_mode = UIMode::MainMenu;
};

