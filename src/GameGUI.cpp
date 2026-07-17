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
#include <iostream>
#include <sstream>
#include <Texture.h>

namespace {
	std::string BoolText(GLboolean value)
	{
		return value == GL_TRUE ? "enabled" : "disabled";
	}
}

void* GameGUIImageLoader::loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename)
{
	// MyGUI calls into this loader whenever it needs to turn an image file on disk
	// into raw pixels. That usually happens for button skins, fonts, and other GUI
	// textures. The loader's job is to decode the file, not to create the OpenGL
	// texture itself.
	try
	{
		StbImage image;
		image.loadFromFile(_filename);

		_width = image.getWidth();
		_height = image.getHeight();

		// We always ask stb_image for 4 channels, so the CPU buffer is RGBA8.
		// MyGUI's OpenGL backend understands this format and can upload it directly.
		_format = MyGUI::PixelFormat::R8G8B8A8;

		const std::size_t byteCount = static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height) * 4u;
		unsigned char* pixels = new unsigned char[byteCount];
		std::copy_n(image.getData(), byteCount, pixels);

		return pixels;
	}
	catch (const std::exception& e)
	{
		_width = 0;
		_height = 0;
		_format = MyGUI::PixelFormat::Unknow;
		return nullptr;
	}
}

void GameGUIImageLoader::saveImage(int _width, int _height, MyGUI::PixelFormat _format, void* _texture, const std::string& _filename)
{
	// We do not need the save path for runtime UI loading yet.
	// MyGUI keeps this API around for completeness, but this engine only needs
	// to load image assets, not export them from the GUI system.
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
	// main Gui singleton can initialize. The platform owns the OpenGL render/data
	// managers internally, while the image loader is the bridge from filenames to raw
	// pixel buffers.
	try
	{
		m_platform = new MyGUI::OpenGLPlatform();
		m_platform->initialise(&m_imageLoader);
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		window.GetFramebufferSize(framebufferWidth, framebufferHeight);
		m_platform->getRenderManagerPtr()->setViewSize(framebufferWidth, framebufferHeight);
		// MyGUI resolves XML resources through its data manager. The media files are
		// copied into the build output directory, so registering "." lets MyGUI find
		// MyGUI_Core.xml and the skin/font/layer files it references.
		MyGUI::OpenGLDataManager::getInstance().addResourceLocation(".", true);

		// Once the backend exists, MyGUI can load its XML skin/font/layer configuration
		// and create widgets.
		// Gui is a MyGUI singleton, but it still needs a real instance constructed
		// before getInstance() is valid.
		m_gui = new MyGUI::Gui();
		m_gui->initialise();

		// Create one simple widget so we can verify the runtime UI path is live.
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
			MyGUI::LayerManager::getInstance().upLayerItem(m_testButton);
		}
		if (!m_loggedStartupState)
		{
			gDebug.LogMessage(
				"MyGUI startup: framebuffer=" + std::to_string(framebufferWidth) + "x" + std::to_string(framebufferHeight) +
				", viewSize set, button=" + std::string(m_testButton ? "created" : "missing") +
				", layer=Main");
			m_loggedStartupState = true;
		}
		m_initialized = true;
		m_loggedRenderSubmission = false;
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
			// Destroy widgets before shutting down the GUI manager that owns them.
			m_gui->destroyWidget(m_testButton);
			m_testButton = nullptr;
		}

		// Shut down in the reverse order of startup: Gui first, then the platform backend.
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

	// MyGUI expects a per-frame tick so animations, input-driven state, and internal
	// widget updates can advance. For now the elapsed time is 0 because this is a
	// minimal integration stub.
	if (!m_loggedRenderSubmission)
	{
		gDebug.LogMessage("MyGUI render submission: entering frameEvent.");
	}
	m_gui->frameEvent(0.0f);
	if (!m_loggedRenderSubmission)
	{
		gDebug.LogMessage("MyGUI render submission: entering drawOneFrame().");
	}
	if (!m_loggedRenderSubmission)
	{
		gDebug.LogMessage("MyGUI render submission: draw call about to execute on active GL context.");
		GLboolean depthTest = GL_FALSE;
		GLboolean blend = GL_FALSE;
		GLboolean cullFace = GL_FALSE;
		GLboolean scissorTest = GL_FALSE;
		GLboolean stencilTest = GL_FALSE;
		GLboolean multisample = GL_FALSE;
		GLint viewport[4] = { 0, 0, 0, 0 };
		GLint program = 0;
		GLint arrayBuffer = 0;
		GLint elementArrayBuffer = 0;
		GLint activeTexture = 0;
		GLint boundTexture2D = 0;
		GLint depthFunc = 0;
		GLint blendSrcRgb = 0;
		GLint blendDstRgb = 0;
		GLint blendSrcAlpha = 0;
		GLint blendDstAlpha = 0;
		GLint polygonMode[2] = { 0, 0 };

		glGetBooleanv(GL_DEPTH_TEST, &depthTest);
		glGetBooleanv(GL_BLEND, &blend);
		glGetBooleanv(GL_CULL_FACE, &cullFace);
		glGetBooleanv(GL_SCISSOR_TEST, &scissorTest);
		glGetBooleanv(GL_STENCIL_TEST, &stencilTest);
		glGetBooleanv(GL_MULTISAMPLE, &multisample);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture2D);
		glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
		glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
		glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
		glGetIntegerv(GL_POLYGON_MODE, polygonMode);

		std::ostringstream state;
		state
			<< "MyGUI GL state: depth=" << BoolText(depthTest)
			<< ", blend=" << BoolText(blend)
			<< ", cull=" << BoolText(cullFace)
			<< ", scissor=" << BoolText(scissorTest)
			<< ", stencil=" << BoolText(stencilTest)
			<< ", multisample=" << BoolText(multisample)
			<< ", viewport=" << viewport[0] << "," << viewport[1] << "," << viewport[2] << "," << viewport[3]
			<< ", program=" << program
			<< ", arrayBuffer=" << arrayBuffer
			<< ", elementArrayBuffer=" << elementArrayBuffer
			<< ", activeTexture=" << activeTexture
			<< ", tex2D=" << boundTexture2D
			<< ", depthFunc=" << depthFunc
			<< ", blendRGB=(" << blendSrcRgb << "," << blendDstRgb << ")"
			<< ", blendA=(" << blendSrcAlpha << "," << blendDstAlpha << ")"
			<< ", polygonMode=(" << polygonMode[0] << "," << polygonMode[1] << ")";
		std::cout << state.str() << std::endl;
	}
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	m_platform->getRenderManagerPtr()->drawOneFrame();
	if (!m_loggedRenderSubmission)
	{
		gDebug.LogMessage("MyGUI render submission: drawOneFrame() returned.");
	}
	m_loggedRenderSubmission = true;
}

void GameGUI::EndFrame()
{
}
