#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Engine/UI/GameGUIAsset.h"

class GameGUICreatorModel {
public:
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

	void Reset();

	std::vector<GameGUIAsset> assets;
	GUIRole selectedGUI = GUIRole::MainMenu;
	int selectedWidgetIndex = -1;
	bool showCreateWidgetPopup = false;
	bool showBindingPopup = false;
	bool pendingProgressBarCreation = false;
	bool pendingProgressBarBindingComplete = false;
	bool showTexturePickerPopup = false;
	bool newWidgetIsText = false;
	bool newWidgetIsImage = false;
	bool newWidgetIsProgressBar = false;
	bool lockWidgetSize = false;
	char newWidgetName[64] = { 0 };
	char newWidgetText[128] = { 0 };
	char newWidgetTexture[256] = { 0 };
	TexturePickerTarget texturePickerTarget = TexturePickerTarget::None;
	std::filesystem::path texturePickerRootDirectory;
	std::filesystem::path texturePickerCurrentDirectory;
	std::filesystem::path texturePickerSelectedPath;
	std::string bindingWidgetName;
	float lockedWidgetSizeRatio = 1.0f;
	GameGUIActionType newWidgetAction = GameGUIActionType::None;
	GameGUIWidgetDef pendingProgressBarWidget;
	bool previousShowAxis = true;
	bool previousShowGrid = true;
	bool previousViewStateCaptured = false;
};
