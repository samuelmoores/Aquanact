#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "UIAsset.h"

class Window;
class Camera;

class UICreator {
public:
	UICreator() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera);
	void EndFrame();

private:
	void DrawCreateAssetPopup();
	std::filesystem::path AssetPathFor(const UIAsset& asset) const;
	UIAsset& CurrentAsset();
	const UIAsset& CurrentAsset() const;
	void AddButtonWidget();
	void AddUIAsset(const std::string& name);
	void SaveCurrentAsset();
	void LoadCurrentAsset();
	void DeleteSelectedWidget();
	void DeleteSelectedAsset();
	void SyncRuntimePreview();
	bool IsCurrentAssetStoredOnDisk() const;

	Window* m_window = nullptr;
	bool m_initialized = false;
	bool m_showCreateAssetPopup = false;
	char m_newAssetName[64] = { 0 };
	std::vector<UIAsset> m_assets;
	int m_selectedAssetIndex = -1;
	int m_selectedWidgetIndex = -1;
};
