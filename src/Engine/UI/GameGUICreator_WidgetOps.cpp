#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Level.h"
#include "Engine/Core/LevelManager.h"
#include "Engine/Core/Window.h"

#include <algorithm>
#include <cstdio>

namespace {
	std::filesystem::path TextureDirectory()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "textures";
#else
		return std::filesystem::current_path() / "assets" / "textures";
#endif
	}
}

void GameGUICreator::OpenTexturePicker(TexturePickerTarget target)
{
	m_texturePickerTarget = target;
	m_texturePickerRootDirectory = TextureDirectory();
	m_texturePickerCurrentDirectory = m_texturePickerRootDirectory;
	m_texturePickerSelectedPath.clear();
	m_showTexturePickerPopup = true;
}

void GameGUICreator::AddButtonWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add button widget skipped: no active GUI");
		return;
	}

	GameGUIWidgetDef button;
	button.type = "Button";
	button.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "button";
	button.skin = "ButtonSkin";
	button.text = "Test Button";
	button.layer = "Main";
	button.width = 320;
	button.height = 90;
	button.action = m_newWidgetAction;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	button.x = std::max(0, (framebufferWidth - button.width) / 2);
	button.y = std::max(0, (framebufferHeight - button.height) / 2);

	GameGUIAsset& asset = CurrentRoleGUI();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&button](const GameGUIWidgetDef& widget)
	{
		return widget.name == button.name;
	}))
	{
		button.name = "button_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(button);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetAction = GameGUIActionType::None;
	Root::Current().Debugger().LogMessage(
		std::string("Created button widget in ") + GUIName(m_selectedGUI) + ": name='" + button.name +
		"', gui='" + asset.name +
		"', pos=(" + std::to_string(button.x) + "," + std::to_string(button.y) + ")" +
		", size=(" + std::to_string(button.width) + "x" + std::to_string(button.height) + ")");
}

void GameGUICreator::AddTextWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add text widget skipped: no active GUI");
		return;
	}

	GameGUIWidgetDef text;
	text.type = "TextBox";
	text.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "text";
	text.skin = "TextBox";
	text.text = m_newWidgetText[0] != '\0' ? m_newWidgetText : "New Text";
	text.layer = "Main";
	text.width = 360;
	text.height = 48;
	text.fontSize = 0;
	text.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	text.x = std::max(0, (framebufferWidth - text.width) / 2);
	text.y = std::max(0, (framebufferHeight - text.height) / 2);

	GameGUIAsset& asset = CurrentRoleGUI();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&text](const GameGUIWidgetDef& widget)
	{
		return widget.name == text.name;
	}))
	{
		text.name = "text_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(text);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	Root::Current().Debugger().LogMessage(
		std::string("Created text widget in ") + GUIName(m_selectedGUI) + ": name='" + text.name +
		"', gui='" + asset.name +
		"', pos=(" + std::to_string(text.x) + "," + std::to_string(text.y) + ")" +
		", size=(" + std::to_string(text.width) + "x" + std::to_string(text.height) + ")");
}

void GameGUICreator::AddImageWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add image widget skipped: no active GUI");
		return;
	}

	GameGUIWidgetDef image;
	image.type = "ImageBox";
	image.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "image";
	image.skin = "ImageBox";
	image.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
	image.layer = "Main";
	image.width = 256;
	image.height = 256;
	image.fontSize = 0;
	image.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	image.x = std::max(0, (framebufferWidth - image.width) / 2);
	image.y = std::max(0, (framebufferHeight - image.height) / 2);

	GameGUIAsset& asset = CurrentRoleGUI();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&image](const GameGUIWidgetDef& widget)
	{
		return widget.name == image.name;
	}))
	{
		image.name = "image_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(image);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	Root::Current().Debugger().LogMessage(
		std::string("Created image widget in ") + GUIName(m_selectedGUI) + ": name='" + image.name +
		"', gui='" + asset.name +
		"', texture='" + image.texture +
		"', pos=(" + std::to_string(image.x) + "," + std::to_string(image.y) + ")" +
		", size=(" + std::to_string(image.width) + "x" + std::to_string(image.height) + ")");
}

void GameGUICreator::AddProgressBarWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add progress bar widget skipped: no active GUI");
		return;
	}

	GameGUIWidgetDef progress;
	if (m_pendingProgressBarCreation)
	{
		progress = m_pendingProgressBarWidget;
	}
	else
	{
		progress.type = "ProgressBar";
		progress.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
		progress.skin = "ImageBox";
		progress.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
		progress.layer = "Main";
		progress.width = 256;
		progress.height = 32;
		progress.textureWidth = progress.width;
		progress.textureHeight = progress.height;
		progress.defaultTextureWidth = progress.textureWidth;
		progress.defaultTextureHeight = progress.textureHeight;
		progress.fontSize = 0;
		progress.action = GameGUIActionType::None;
	}

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	progress.x = std::max(0, (framebufferWidth - progress.width) / 2);
	progress.y = std::max(0, (framebufferHeight - progress.height) / 2);

	GameGUIAsset& asset = CurrentRoleGUI();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&progress](const GameGUIWidgetDef& widget)
	{
		return widget.name == progress.name;
	}))
	{
		progress.name = "progress_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(progress);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_pendingProgressBarWidget = {};
	Root::Current().Debugger().LogMessage(
		std::string("Created progress bar widget in ") + GUIName(m_selectedGUI) + ": name='" + progress.name +
		"', gui='" + asset.name +
		"', texture='" + progress.texture +
		"', pos=(" + std::to_string(progress.x) + "," + std::to_string(progress.y) + ")" +
		", size=(" + std::to_string(progress.width) + "x" + std::to_string(progress.height) + ")");
}

void GameGUICreator::DeleteSelectedWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		return;
	}

	GameGUIAsset& asset = CurrentRoleGUI();
	if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		return;
	}

	const std::string deletedWidgetName = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)].name;
	asset.widgets.erase(asset.widgets.begin() + m_selectedWidgetIndex);
	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.parentName == deletedWidgetName)
		{
			widget.parentName.clear();
		}
	}
	if (asset.widgets.empty())
	{
		m_selectedWidgetIndex = -1;
	}
	else if (m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	}

	SyncRuntimePreview();
}

