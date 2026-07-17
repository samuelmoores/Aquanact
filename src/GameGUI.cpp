#include "GameGUI.h"

#include "Debug.h"
#include "Globals.h"
#include "Window.h"
#include "StbImage.h"
#include "GLHeaders.h"

#include <MYGUI/MyGUI_Button.h>
#include <MYGUI/MyGUI_Gui.h>
#include <MYGUI/MyGUI_OpenGLDataManager.h>
#include <MYGUI/MyGUI_OpenGLPlatform.h>
#include <MYGUI/MyGUI_LayerManager.h>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>
#include <algorithm>
#include <Texture.h>

void* GameGUIImageLoader::loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename)
{
	// MyGUI was originally failing to load our UI skin assets because it needed a
	// loader that understood the engine's image path and stb_image wrapper.
	// This loader converts disk files into raw RGBA pixels for MyGUI's OpenGL backend.
	try
	{
		StbImage image;
		image.loadFromFile(_filename);

		_width = image.getWidth();
		_height = image.getHeight();

		// Force 4 channels so the upload path is predictable. The earlier format
		// mismatch was one reason the GUI assets were not behaving correctly.
		_format = MyGUI::PixelFormat::R8G8B8A8;

		const std::size_t byteCount = static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height) * 4u;
		unsigned char* pixels = new unsigned char[byteCount];
		std::copy_n(image.getData(), byteCount, pixels);

		return pixels;
	}
	catch (const std::exception& e)
	{
		// If the image load fails, MyGUI needs a clean failure instead of partial data.
		_width = 0;
		_height = 0;
		_format = MyGUI::PixelFormat::Unknow;
		return nullptr;
	}
}

void GameGUIImageLoader::saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename)
{
	// Saving is intentionally a no-op for now. The integration problem we had was
	// read-side only, so this stays as a stub until the editor can export UI assets.
	(void)_width;
	(void)_height;
	(void)_format;
	(void)_texture;
	(void)_filename;
}

void GameGUI::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;

	// MyGUI's OpenGL backend needs a platform object plus an image loader before the
	// main Gui singleton can initialize. Earlier crashes came from creating Gui
	// before the platform and resource paths were ready.
	try
	{
		m_platform = new MyGUI::OpenGLPlatform();
		m_platform->initialise(&m_imageLoader);
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		window.GetFramebufferSize(framebufferWidth, framebufferHeight);
		m_platform->getRenderManagerPtr()->setViewSize(framebufferWidth, framebufferHeight);
		// MyGUI resolves XML resources through its data manager. Registering the build
		// output root lets it find the copied MyGUI media folder and the XML skin files
		// referenced by the test button.
		MyGUI::OpenGLDataManager::getInstance().addResourceLocation(".", true);

		// Gui has to exist only after the platform and resources are available. That
		// ordering fixed the runtime exceptions we saw during the first integration pass.
		m_gui = new MyGUI::Gui();
		m_gui->initialise();

		// Create one visible widget as a smoke test. We used a known skin from the
		// bundled MyGUI media so the test was about rendering, not asset authoring.
		const int buttonWidth = 640;
		const int buttonHeight = 180;
		const int buttonLeft = 24;
		const int buttonTop = 24;
		m_testButton = m_gui->createWidget<MyGUI::Button>(
			// Use a real bundled MyGUI skin name from MyGUI_BlueWhiteSkins.xml.
			"ButtonSkin",
			buttonLeft,
			buttonTop,
			buttonWidth,
			buttonHeight,
			MyGUI::Align::Default,
			"Main",
			"TestButton");
		if (m_testButton)
		{
			m_testButton->setCaption("Test Button");
			m_testButton->setVisible(true);
			m_testButton->setAlpha(1.0f);
			// The button originally existed but still did not show up; pushing it onto the
			// correct layer made the widget participate in the render submission path.
			MyGUI::LayerManager::getInstance().upLayerItem(m_testButton);
		}
		m_initialized = true;
	}
	catch (const std::exception& e)
	{
		if (m_gui)
		{
			m_gui->shutdown();
			delete m_gui;
			m_gui = nullptr;
		}
		if (m_platform)
		{
			m_platform->shutdown();
			delete m_platform;
			m_platform = nullptr;
		}
		m_window = nullptr;
		m_initialized = false;
		throw;
	}
}

void GameGUI::shutDown()
{
	if (m_initialized)
	{
		if (m_testButton)
		{
			// Widgets are owned by Gui; destroy them before tearing down the backend.
			m_gui->destroyWidget(m_testButton);
			m_testButton = nullptr;
		}

		// Reverse startup order to avoid dangling MyGUI objects during shutdown.
		m_gui->shutdown();
		delete m_gui;
		m_gui = nullptr;
		if (m_platform)
		{
			m_platform->shutdown();
			delete m_platform;
			m_platform = nullptr;
		}
	}
	m_window = nullptr;
	m_initialized = false;
}

void GameGUI::BeginFrame()
{
}

void GameGUI::Draw()
{
	if (!m_initialized)
	{
		return;
	}

	// MyGUI needs a per-frame tick so internal widget state and input-driven updates
	// advance before the renderer submits the overlay.
	m_gui->frameEvent(0.0f);
	// The final bug was not in MyGUI itself, but in inherited scene render state.
	// The GUI pass must start from a clean overlay-friendly OpenGL state.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Clear 3D state that leaked in from the scene pass. MyGUI's OpenGL renderer
	// expects to control its own simple overlay pipeline.
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	m_platform->getRenderManagerPtr()->drawOneFrame();
}

void GameGUI::EndFrame()
{
}
