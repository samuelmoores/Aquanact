#include "Engine/UI/GameGUI.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/StbImage.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/UI/GameGUIAsset.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <MYGUI/MyGUI_Button.h>
#include <MYGUI/MyGUI_Colour.h>
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
#include <iostream>
#include <unordered_map>
#include "Engine/Core/Texture.h"

namespace {
	float ReadProgressPercent(const GameGUIWidgetDef& def)
	{
		if (def.bindEntity.empty() || def.bindComponent.empty() || def.bindMember.empty())
		{
			return 0.0f;
		}

		Scene* activeLevel = Root::Current().Levels().ActiveLevel();
		if (!activeLevel)
		{
			return 0.0f;
		}

		for (const auto& entity : activeLevel->Entities())
		{
			if (!entity || entity->Name() != def.bindEntity)
			{
				continue;
			}

			Component* component = entity->GetComponentByName(def.bindComponent);
			if (!component)
			{
				return 0.0f;
			}

			float value = 0.0f;
			if (!component->TryGetBindableValue(def.bindMember, value))
			{
				return 0.0f;
			}
			return std::clamp(value, 0.0f, 100.0f) / 100.0f;
		}

		return 0.0f;
	}

	std::filesystem::path ResolveGameGUIImagePath(const std::string& filename)
	{
		const std::filesystem::path requestedPath(filename);
		std::error_code ec;
		if (requestedPath.is_absolute() && std::filesystem::exists(requestedPath, ec) && !ec)
		{
			return requestedPath;
		}

		const std::filesystem::path executableRoot = Root::Current().FileSystemRef().ExecutableDirectory();
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

	Scene* FindPlayableScene(SceneManager& sceneManager)
	{
		for (const auto& scene : sceneManager.Levels())
		{
			if (scene && sceneManager.SceneKindFor(scene->Name()) == SceneManager::SceneKind::Level)
			{
				return scene.get();
			}
		}
		return nullptr;
	}

	Scene* FindNamedLevel(SceneManager& sceneManager, const std::string& levelName)
	{
		if (levelName.empty())
		{
			return nullptr;
		}
		Scene* scene = sceneManager.FindLevel(levelName);
		if (scene && sceneManager.SceneKindFor(scene->Name()) == SceneManager::SceneKind::Level)
		{
			return scene;
		}
		return nullptr;
	}

	Component* ResolveBoundComponent(const GameGUIWidgetDef& def)
	{
		Scene* activeLevel = Root::Current().Levels().ActiveLevel();
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

	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback)
	{
		try
		{
			if (value.empty())
			{
				return fallback;
			}
			return MyGUI::Colour(value);
		}
		catch (...)
		{
			return fallback;
		}
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

		const std::size_t pixelCount = static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height);
		const std::size_t byteCount = pixelCount * 4u;
		unsigned char* pixels = new unsigned char[byteCount];
		const unsigned char* source = image.getData();
		for (std::size_t pixel = 0; pixel < pixelCount; ++pixel)
		{
			// stb_image returns RGBA, while MyGUI's OpenGL R8G8B8A8 upload path
			// expects BGRA data because it uses GL_BGRA as the source format.
			pixels[pixel * 4u + 0u] = source[pixel * 4u + 2u];
			pixels[pixel * 4u + 1u] = source[pixel * 4u + 1u];
			pixels[pixel * 4u + 2u] = source[pixel * 4u + 0u];
			pixels[pixel * 4u + 3u] = source[pixel * 4u + 3u];
		}

		return pixels;
	}
	catch (const std::exception& e)
	{
		// If the image load fails, MyGUI needs a clean failure instead of partial data.
		Root::Current().Debugger().LogMessage("GameGUI image load failed: requested='" + _filename + "', reason='" + e.what() + "'");
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
		const std::filesystem::path resourceRoot = Root::Current().FileSystemRef().ExecutableDirectory();
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

	for (const GameGUIWidgetDef& widget : m_loadedAsset.widgets)
	{
		if (widget.type != "ProgressBar")
		{
			continue;
		}

		auto it = m_runtimeWidgetLookup.find(widget.name);
		if (it == m_runtimeWidgetLookup.end())
		{
			continue;
		}

		auto* fillImage = dynamic_cast<MyGUI::ImageBox*>(it->second);
		if (!fillImage)
		{
			continue;
		}

		const float percent = ReadProgressPercent(widget);
		const int filledWidth = percent <= 0.0f
			? 0
			: std::max(1, static_cast<int>(std::lround(static_cast<float>(widget.width) * percent)));
		fillImage->setSize(MyGUI::IntSize(filledWidth, widget.height));
	}

	// MyGUI needs a per-frame tick so internal widget state and input-driven updates
	// advance before the renderer submits the overlay.
	m_gui->frameEvent(0.0f);
	// The final bug was not in MyGUI itself, but in inherited Scene render state.
	// The GUI pass must start from a clean overlay-friendly OpenGL state.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Clear 3D state that leaked in from the Scene pass. MyGUI's OpenGL renderer
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
			Root::Current().Debugger().LogMessage("GameGUI skipped child widget with missing/cyclic parent: " + widget.name);
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

	std::stable_sort(m_controllerButtons.begin(), m_controllerButtons.end(), [this](const MyGUI::Button* left, const MyGUI::Button* right)
	{
		const GameGUIWidgetDef* leftDef = FindWidgetDef(m_loadedAsset, left->getName());
		const GameGUIWidgetDef* rightDef = FindWidgetDef(m_loadedAsset, right->getName());
		const int leftY = leftDef ? leftDef->y : 0;
		const int rightY = rightDef ? rightDef->y : 0;
		if (leftY != rightY)
		{
			return leftY < rightY;
		}

		const int leftX = leftDef ? leftDef->x : 0;
		const int rightX = rightDef ? rightDef->x : 0;
		if (leftX != rightX)
		{
			return leftX < rightX;
		}
		return left->getName() < right->getName();
	});

	// Keep the navigation pointer separate from the asset so it does not affect
	// layout editing or become an interactive widget in the menu.
	m_menuPointer = m_gui->createWidget<MyGUI::TextBox>(
		"TextBox", 0, 0, 40, 40, MyGUI::Align::Default, "Pointer", "__GameGUIMenuPointer");
	if (m_menuPointer)
	{
		m_menuPointer->setCaption(">");
		m_menuPointer->setFontHeight(32);
		m_menuPointer->setTextColour(MyGUI::Colour::White);
		m_menuPointer->setTextAlign(MyGUI::Align::Center);
		m_menuPointer->setNeedMouseFocus(false);
		m_menuPointer->setNeedKeyFocus(false);
		m_menuPointer->setInheritsPick(false);
		m_menuPointer->setVisible(false);
		MyGUI::LayerManager::getInstance().upLayerItem(m_menuPointer);
	}
}

void GameGUI::ClearUI()
{
	for (const auto& [name, widget] : m_runtimeWidgetLookup)
	{
		(void)name;
		if (widget)
		{
			Root::Current().Events().Unsubscribe(widget);
		}
	}

	if (!m_gui)
	{
		m_menuPointer = nullptr;
		m_runtimeWidgets.clear();
		m_controllerButtons.clear();
		m_runtimeWidgetLookup.clear();
		m_focusedControllerButton = -1;
		return;
	}

	for (MyGUI::Widget* widget : m_runtimeWidgets)
	{
		if (widget)
		{
			m_gui->destroyWidget(widget);
		}
	}
	if (m_menuPointer)
	{
		m_gui->destroyWidget(m_menuPointer);
	}
	m_menuPointer = nullptr;
	m_runtimeWidgets.clear();
	m_controllerButtons.clear();
	m_runtimeWidgetLookup.clear();
	m_focusedControllerButton = -1;
}

MyGUI::Widget* GameGUI::CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	if (def.type == "Button")
	{
		const std::string skin = def.skin.empty() ? "Button" : def.skin;
		MyGUI::Button* button = parent ?
			parent->createWidget<MyGUI::Button>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.name) :
			m_gui->createWidget<MyGUI::Button>(skin, def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.layer, def.name);
		if (button)
		{
			button->setCaption(def.text.empty() ? def.name : def.text);
			button->setVisible(def.visible);
			button->setAlpha(def.alpha);
			button->setColour(ParseColour(def.highlightColor, MyGUI::Colour::White));
			button->setStateSelected(false);
			button->eventMouseSetFocus += MyGUI::newDelegate(this, &GameGUI::OnButtonMouseFocus);
			button->eventMouseLostFocus += MyGUI::newDelegate(this, &GameGUI::OnButtonMouseLostFocus);
			if (def.visible)
			{
				m_controllerButtons.push_back(button);
			}
			MyGUI::TextBox* label = button->createWidget<MyGUI::TextBox>("TextBox", 0, 0, def.width, def.height, MyGUI::Align::Default, def.name + "_label");
			if (label)
			{
				label->setCaption(def.text);
				if (def.fontSize > 0)
				{
					label->setFontHeight(def.fontSize);
				}
				label->setNeedMouseFocus(false);
				label->setNeedKeyFocus(false);
				label->setInheritsPick(false);
				label->setTextAlign(MyGUI::Align::Center);
				label->setTextColour(MyGUI::Colour::Black);
				label->setVisible(def.visible);
				label->setAlpha(def.alpha);
			}
			// Button clicks are routed back into GameGUI so we can attach small
			// built-in behaviors without hardcoding them into the widget assets.
			button->eventMouseButtonClick += MyGUI::newDelegate(this, &GameGUI::OnWidgetClicked);
			Root::Current().Debugger().LogMessage(std::string("GameGUI click handler bound for widget: ") + def.name);
			if (!parent)
			{
				MyGUI::LayerManager::getInstance().upLayerItem(button);
			}
			Root::Current().Debugger().LogMessage(
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
			Root::Current().Debugger().LogMessage(
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
			Root::Current().Debugger().LogMessage(
				std::string("GameGUI widget created: name='") + def.name +
				"', type='" + def.type +
				"', skin='" + def.skin +
				"', layer='" + def.layer + "'");
			return image;
		}
	}
	else if (def.type == "ProgressBar")
	{
		MyGUI::ImageBox* progress = parent ?
			parent->createWidget<MyGUI::ImageBox>("ImageBox", def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.name) :
			m_gui->createWidget<MyGUI::ImageBox>("ImageBox", def.x, def.y, def.width, def.height, MyGUI::Align::Default, def.layer, def.name);
		if (progress)
		{
			if (!def.texture.empty())
			{
				progress->setImageTexture(def.texture);
			}
			progress->setVisible(def.visible);
			progress->setAlpha(def.alpha);
			if (!parent)
			{
				MyGUI::LayerManager::getInstance().upLayerItem(progress);
			}
			return progress;
		}
	}

	return nullptr;
}

void GameGUI::FocusFirstControllerButton()
{
	if (m_controllerButtons.empty())
	{
		return;
	}

	ClearControllerFocus();
	m_focusedControllerButton = 0;
	PositionMenuPointer(m_controllerButtons[0]);
	m_controllerButtons[0]->_setMouseFocus(true);
}

void GameGUI::ClearControllerFocus()
{
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()))
	{
		m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]->_setMouseFocus(false);
	}
	if (m_menuPointer)
	{
		m_menuPointer->setVisible(false);
	}
	m_focusedControllerButton = -1;
}

bool GameGUI::HasControllerFocus() const
{
	return m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size());
}

void GameGUI::NavigateControllerButtons(int direction)
{
	if (m_controllerButtons.empty() || direction == 0)
	{
		return;
	}
	if (!HasControllerFocus())
	{
		FocusFirstControllerButton();
		return;
	}

	const int count = static_cast<int>(m_controllerButtons.size());
	const int previous = m_focusedControllerButton;
	m_focusedControllerButton = (previous + direction + count) % count;
	m_controllerButtons[static_cast<std::size_t>(previous)]->_setMouseFocus(false);
	PositionMenuPointer(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
	m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]->_setMouseFocus(true);
}

void GameGUI::ActivateFocusedControllerButton()
{
	if (HasControllerFocus())
	{
		OnWidgetClicked(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
	}
}

void GameGUI::OnButtonMouseFocus(MyGUI::Widget* sender, MyGUI::Widget*)
{
	PositionMenuPointer(sender);
}

void GameGUI::OnButtonMouseLostFocus(MyGUI::Widget* sender, MyGUI::Widget*)
{
	if (m_focusedControllerButton < 0 && m_menuPointer)
	{
		m_menuPointer->setVisible(false);
	}
}

void GameGUI::PositionMenuPointer(MyGUI::Widget* button)
{
	if (!m_menuPointer || !button)
	{
		return;
	}

	const MyGUI::IntCoord buttonCoord = button->getAbsoluteCoord();
	constexpr int pointerWidth = 40;
	constexpr int pointerHeight = 40;
	constexpr int gap = 24;
	const int pointerX = std::max(0, buttonCoord.left - pointerWidth - gap);
	const int pointerY = buttonCoord.top + (buttonCoord.height - pointerHeight) / 2;
	m_menuPointer->setCoord(pointerX, pointerY, pointerWidth, pointerHeight);
	m_menuPointer->setVisible(true);
	MyGUI::LayerManager::getInstance().upLayerItem(m_menuPointer);
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
		Root::Current().Debugger().LogMessage("GameGUI binding ignored for non-text widget: " + def.name);
		return;
	}

	Component* component = ResolveBoundComponent(def);
	if (!component)
	{
		Root::Current().Debugger().LogMessage(
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
		Root::Current().Debugger().LogMessage(
			"GameGUI binding event is not exposed by component: " +
			def.bindEntity + "." + def.bindComponent + "." + def.bindEvent);
		return;
	}

	textWidget->setCaption(component->GetBindableEventText(def.bindEvent));
	const std::string channel = component->BindableEventChannel(def.bindEvent);
	Root::Current().Events().GetEvent(channel).Subscribe(widget, [widget, binding = def]()
	{
		Component* currentComponent = ResolveBoundComponent(binding);
		auto* currentTextWidget = dynamic_cast<MyGUI::TextBox*>(widget);
		if (!currentComponent || !currentTextWidget)
		{
			return;
		}
		currentTextWidget->setCaption(currentComponent->GetBindableEventText(binding.bindEvent));
	});
	Root::Current().Debugger().LogMessage("GameGUI bound widget '" + def.name + "' to " + channel);
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
	const std::string launchLevel = def ? def->launchLevel : std::string{};
	Root::Current().Debugger().LogMessage(std::string("GameGUI click received for widget: ") + (name.empty() ? "<unnamed>" : name));
	Root::Current().Debugger().LogMessage("GameGUI button action=" + std::string(def ? GameGUICreatorHelpers::ActionToString(action) : "None") + ", launchLevel=" + (launchLevel.empty() ? std::string("<none>") : launchLevel));
	std::cout << "GameGUI click received for widget: " << (name.empty() ? "<unnamed>" : name) << '\n';
	std::cout << "GameGUI button action=" << (def ? GameGUICreatorHelpers::ActionToString(action) : "None")
		<< ", launchLevel=" << (launchLevel.empty() ? "<none>" : launchLevel) << '\n';
	Root::Current().FrontEnd().RuntimeGUI().RecordClick("Clicked widget: " + (name.empty() ? std::string("<unnamed>") : name));
	Root::Current().FrontEnd().RuntimeGUI().RecordButtonClick(m_loadedAsset.name, name, action);
	switch (action)
	{
	case GameGUIActionType::NewGame:
	{
		if (auto* button = dynamic_cast<MyGUI::Button*>(sender))
		{
			const GameGUIWidgetDef* buttonDef = FindWidgetDef(m_loadedAsset, name);
			button->setColour(ParseColour(buttonDef ? buttonDef->clickedColor : std::string{}, MyGUI::Colour::White));
			button->setStateSelected(true);
		}
		Root::Current().FrontEnd().RuntimeGUI().RecordClick("New Game action requested");
		Root::Current().Debugger().LogMessage("GameGUI NewGame action requested");
		if (Root::Current().Projects().CurrentProjectPath().empty())
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame failed: no project is currently loaded.");
			break;
		}
		Root::Current().FrontEnd().RuntimeGUI().HideAll();
		Scene* targetScene = FindNamedLevel(Root::Current().Levels(), launchLevel);
		Root::Current().Debugger().LogMessage(
			"GameGUI NewGame initial scene lookup: " +
			std::string(targetScene ? targetScene->Name() : "<not found>"));
		std::cout << "GameGUI NewGame initial scene lookup: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		if (!targetScene)
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame reloading project to retry scene lookup.");
			std::cout << "GameGUI NewGame reloading project to retry scene lookup.\n";
			if (!Root::Current().Projects().LoadProject(Root::Current().Projects().CurrentProjectPath(), Root::Current().Levels()))
			{
				Root::Current().Debugger().LogMessage("GameGUI NewGame failed: could not reload the current project.");
				break;
			}
			targetScene = FindNamedLevel(Root::Current().Levels(), launchLevel);
			Root::Current().Debugger().LogMessage(
				"GameGUI NewGame post-reload scene lookup: " +
				std::string(targetScene ? targetScene->Name() : "<not found>"));
			std::cout << "GameGUI NewGame post-reload scene lookup: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		}
		if (!targetScene)
		{
			targetScene = FindPlayableScene(Root::Current().Levels());
			Root::Current().Debugger().LogMessage(
				"GameGUI NewGame fallback playable scene: " +
				std::string(targetScene ? targetScene->Name() : "<not found>"));
			std::cout << "GameGUI NewGame fallback playable scene: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		}
		if (targetScene)
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame launching scene: " + targetScene->Name());
			std::cout << "GameGUI NewGame launching scene: " << targetScene->Name() << '\n';
			Root::Current().Levels().SetActiveLevel(targetScene->Name());
			Root::Current().Levels().SetStartupLevelName(targetScene->Name());
		}
		if (!Root::Current().Gameplay().BootPlayableLevel(Root::Current().FrontEnd(), Root::Current().Debugger()))
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame failed: no playable Scene could be selected.");
			break;
		}
		Root::Current().FrontEnd().RuntimeGUI().RecordClick("New Game started");
		break;
	}
	case GameGUIActionType::None:
	default:
		break;
	}
}







