#pragma once

#include "Engine/UI/GameGUIAsset.h"
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
class Window;
class Camera;
class GameGUICreator {
public:
	using MenuNavigationMode = GameGUIMenuNavigationMode;
	enum class NewWidgetType { Button, Panel, Image, ProgressBar, Text };
	GameGUICreator() = default;

	// Lifecycle and frame flow
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void EndFrame();

	// External state and selection control
	void CaptureEditorViewState(bool showAxis, bool showGrid);
	bool IsMainMenuSelected() const;
	std::string SelectedGUIAssetName() const;
	bool SelectGUIAsset(const std::string& assetName);
	void PreviewSelectedGUI();
	void RestoreEditorViewState();
	void SaveAllRoleGUIs();
	void SetMenuNavigationMode(MenuNavigationMode mode);

	friend class GameGUICreatorView;

private:
	enum class GUIRole { MainMenu=0, HUD=1, Count=2 };

	// Asset access and persistence helpers.
	std::filesystem::path GUIPathFor(const GameGUIAsset& asset) const;
	GameGUIAsset& CurrentGameGUI();
	const GameGUIAsset& CurrentRoleGUI() const;
	GameGUIAsset& GUIFor(GUIRole role);
	const GameGUIAsset& GUIFor(GUIRole role) const;

	// Simple role-to-index / role-to-label helpers.
	static std::size_t GUIIndex(GUIRole role);
	static const char* GUIName(GUIRole role);
	void LoadNavigationSettingsFromAsset();

	// Persistence and runtime synchronization.
	void SaveSelectedRoleGUI();
	void LoadSelectedRoleGUI();
	void SyncRuntimePreview();
	// Widget creation and mutation helpers.
	void AddButtonWidget();
	void AddImageWidget();
	void AddProgressBarWidget();
	void AddPanelWidget();
	void AddTextWidget();
	void CenterWidget(GameGUIWidgetDef& widget);
	void ApplyPanelButtonLayout(GameGUIWidgetDef& panel);
	void DeleteSelectedWidget();
	// Popup and detail rendering helpers.
	void DrawCreateWidgetPopup();
	void DrawCreateWidgetPopupHeader(const char* title);
	void DrawCreateWidgetPopupFooter();
	void DrawCreateWidgetNameField();
	void DrawCreateWidgetTextureField();
	void DrawCreateActionField();
	void DrawCreateLaunchLevelField();
	void OpenCreateWidgetPopup(NewWidgetType type);
	void DrawCreateButtonPopup();
	void DrawCreatePanelPopup();
	void DrawCreateImagePopup();
	void DrawCreateProgressBarPopup(); 
	void DrawCreateTextPopup();
	void DrawBindingPopup();
	void DrawButtonWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget);
	void DrawPanelWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget);
	void DrawImageWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget);
	void DrawTextWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget);
	void DrawProgressBarWidgetDetails(GameGUICreator& creator, GameGUIAsset& asset, GameGUIWidgetDef& widget);

	// ************************
	// ******* Members ********
	// ************************

	// Asset selection and widget focus.
	Window* m_window = nullptr;
	bool m_initialized = false;
	std::vector<GameGUIAsset> m_assets;
	GUIRole m_selectedGUI = GUIRole::MainMenu;
	int m_selectedWidgetIndex = -1;

	// Editor capture and restoration state.
	bool m_previousShowAxis = true;
	bool m_previousShowGrid = true;
	bool m_previousViewStateCaptured = false;

	// Frontend mode and visible tool windows.
	MenuNavigationMode m_menuNavigationMode = MenuNavigationMode::Pointer;
	bool m_showCreateWidgetPopup = false;
	bool m_showBindingPopup = false;
	bool m_showWidgetListWindow = true;
	bool m_showWidgetDetailsWindow = true;

	// Widget creation defaults and edit buffers.
	NewWidgetType m_newWidgetType = NewWidgetType::Button;
	bool m_lockWidgetSize = false;
	char m_newWidgetName[64] = { 0 };
	char m_newWidgetTexture[256] = { 0 };
	GameGUIActionType m_newWidgetAction = GameGUIActionType::None;
	std::string m_newWidgetLaunchLevel;
	std::string m_newButtonParentPanel;
	std::string m_bindingWidgetName;
	std::string m_dimensionRequestWidgetName;
	int m_dimensionRequestWidth = 32;
	int m_dimensionRequestHeight = 21;
	float m_lockedWidgetSizeRatio = 1.0f;

	// Navigation and style values.
	int m_pointerWidth = 40;
	int m_pointerHeight = 40;
	int m_pointerGap = 24;
	float m_highlightR = 1.0f;
	float m_highlightG = 1.0f;
	float m_highlightB = 0.0f;
	float m_selectedR = 1.0f;
	float m_selectedG = 1.0f;
	float m_selectedB = 1.0f;
	int m_pointerSkinIndex = 0;
	int m_boxPadding = 8;
	int m_boxOffsetX = 0;
	int m_boxOffsetY = 0;
	int m_boxSkinIndex = 0;
};

