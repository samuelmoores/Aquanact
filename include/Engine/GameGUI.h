#pragma once

#include "Engine/GameGUIAsset.h"
#include "Engine/Entity.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>

class Window;
namespace MyGUI
{
	class Button;
	class OpenGLPlatform;
}

class GameGUIImageLoader final : public MyGUI::OpenGLImageLoader
{
public:
	~GameGUIImageLoader() override = default;
	// MyGUI requests decoded pixels through this callback, so the loader bridges
	// the engine's file/image handling into MyGUI's OpenGL texture pipeline.
	void* loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename) override;
	// The runtime path does not export textures yet, so saving remains a stub.
	void saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename) override;
};

class GameGUI {
public:
	GameGUI() = default;
	// Startup and shutdown bracket the lifetime of the MyGUI platform, renderer,
	// and widget objects owned by the runtime wrapper.
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	// Draw submits the MyGUI overlay after the engine has finished its scene pass.
	void Draw();
	void EndFrame();
	// UI assets are deserialized into live MyGUI widgets through this entry point.
	void LoadUIAsset(const GameGUIAsset& asset);
	void ClearUI();

private:
	// Converts the asset-level widget description into an actual MyGUI widget.
	MyGUI::Widget* CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent);
	void BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget);
	// Handles runtime widget clicks for simple built-in actions like quitting.
	void OnWidgetClicked(MyGUI::Widget* sender);

	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;
	MyGUI::Button* m_testButton = nullptr;
	std::vector<MyGUI::Widget*> m_runtimeWidgets;
	std::unordered_map<std::string, MyGUI::Widget*> m_runtimeWidgetLookup;
	GameGUIAsset m_loadedAsset;
	bool m_initialized = false;
};

