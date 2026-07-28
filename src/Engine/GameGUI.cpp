#include "Engine/GameGUI.h"

#include "Engine/Debug.h"
#include "Engine/FrontEndManager.h"
#include "Engine/EventManager.h"
#include "Engine/Globals.h"
#include "Engine/GameplayManager.h"
#include "Engine/Window.h"
#include "Engine/StbImage.h"
#include "Engine/GLHeaders.h"
#include "Engine/GameGUIAsset.h"
#include "Engine/FileSystem.h"
#include "Engine/Level.h"
#include "Engine/LevelManager.h"

#include <MYGUI/MyGUI_Button.h>
#include <MYGUI/MyGUI_Gui.h>
#include <MYGUI/MyGUI_ImageBox.h>
#include <MYGUI/MyGUI_TextBox.h>
#include <MYGUI/MyGUI_OpenGLDataManager.h>
#include <MYGUI/MyGUI_OpenGLPlatform.h>
#include <MYGUI/MyGUI_LayerManager.h>
#include <MYGUI/MyGUI_PointerManager.h>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>
#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <Engine/Texture.h>

namespace {
	std::filesystem::path ResolveGameGUIImagePath(const std::string& filename)
	{
		const std::filesystem::path requestedPath(filename);
		std::error_code ec;
		if (requestedPath.is_absolute() && std::filesystem::exists(requestedPath, ec) && !ec)
		{
			return requestedPath;
		}

		const std::filesystem::path executableRoot = gFileSystem.ExecutableDirectory();
		const std::filesystem::path candidatePaths[] = {
			requestedPath,
			executableRoot / requestedPath,
			executableRoot / "assets" / requestedPath,
#ifdef AQUANACT_SOURCE_ROOT
			std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / requestedPath,
#endif
		};

		for (const std::filesystem::path& candidate : candidatePaths)
		{
			ec.clear();
			if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !ec)
			{
				return candidate;
			}
		}

		return requestedPath;
	}

	const GameGUIWidgetDef* FindWidgetDef(const GameGUIAsset& asset, const std::string& name)
	{
		for (const auto& widget : asset.widgets)
		{
			if (widget.name == name)
			{
				return &widget;
			}
		}
		return nullptr;
	}

	Component* ResolveBoundComponent(const GameGUIWidgetDef& def)
	{
		Level* activeLevel = gLevelManager.ActiveLevel();
		if (!activeLevel)
		{
			return nullptr;
		}

		for (const auto& entity : activeLevel->Entities())
		{
			if (entity && entity->Name() == def.bindEntity)
			{
				return entity->GetComponentByName(def.bindComponent);
			}
		}
		return nullptr;
	}
}

void* GameGUIImageLoader::loadImage(int& _width, int& _height, MyGUI::PixelFormat& _format, const std::string& _filename)
{
	// MyGUI was originally failing to load our UI skin assets because it needed a
	// loader that understood the engine's image path and stb_image wrapper.
	// This loader converts disk files into raw RGBA pixels for MyGUI's OpenGL backend.
	try
	{
		StbImage image;
		const std::filesystem::path resolvedPath = ResolveGameGUIImagePath(_filename);
		image.loadFromFile(resolvedPath.string());

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
		gDebug.LogMessage("GameGUI image load failed: requested='" + _filename + "', reason='" + e.what() + "'");
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
		// MyGUI resolves XML resources through its data manager. Register the build
		// output root without recursion so it can find the copied XML/PNG skin files
		// without scanning nested build-tree copies under vcpkg.
		const std::filesystem::path resourceRoot = gFileSystem.ExecutableDirectory();
		MyGUI::OpenGLDataManager& dataManager = MyGUI::OpenGLDataManager::getInstance();
		dataManager.addResourceLocation(resourceRoot.string(), false);
		dataManager.addResourceLocation((resourceRoot / "assets").string(), true);
#ifdef AQUANACT_SOURCE_ROOT
		dataManager.addResourceLocation((std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets").string(), true);
#endif

		// Gui has to exist only after the platform and resources are available. That
		// ordering fixed the runtime exceptions we saw during the first integration pass.
		m_gui = new MyGUI::Gui();
		m_gui->initialise();
		MyGUI::PointerManager::getInstance().setVisible(false);
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
		ClearUI();

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
	// The final bug was not in MyGUI itself, but in inherited level render state.
	// The GUI pass must start from a clean overlay-friendly OpenGL state.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Clear 3D state that leaked in from the level pass. MyGUI's OpenGL renderer
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

void GameGUI::LoadUIAsset(const GameGUIAsset& asset)
{
	if (!m_initialized || !m_gui)
	{
		return;
	}

	ClearUI();
	m_loadedAsset = asset;
	m_runtimeWidgetLookup.clear();
	std::unordered_map<std::string, MyGUI::Widget*> createdWidgets;
	for (const GameGUIWidgetDef& widget : m_loadedAsset.widgets)
	{
		if (!widget.parentName.empty())
		{
			continue;
		}

		MyGUI::Widget* createdWidget = CreateWidgetFromDef(widget, nullptr);
		if (createdWidget)
		{
			createdWidgets[widget.name] = createdWidget;
			m_runtimeWidgets.push_back(createdWidget);
			m_runtimeWidgetLookup[widget.name] = createdWidget;
		}
	}

	bool madeProgress = true;
	while (madeProgress)
	{
		madeProgress = false;
		for (const GameGUIWidgetDef& widget : m_loadedAsset.widgets)
		{
			if (widget.parentName.empty() || createdWidgets.find(widget.name) != createdWidgets.end())
			{
				continue;
			}

			const auto parentIt = createdWidgets.find(widget.parentName);
			if (parentIt == createdWidgets.end())
			{
				continue;
			}

			MyGUI::Widget* createdWidget = CreateWidgetFromDef(widget, parentIt->second);
			if (createdWidget)
			{
				createdWidgets[widget.name] = createdWidget;
				m_runtimeWidgetLookup[widget.name] = createdWidget;
				madeProgress = true;
			}
		}
	}

	for (const GameGUIWidgetDef& widget : m_loadedAsset.widgets)
	{
		if (!widget.parentName.empty() && createdWidgets.find(widget.name) == createdWidgets.end())
		{
			gDebug.LogMessage("GameGUI skipped child widget with missing/cyclic parent: " + widget.name);
		}
	}

	for (const GameGUIWidgetDef& widget : m_loadedAsset.widgets)
	{
		if (!widget.bindEvent.empty())
		{
			auto it = m_runtimeWidgetLookup.find(widget.name);
			if (it != m_runtimeWidgetLookup.end())
			{
				BindWidgetFromDef(widget, it->second);
			}
		}
	}
}

void GameGUI::ClearUI()
{
	for (const auto& [name, widget] : m_runtimeWidgetLookup)
	{
		(void)name;
		if (widget)
		{
			gEventManager.Unsubscribe(widget);
		}
	}

	if (!m_gui)
	{
		m_runtimeWidgets.clear();
		m_runtimeWidgetLookup.clear();
		return;
	}

	for (MyGUI::Widget* widget : m_runtimeWidgets)
	{
		if (widget)
		{
			m_gui->destroyWidget(widget);
		}
	}
	m_runtimeWidgets.clear();
	m_runtimeWidgetLookup.clear();
}

MyGUI::Widget* GameGUI::CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	if (def.type == "Button")
	{
		MyGUI::Button* button = parent ?
			parent->createWidget<MyGUI::Button>(def.skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.name) :
			m_gui->createWidget<MyGUI::Button>(def.skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.layer, def.name);
		if (button)
		{
			button->setCaption(def.text);
			button->setVisible(def.visible);
			button->setAlpha(def.alpha);
			// Button clicks are routed back into GameGUI so we can attach small
			// built-in behaviors without hardcoding them into the widget assets.
			button->eventMouseButtonClick += MyGUI::newDelegate(this, &GameGUI::OnWidgetClicked);
			gDebug.LogMessage(std::string("GameGUI click handler bound for widget: ") + def.name);
			if (!parent)
			{
				MyGUI::LayerManager::getInstance().upLayerItem(button);
			}
			gDebug.LogMessage(
				std::string("GameGUI widget created: name='") + def.name +
				"', type='" + def.type +
				"', skin='" + def.skin +
				"', layer='" + def.layer + "'");
			return button;
		}
	}
	else if (def.type == "TextBox" || def.type == "Text")
	{
		const std::string skin = def.skin.empty() ? "TextBox" : def.skin;
		MyGUI::TextBox* text = parent ?
			parent->createWidget<MyGUI::TextBox>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.name) :
			m_gui->createWidget<MyGUI::TextBox>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.layer, def.name);
		if (text)
		{
			text->setCaption(def.text);
			if (def.fontSize > 0)
			{
				text->setFontHeight(def.fontSize);
			}
			text->setVisible(def.visible);
			text->setAlpha(def.alpha);
			if (!parent)
			{
				MyGUI::LayerManager::getInstance().upLayerItem(text);
			}
			gDebug.LogMessage(
				std::string("GameGUI widget created: name='") + def.name +
				"', type='" + def.type +
				"', skin='" + def.skin +
				"', layer='" + def.layer + "'");
			return text;
		}
	}
	else if (def.type == "ImageBox" || def.type == "Image")
	{
		const std::string skin = def.skin.empty() ? "ImageBox" : def.skin;
		MyGUI::ImageBox* image = parent ?
			parent->createWidget<MyGUI::ImageBox>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.name) :
			m_gui->createWidget<MyGUI::ImageBox>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.layer, def.name);
		if (image)
		{
			if (!def.texture.empty())
			{
				image->setImageTexture(def.texture);
			}
			image->setVisible(def.visible);
			image->setAlpha(def.alpha);
			if (!parent)
			{
				MyGUI::LayerManager::getInstance().upLayerItem(image);
			}
			gDebug.LogMessage(
				std::string("GameGUI widget created: name='") + def.name +
				"', type='" + def.type +
				"', skin='" + def.skin +
				"', layer='" + def.layer + "'");
			return image;
		}
	}

	return nullptr;
}

void GameGUI::BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget)
{
	if (!widget || def.bindEntity.empty() || def.bindComponent.empty() || def.bindEvent.empty())
	{
		return;
	}

	auto* textWidget = dynamic_cast<MyGUI::TextBox*>(widget);
	if (!textWidget)
	{
		gDebug.LogMessage("GameGUI binding ignored for non-text widget: " + def.name);
		return;
	}

	Component* component = ResolveBoundComponent(def);
	if (!component)
	{
		gDebug.LogMessage(
			"GameGUI binding could not resolve component: " +
			def.bindEntity + "." + def.bindComponent);
		return;
	}

	const std::vector<BindableEvent> bindableEvents = component->GetBindableEvents();
	const bool exposesEvent = std::any_of(bindableEvents.begin(), bindableEvents.end(), [&def](const BindableEvent& event)
	{
		return event.name == def.bindEvent;
	});
	if (!exposesEvent)
	{
		gDebug.LogMessage(
			"GameGUI binding event is not exposed by component: " +
			def.bindEntity + "." + def.bindComponent + "." + def.bindEvent);
		return;
	}

	textWidget->setCaption(component->GetBindableEventText(def.bindEvent));
	const std::string channel = component->BindableEventChannel(def.bindEvent);
	gEventManager.GetEvent(channel).Subscribe(widget, [widget, binding = def]()
	{
		Component* currentComponent = ResolveBoundComponent(binding);
		auto* currentTextWidget = dynamic_cast<MyGUI::TextBox*>(widget);
		if (!currentComponent || !currentTextWidget)
		{
			return;
		}
		currentTextWidget->setCaption(currentComponent->GetBindableEventText(binding.bindEvent));
	});
	gDebug.LogMessage("GameGUI bound widget '" + def.name + "' to " + channel);
}

void GameGUI::OnWidgetClicked(MyGUI::Widget* sender)
{
	if (!sender)
	{
		return;
	}

	const std::string name = sender->getName();
	const GameGUIWidgetDef* def = FindWidgetDef(m_loadedAsset, name);
	const GameGUIActionType action = def ? def->action : GameGUIActionType::None;
	gDebug.LogMessage(std::string("GameGUI click received for widget: ") + (name.empty() ? "<unnamed>" : name));
	gFrontEndManager.RuntimeGUI().RecordClick("Clicked widget: " + (name.empty() ? std::string("<unnamed>") : name));
	switch (action)
	{
	case GameGUIActionType::QuitGame:
		gFrontEndManager.RuntimeGUI().RecordClick("Quit action requested");
		gDebug.LogMessage("GameGUI Quit action requested");
		if (m_window && m_window->GLFW())
		{
			gDebug.LogMessage("GameGUI requested window close via Quit button");
			gFrontEndManager.RuntimeGUI().RecordClick("Window close requested");
			glfwSetWindowShouldClose(m_window->GLFW(), GLFW_TRUE);
		}
		break;
	case GameGUIActionType::PauseGame:
		gFrontEndManager.RuntimeGUI().RecordClick("Pause Game action requested");
		gDebug.LogMessage("GameGUI PauseGame action requested");
		gGameplayManager.TogglePaused();
		gFrontEndManager.RuntimeGUI().RecordClick(gGameplayManager.IsPaused() ? "Gameplay paused" : "Gameplay resumed");
		break;
	case GameGUIActionType::None:
	default:
		break;
	}
}



