#pragma once

#include "UIAsset.h"

#include <string>
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

	// MyGUI asks the engine to decode images into raw pixels here. This became a
	// custom loader because the default MyGUI image path was not aware of our stb_image
	// wrapper and the engine's asset layout.
	void* loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename) override;
	// The runtime UI only needs loading, not saving. We keep the method because the
	// MyGUI interface requires it, but the engine does not use it yet.
	void saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename) override;

private:
	// MyGUI asks for raw CPU pixel memory here. The memory has to stay valid long enough
	// for the backend to upload it into an OpenGL texture. MyGUI/OpenGLTexture takes
	// ownership and deletes the buffer with delete[], so we must return heap memory.
};

class GameGUI {
public:
	GameGUI() = default;

	// Startup had to be done in a specific order:
	// platform first, then resources, then Gui, then widgets.
	// Earlier runtime crashes came from initializing these pieces out of sequence.
	void startUp(Window& window);
	// Shutdown mirrors startup in reverse order so widgets die before the GUI platform.
	void shutDown();

	void BeginFrame();
	// Draw is where the MyGUI overlay is actually submitted to OpenGL.
	// The final fix was to restore a GUI-safe GL state before this call.
	void Draw();
	void EndFrame();

	// Creates the current smoke-test widget on demand instead of at startup.
	// This lets the UI creator own the moment the first runtime widget appears.
	void CreateTestButton();
	// Loads a whole UI asset and rebuilds the live MyGUI widgets from its definitions.
	void LoadUIAsset(const UIAsset& asset);
	void ClearUI();

private:
	void CreateWidgetFromDef(const UIWidgetDef& def);
	void DestroyTestButton();

	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;
	MyGUI::Button* m_testButton = nullptr;
	std::vector<MyGUI::Widget*> m_runtimeWidgets;
	UIAsset m_loadedAsset;
	bool m_initialized = false;
};
