#pragma once

#include <string>
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

	void* loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename) override;
	void saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename) override;

private:
	// MyGUI asks for raw CPU pixel memory here. The memory has to stay valid long enough
	// for the backend to upload it into an OpenGL texture. MyGUI/OpenGLTexture takes
	// ownership and deletes the buffer with delete[], so we must return heap memory.
};

class GameGUI {
public:
	GameGUI() = default;

	void startUp(Window& window);
	void shutDown();

	void BeginFrame();
	void Draw();
	void EndFrame();

private:
	Window* m_window = nullptr;
	MyGUI::OpenGLPlatform* m_platform = nullptr;
	MyGUI::Gui* m_gui = nullptr;
	GameGUIImageLoader m_imageLoader;
	MyGUI::Button* m_testButton = nullptr;
	bool m_initialized = false;
	bool m_loggedRenderSubmission = false;
	bool m_loggedStartupState = false;
};
