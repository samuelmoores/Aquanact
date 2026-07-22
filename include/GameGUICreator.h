#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "GameGUIAsset.h"

class Window;
class Camera;

class GameGUICreator {
public:
	GameGUICreator() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera);
	void EndFrame();

private:
	void DrawCreateAssetPopup();
	void DrawCreateWidgetPopup();
	std::filesystem::path AssetPathFor(const GameGUIAsset& asset) const;
	GameGUIAsset& CurrentAsset();
	const GameGUIAsset& CurrentAsset() const;
	void AddButtonWidget();
	void AddTextWidget();
	void AddImageWidget();
	void AddGameGUIAsset(const std::string& name);
	void SaveCurrentAsset();
	void LoadCurrentAsset();
	void DeleteSelectedWidget();
	void DeleteSelectedAsset();
	void SyncRuntimePreview();
	bool IsCurrentAssetStoredOnDisk() const;

	Window* m_window = nullptr;
	bool m_initialized = false;
	bool m_showCreateAssetPopup = false;
	bool m_showCreateWidgetPopup = false;
	bool m_newWidgetIsText = false;
	bool m_newWidgetIsImage = false;
	char m_newAssetName[64] = { 0 };
	char m_newWidgetName[64] = { 0 };
	char m_newWidgetText[128] = { 0 };
	char m_newWidgetTexture[256] = { 0 };
	GameGUIActionType m_newWidgetAction = GameGUIActionType::None;
	std::vector<GameGUIAsset> m_assets;
	int m_selectedAssetIndex = -1;
	int m_selectedWidgetIndex = -1;
};
