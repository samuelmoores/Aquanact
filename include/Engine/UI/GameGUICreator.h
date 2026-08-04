#pragma once

#include "Engine/UI/GameGUIAsset.h"
#include <filesystem>
#include <string>
#include <vector>
class Window;
class Camera;
class GameGUICreator {
public:
	using MenuNavigationMode = GameGUIMenuNavigationMode;
	GameGUICreator() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera);
	void EndFrame();
	void CaptureEditorViewState(bool showAxis, bool showGrid);
	bool IsMainMenuSelected() const;
	std::string SelectedGUIAssetName() const;
	bool SelectGUIAsset(const std::string& assetName);
	void PreviewSelectedGUI();
	void RestoreEditorViewState();
	void SaveAllRoleGUIs();
	void SetMenuNavigationMode(MenuNavigationMode mode);
private:
	enum class GUIRole { MainMenu=0, HUD=1, Count=2 };
	enum class TexturePickerTarget { None, NewWidgetTexture, SelectedWidgetTexture };
	void DrawCreateWidgetPopup();
	void DrawBindingPopup();
	void DrawTexturePickerPopup();
	void DrawWidgetList();
	void DrawWidgetDetails();
	static std::size_t GUIIndex(GUIRole role);
	static const char* GUIName(GUIRole role);
	std::filesystem::path GUIPathFor(const GameGUIAsset& asset) const;
	GameGUIAsset& CurrentRoleGUI();
	const GameGUIAsset& CurrentRoleGUI() const;
	GameGUIAsset& GUIFor(GUIRole role);
	const GameGUIAsset& GUIFor(GUIRole role) const;
	void OpenTexturePicker(TexturePickerTarget target);
	void AddButtonWidget();
	void AddImageWidget();
	void AddProgressBarWidget();
	void AddPanelWidget();
	void ApplyPanelButtonLayout(GameGUIWidgetDef& panel);
	void SaveSelectedRoleGUI();
	void LoadSelectedRoleGUI();
	void DeleteSelectedWidget();
	void SyncRuntimePreview();
	void LoadNavigationSettingsFromAsset();
	int m_pointerWidth = 40;
	int m_pointerHeight = 40;
	int m_pointerGap = 24;
	float m_highlightR = 1.0f;
	float m_highlightG = 1.0f;
	float m_highlightB = 0.0f;
	float m_selectedR = 1.0f; float m_selectedG = 1.0f; float m_selectedB = 1.0f;
	int m_pointerSkinIndex = 0;
	bool m_newWidgetIsPanel = false;
	std::string m_newButtonParentPanel;
	int m_dimensionRequestWidth = 32;
	int m_dimensionRequestHeight = 21;
	std::string m_dimensionRequestWidgetName;
	Window* m_window = nullptr; bool m_initialized = false; bool m_showCreateWidgetPopup = false; bool m_showBindingPopup = false; bool m_pendingProgressBarCreation = false; bool m_pendingProgressBarBindingComplete = false; bool m_showTexturePickerPopup = false; bool m_newWidgetIsImage = false; bool m_newWidgetIsProgressBar = false; bool m_lockWidgetSize = false; char m_newWidgetName[64] = {0}; char m_newWidgetTexture[256] = {0}; TexturePickerTarget m_texturePickerTarget = TexturePickerTarget::None; std::filesystem::path m_texturePickerRootDirectory; std::filesystem::path m_texturePickerCurrentDirectory; std::filesystem::path m_texturePickerSelectedPath; std::string m_bindingWidgetName; float m_lockedWidgetSizeRatio = 1.0f; GameGUIActionType m_newWidgetAction = GameGUIActionType::None; std::string m_newWidgetLaunchLevel; GameGUIWidgetDef m_pendingProgressBarWidget; std::vector<GameGUIAsset> m_assets; GUIRole m_selectedGUI = GUIRole::MainMenu; int m_selectedWidgetIndex = -1; bool m_previousShowAxis = true; bool m_previousShowGrid = true; bool m_previousViewStateCaptured = false; MenuNavigationMode m_menuNavigationMode = MenuNavigationMode::Pointer; int m_boxPadding = 8; int m_boxOffsetX = 0; int m_boxOffsetY = 0; int m_boxSkinIndex = 0;
};

