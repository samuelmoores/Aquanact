#include "Engine/UI/GameGUICreatorModel.h"

void GameGUICreatorModel::Reset()
{
	assets.clear();
	selectedGUI = GUIRole::MainMenu;
	selectedWidgetIndex = -1;
	showCreateWidgetPopup = false;
	showBindingPopup = false;
	pendingProgressBarCreation = false;
	pendingProgressBarBindingComplete = false;
	showTexturePickerPopup = false;
	newWidgetIsText = false;
	newWidgetIsImage = false;
	newWidgetIsProgressBar = false;
	lockWidgetSize = false;
	newWidgetName[0] = '\0';
	newWidgetText[0] = '\0';
	newWidgetTexture[0] = '\0';
	texturePickerTarget = TexturePickerTarget::None;
	texturePickerRootDirectory.clear();
	texturePickerCurrentDirectory.clear();
	texturePickerSelectedPath.clear();
	bindingWidgetName.clear();
	lockedWidgetSizeRatio = 1.0f;
	newWidgetAction = GameGUIActionType::None;
	pendingProgressBarWidget = {};
	previousShowAxis = true;
	previousShowGrid = true;
	previousViewStateCaptured = false;
}

