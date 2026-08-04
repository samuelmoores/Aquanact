#pragma once

#include "Engine/UI/GameGUIAsset.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>
#include <MYGUI/MyGUI_Colour.h>

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
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw();
	void EndFrame();
	void LoadUIAsset(const GameGUIAsset& asset);
	void ClearUI();
	void FocusFirstControllerButton();
	void ClearControllerFocus();
	bool HasControllerFocus() const;
	void NavigateControllerButtons(int direction);
	void ActivateFocusedControllerButton();
	void SetMenuNavigationMode(MenuNavigationMode mode);
	void SetBoxStyle(int padding, int offsetX, int offsetY);
	void SetBoxSkin(const std::string& skin);
	void SetPointerStyle(int width, int height, int gap);
	void SetHighlightColour(float r, float g, float b);
	void SetPointerSkin(const std::string& skin);
private:
	MyGUI::Widget* CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	void BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget);
	void OnWidgetClicked(MyGUI::Widget* sender);
	void OnButtonMouseFocus(MyGUI::Widget* sender, MyGUI::Widget* oldFocus);
	void OnButtonMouseLostFocus(MyGUI::Widget* sender, MyGUI::Widget* newFocus);
	void PositionMenuPointer(MyGUI::Widget* button);
	void ApplyTextHighlight(MyGUI::Button* button, bool highlighted);
	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;
	MyGUI::Button* m_testButton = nullptr;
	MyGUI::Widget* m_menuPointer = nullptr;
	MyGUI::Widget* m_menuBox = nullptr;
	std::vector<MyGUI::Widget*> m_runtimeWidgets;
	std::vector<MyGUI::Button*> m_controllerButtons;
	std::unordered_map<std::string, MyGUI::Widget*> m_runtimeWidgetLookup;
	std::unordered_map<MyGUI::Button*, MyGUI::Colour> m_buttonDefaultTextColours;
	std::unordered_map<MyGUI::Button*, MyGUI::TextBox*> m_buttonLabels;
	GameGUIAsset m_loadedAsset;
	int m_focusedControllerButton = -1;
	MenuNavigationMode m_menuNavigationMode = MenuNavigationMode::Pointer;
	int m_boxPadding = 8;
	int m_boxOffsetX = 0;
	int m_boxOffsetY = 0;
	std::string m_boxSkin = "WindowFrameSkin";
	std::string m_pointerSkin = "NavigationArrowRight1";
	int m_pointerWidth = 40, m_pointerHeight = 40, m_pointerGap = 24;
	MyGUI::Colour m_highlightColour = MyGUI::Colour(1.0f, 1.0f, 0.0f);
	MyGUI::Colour m_selectedColour = MyGUI::Colour(1.0f, 1.0f, 1.0f);
	bool m_initialized = false;
};

