#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Engine/GameGUIAsset.h"

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
	void CaptureEditorViewState(bool showAxis, bool showGrid);
	bool IsMainMenuSelected() const;
	void RestoreEditorViewState();
	void SaveAllRoleGUIs();

private:
	enum class GUIRole {
		MainMenu = 0,
		HUD = 1,
		PauseMenu = 2,
		PlayerUI = 3,
		Count = 4
	};

	enum class TexturePickerTarget {
		None,
		NewWidgetTexture,
		SelectedWidgetTexture
	};

	void DrawCreateWidgetPopup();
	void DrawBindingPopup();
	void DrawTexturePickerPopup();
	static std::size_t GUIIndex(GUIRole role);
	static const char* GUIName(GUIRole role);
	std::filesystem::path GUIPathFor(const GameGUIAsset& asset) const;
	GameGUIAsset& CurrentRoleGUI();
	const GameGUIAsset& CurrentRoleGUI() const;
	GameGUIAsset& GUIFor(GUIRole role);
	const GameGUIAsset& GUIFor(GUIRole role) const;
	void OpenTexturePicker(TexturePickerTarget target);
	void AddButtonWidget();
	void AddTextWidget();
	void AddImageWidget();
	void AddProgressBarWidget();
	void SaveSelectedRoleGUI();
	void LoadSelectedRoleGUI();
	void DeleteSelectedWidget();
	void SyncRuntimePreview();

	Window* m_window = nullptr;
	bool m_initialized = false;
	bool m_showCreateWidgetPopup = false;
	bool m_showBindingPopup = false;
	bool m_pendingProgressBarCreation = false;
	bool m_pendingProgressBarBindingComplete = false;
	bool m_showTexturePickerPopup = false;
	bool m_newWidgetIsText = false;
	bool m_newWidgetIsImage = false;
	bool m_newWidgetIsProgressBar = false;
	bool m_lockWidgetSize = false;
	char m_newWidgetName[64] = { 0 };
	char m_newWidgetText[128] = { 0 };
	char m_newWidgetTexture[256] = { 0 };
	TexturePickerTarget m_texturePickerTarget = TexturePickerTarget::None;
	std::filesystem::path m_texturePickerRootDirectory;
	std::filesystem::path m_texturePickerCurrentDirectory;
	std::filesystem::path m_texturePickerSelectedPath;
	std::string m_bindingWidgetName;
	float m_lockedWidgetSizeRatio = 1.0f;
	GameGUIActionType m_newWidgetAction = GameGUIActionType::None;
	GameGUIWidgetDef m_pendingProgressBarWidget;
	std::vector<GameGUIAsset> m_assets;
	GUIRole m_selectedGUI = GUIRole::MainMenu;
	int m_selectedWidgetIndex = -1;
	bool m_previousShowAxis = true;
	bool m_previousShowGrid = true;
	bool m_previousViewStateCaptured = false;
};

