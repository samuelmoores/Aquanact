#pragma once

#include "GameGUIAsset.h"

#include <memory>
#include <string>
#include <vector>

class Window;

class GameGUI;

class GameGUIManager {
public:
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
	void LogAction(const std::string& message);
	void RecordClick(const std::string& message);
	void ClearUI();
	std::size_t LoadedAssetCount() const;
	std::size_t PlacedAssetCount() const;
	std::string ActiveAssetName() const;
	bool HasRuntime() const;

private:
	void ApplyActiveAsset();

	std::unique_ptr<GameGUI> m_runtime;
	std::vector<GameGUIAsset> m_assets;
	std::vector<std::string> m_sceneAssets;
	std::vector<std::string> m_actionLog;
	std::string m_lastClickMessage;
	int m_activeAssetIndex = -1;
};
