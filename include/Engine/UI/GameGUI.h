#pragma once

#include "Engine/UI/GameGUIAsset.h"

#include <string>
#include <functional>
#include <utility>
#include <unordered_map>
#include <vector>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>
#include <MYGUI/MyGUI_Colour.h>
#include <MYGUI/MyGUI_ProgressBar.h>

class Window;
namespace MyGUI { class Button; class ImageBox; class TextBox; class Widget; class OpenGLPlatform; }

class GameGUIImageLoader final : public MyGUI::OpenGLImageLoader {
public:
	~GameGUIImageLoader() override = default;
	void* loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename) override;
	void saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename) override;
};

class GameGUI {
public:
	using MenuNavigationMode = GameGUIMenuNavigationMode;
	GameGUI() = default;

	// Lifecycle
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw();
	void EndFrame();
	bool IsInitialized() const { return m_initialized; }

	// UI loading and reset
	void LoadUIAsset(const GameGUIAsset& asset);
	void ClearUI();

	// Controller navigation
	void FocusFirstControllerButton();
	void ClearControllerFocus();
	void RelinquishControllerFocusToMouse();
	bool HasControllerFocus() const;
	void NavigateControllerButtons(int direction);
	void ActivateFocusedControllerButton();
	bool NavigateBackFromSubPanel();

	// Visual configuration
	void SetMenuNavigationMode(MenuNavigationMode mode);
	void SetBoxStyle(int padding, int offsetX, int offsetY);
	void SetBoxSkin(const std::string& skin);
	void SetPointerStyle(int width, int height, int gap);
	void SetHighlightColour(float r, float g, float b);
	void SetPointerSkin(const std::string& skin);

	// Lookup
	MyGUI::Widget* RuntimeWidget(const std::string& name) const;
private:
	// Widget creation
	MyGUI::Widget* CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	MyGUI::Widget* CreatePanelWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	MyGUI::Button* CreateButtonWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	MyGUI::TextBox* CreateTextWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	MyGUI::ImageBox* CreateImageWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	MyGUI::ProgressBar* CreateProgressBarWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	std::string ResolveButtonSkin(const GameGUIWidgetDef& def) const;
	void ConfigureProgressBar(MyGUI::ProgressBar* progress);
	void SetButtonVisualState(MyGUI::Button* button, const GameGUIWidgetDef& def);
	void SetButtonFocusState(MyGUI::Button* button, const GameGUIWidgetDef& def);
	void SetButtonLabel(MyGUI::Button* button, const GameGUIWidgetDef& def, int buttonWidth, int buttonHeight);
	void HookButtonClick(MyGUI::Button* button, const GameGUIWidgetDef& def);
	void FinalizeAndLogWidget(MyGUI::Widget* widget, const GameGUIWidgetDef& def, bool promoteToTopLayer, bool allowMouseFocus, bool inheritPick);

	// Runtime binding and input handling
	void BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget);
	void BindTextWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget);
	void BindTextWidgetValue(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget);
	void BindTextWidgetEvent(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget);
	void BindValueWidgetRefresh(const GameGUIWidgetDef& def, MyGUI::Widget* widget, const std::function<void(float)>& applyValue);
	void OnWidgetClicked(MyGUI::Widget* sender);
	void OnButtonMouseFocus(MyGUI::Widget* sender, MyGUI::Widget* oldFocus);
	void OnButtonMouseLostFocus(MyGUI::Widget* sender, MyGUI::Widget* newFocus);
	void PositionMenuPointer(MyGUI::Widget* button);
	void ApplyTextHighlight(MyGUI::Button* button, bool highlighted);
	void BindProgressBarFromDef(const GameGUIWidgetDef& def, MyGUI::ProgressBar* progress);
	void RefreshVisibleControllerButtons();
	void FocusFirstControllerButtonInPanel(const std::string& panelName);

	// Core runtime
	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;

	// Runtime widget registry
	std::vector<MyGUI::Widget*> m_runtimeWidgets;
	std::unordered_map<std::string, MyGUI::Widget*> m_runtimeWidgetLookup;

	// Controller navigation state
	MyGUI::Button* m_testButton = nullptr;
	MyGUI::Widget* m_menuPointer = nullptr;
	MyGUI::Widget* m_menuBox = nullptr;
	std::vector<MyGUI::Button*> m_allControllerButtons;
	std::vector<MyGUI::Button*> m_controllerButtons;
	std::vector<std::pair<std::string, std::string>> m_subPanelHistory;
	int m_focusedControllerButton = -1;
	MyGUI::Button* m_lastFocusSoundButton = nullptr;

	// Widget styling and selection state
	std::unordered_map<MyGUI::Button*, MyGUI::Colour> m_buttonDefaultTextColours;
	std::unordered_map<MyGUI::Button*, MyGUI::TextBox*> m_buttonLabels;
	std::unordered_map<MyGUI::Button*, std::string> m_buttonFocusSounds;
	MenuNavigationMode m_menuNavigationMode = MenuNavigationMode::Pointer;
	int m_boxPadding = 8;
	int m_boxOffsetX = 0;
	int m_boxOffsetY = 0;
	std::string m_boxSkin = "WindowFrameSkin";
	std::string m_pointerSkin = "NavigationArrowRight1";
	int m_pointerWidth = 40, m_pointerHeight = 40, m_pointerGap = 24;
	MyGUI::Colour m_highlightColour = MyGUI::Colour(1.0f, 1.0f, 0.0f);
	MyGUI::Colour m_selectedColour = MyGUI::Colour(1.0f, 1.0f, 1.0f);

	// Loaded asset and initialization flag
	GameGUIAsset m_loadedAsset;
	bool m_initialized = false;
};

