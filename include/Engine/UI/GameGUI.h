#pragma once

#include "Engine/UI/GameGUIAsset.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>

class Window;
namespace MyGUI { class Button; class OpenGLPlatform; }

class GameGUIImageLoader final : public MyGUI::OpenGLImageLoader {
public:
	~GameGUIImageLoader() override = default;
	void* loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename) override;
	void saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename) override;
};

class GameGUI {
public:
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
private:
	MyGUI::Widget* CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	void BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget);
	void OnWidgetClicked(MyGUI::Widget* sender);
	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;
	MyGUI::Button* m_testButton = nullptr;
	std::vector<MyGUI::Widget*> m_runtimeWidgets;
	std::vector<MyGUI::Button*> m_controllerButtons;
	std::unordered_map<std::string, MyGUI::Widget*> m_runtimeWidgetLookup;
	GameGUIAsset m_loadedAsset;
	int m_focusedControllerButton = -1;
	bool m_initialized = false;
};

