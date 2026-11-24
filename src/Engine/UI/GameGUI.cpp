#include "Engine/UI/GameGUI.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/StbImage.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/UI/GameGUIAsset.h"
#include "Engine/UI/GameGUIAssetUtils.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/Audio.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Health.h"
#include "PlayerController.h"

#include <MYGUI/MyGUI_Button.h>
#include <MYGUI/MyGUI_Colour.h>
#include <MYGUI/MyGUI_Gui.h>
#include <MYGUI/MyGUI_ImageBox.h>
#include <MYGUI/MyGUI_InputManager.h>
#include <MYGUI/MyGUI_ResourceManager.h>
#include <MYGUI/MyGUI_SkinManager.h>
#include <MYGUI/MyGUI_TextBox.h>
#include <MYGUI/MyGUI_FontManager.h>
#include <MYGUI/MyGUI_IFont.h>
#include <MYGUI/MyGUI_ITexture.h>
#include <MYGUI/MyGUI_OpenGLTexture.h>
#include <MYGUI/MyGUI_OpenGLDataManager.h>
#include <MYGUI/MyGUI_OpenGLPlatform.h>
#include <MYGUI/MyGUI_LayerManager.h>
#include <MYGUI/MyGUI_PointerManager.h>
#include <MYGUI/MyGUI_OpenGLImageLoader.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <functional>
#include <cctype>
#include <unordered_map>
#include <vector>
#include "Engine/Core/Texture.h"

namespace {
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
			Root::Current().Projects().ProjectAssetsDirectory() / requestedPath,
			requestedPath,
			executableRoot / requestedPath,
			executableRoot / "resources" / requestedPath,
			executableRoot / "assets" / requestedPath,
		#if defined(AQUANACT_SOURCE_ROOT) && !defined(AQUANACT_GAME)
			std::filesystem::path(AQUANACT_SOURCE_ROOT) / "projects" / "project" / "assets" / requestedPath,
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
			if (scene && sceneManager.SceneKindFor(scene->Name()) == SceneManager::SceneKind::Level &&
				!sceneManager.IsMainMenuScene(scene->Name()))
			{
				return scene.get();
			}
		}
		return nullptr;
	}

	std::string InspectFontAtlasOnGPU(MyGUI::ITexture* atlas, const MyGUI::GlyphInfo* glyph)
	{
		const auto* glAtlas = dynamic_cast<MyGUI::OpenGLTexture*>(atlas);
		if (!glAtlas)
		{
			return "gpuAtlas=not-an-OpenGL-texture";
		}

		const GLuint textureId = glAtlas->getTextureId();
		const GLenum previousError = glGetError();
		GLint previousBinding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
		const GLenum bindingQueryError = glGetError();
		glBindTexture(GL_TEXTURE_2D, textureId);
		GLint boundTexture = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
		const GLenum bindAndStateQueryError = glGetError();

		GLint storageWidth = 0;
		GLint storageHeight = 0;
		GLint internalFormat = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &storageWidth);
		const GLenum widthQueryError = glGetError();
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &storageHeight);
		const GLenum heightQueryError = glGetError();
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
		const GLenum formatQueryError = glGetError();
		GLint minFilter = 0;
		GLint magFilter = 0;
		GLint wrapS = 0;
		GLint wrapT = 0;
		GLint baseLevel = 0;
		GLint maxLevel = 0;
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minFilter);
		const GLenum minFilterQueryError = glGetError();
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);
		const GLenum magFilterQueryError = glGetError();
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrapS);
		const GLenum wrapSQueryError = glGetError();
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrapT);
		const GLenum wrapTQueryError = glGetError();
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &baseLevel);
		const GLenum baseLevelQueryError = glGetError();
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maxLevel);
		const GLenum maxLevelQueryError = glGetError();

		std::ostringstream result;
		result << "gpuAtlasId=" << textureId
			<< ", gpuAtlasObject=" << (glIsTexture(textureId) ? "yes" : "no")
			<< ", bindingBefore=" << previousBinding << ", bindingQueryGlError=0x" << std::hex << bindingQueryError << std::dec
			<< ", bindingAfter=" << boundTexture << ", bindStateGlError=0x" << std::hex << bindAndStateQueryError << std::dec
			<< ", myguiAtlas=" << atlas->getWidth() << 'x' << atlas->getHeight()
			<< ", myguiFormat=" << atlas->getFormat().getValue() << ", myguiUsage=" << atlas->getUsage().getValue()
			<< ", gpuStorage=" << storageWidth << 'x' << storageHeight
			<< ", gpuInternalFormat=0x" << std::hex << internalFormat << std::dec
			<< ", preexistingGlError=0x" << std::hex << previousError << std::dec
			<< ", levelQueryErrors[width,height,format]=0x" << std::hex << widthQueryError << ",0x"
			<< heightQueryError << ",0x" << formatQueryError << std::dec
			<< ", textureParams[min,mag,wrapS,wrapT,base,max]=0x"
			<< std::hex << minFilter << ",0x" << magFilter << ",0x" << wrapS << ",0x" << wrapT
			<< std::dec << ',' << baseLevel << ',' << maxLevel
			<< ", textureParamErrors[min,mag,wrapS,wrapT,base,max]=0x"
			<< std::hex << minFilterQueryError << ",0x" << magFilterQueryError << ",0x" << wrapSQueryError
			<< ",0x" << wrapTQueryError << ",0x" << baseLevelQueryError << ",0x" << maxLevelQueryError
			<< std::dec;

		if (glyph && widthQueryError == GL_NO_ERROR && heightQueryError == GL_NO_ERROR
			&& storageWidth > 0 && storageHeight > 0
			&& static_cast<std::uint64_t>(storageWidth) * static_cast<std::uint64_t>(storageHeight) <= 16'777'216)
		{
			std::vector<unsigned char> rgba(
				static_cast<std::size_t>(storageWidth) * static_cast<std::size_t>(storageHeight) * 4);
			GLint previousPackAlignment = 4;
			GLint previousPackRowLength = 0;
			GLint previousPackSkipRows = 0;
			GLint previousPackSkipPixels = 0;
			GLint previousPixelPackBuffer = 0;
			glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
			glGetIntegerv(GL_PACK_ROW_LENGTH, &previousPackRowLength);
			glGetIntegerv(GL_PACK_SKIP_ROWS, &previousPackSkipRows);
			glGetIntegerv(GL_PACK_SKIP_PIXELS, &previousPackSkipPixels);
			glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPixelPackBuffer);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			glPixelStorei(GL_PACK_ROW_LENGTH, 0);
			glPixelStorei(GL_PACK_SKIP_ROWS, 0);
			glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			const GLenum pixelReadError = glGetError();
			glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
			glPixelStorei(GL_PACK_ROW_LENGTH, previousPackRowLength);
			glPixelStorei(GL_PACK_SKIP_ROWS, previousPackSkipRows);
			glPixelStorei(GL_PACK_SKIP_PIXELS, previousPackSkipPixels);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPixelPackBuffer));
			result << ", atlasReadGlError=0x" << std::hex << pixelReadError << std::dec;
			if (pixelReadError == GL_NO_ERROR)
			{
				const float minU = std::clamp(std::min(glyph->uvRect.left, glyph->uvRect.right), 0.0f, 1.0f);
				const float maxU = std::clamp(std::max(glyph->uvRect.left, glyph->uvRect.right), 0.0f, 1.0f);
				const float minV = std::clamp(std::min(glyph->uvRect.top, glyph->uvRect.bottom), 0.0f, 1.0f);
				const float maxV = std::clamp(std::max(glyph->uvRect.top, glyph->uvRect.bottom), 0.0f, 1.0f);
				const int x0 = std::clamp(static_cast<int>(std::floor(minU * storageWidth)), 0, storageWidth - 1);
				const int x1 = std::clamp(static_cast<int>(std::ceil(maxU * storageWidth)), x0 + 1, storageWidth);
				const int y0 = std::clamp(static_cast<int>(std::floor(minV * storageHeight)), 0, storageHeight - 1);
				const int y1 = std::clamp(static_cast<int>(std::ceil(maxV * storageHeight)), y0 + 1, storageHeight);
				unsigned int minimum[4] = { 255, 255, 255, 255 };
				unsigned int maximum[4] = { 0, 0, 0, 0 };
				std::uint64_t totals[4] = { 0, 0, 0, 0 };
				std::size_t sampleCount = 0;
				for (int y = y0; y < y1; ++y)
				{
					for (int x = x0; x < x1; ++x)
					{
						const std::size_t offset = (static_cast<std::size_t>(y) * storageWidth + x) * 4;
						for (std::size_t channel = 0; channel < 4; ++channel)
						{
							const unsigned int value = rgba[offset + channel];
							minimum[channel] = std::min(minimum[channel], value);
							maximum[channel] = std::max(maximum[channel], value);
							totals[channel] += value;
						}
						++sampleCount;
					}
				}
				result << ", glyphAtlasRegion=" << x0 << ',' << y0 << ' ' << x1 - x0 << 'x' << y1 - y0
					<< ", sampledRGBA[min/max/avg]=";
				for (std::size_t channel = 0; channel < 4; ++channel)
				{
					if (channel != 0) result << ';';
					result << minimum[channel] << '/' << maximum[channel] << '/'
						<< (sampleCount ? static_cast<double>(totals[channel]) / sampleCount : 0.0);
				}
				result << ", glyphAtlasRGBA=";
				const int loggedWidth = std::min(16, x1 - x0);
				const int loggedHeight = std::min(16, y1 - y0);
				for (int row = 0; row < loggedHeight; ++row)
				{
					if (row != 0) result << '|';
					for (int column = 0; column < loggedWidth; ++column)
					{
						if (column != 0) result << ',';
						const std::size_t offset =
							(static_cast<std::size_t>(y0 + row) * storageWidth + x0 + column) * 4;
						result << static_cast<unsigned int>(rgba[offset]) << '/'
							<< static_cast<unsigned int>(rgba[offset + 1]) << '/'
							<< static_cast<unsigned int>(rgba[offset + 2]) << '/'
							<< static_cast<unsigned int>(rgba[offset + 3]);
					}
				}
			}
		}

		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
		return result.str();
	}

	Scene* FindNamedLevel(SceneManager& sceneManager, const std::string& levelName)
	{
		if (levelName.empty())
		{
			return nullptr;
		}
		Scene* scene = sceneManager.FindLevel(levelName);
		if (scene)
		{
			return scene;
		}
		return nullptr;
	}

	Component* ResolveBoundComponent(const GameGUIWidgetDef& def)
	{
		Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
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
		if (def.bindEntity == "GameManager")
			if (Entity* gameManager = activeLevel->GameManagerEntity())
				return gameManager->GetComponentByName(def.bindComponent);
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

	template<typename TWidget>
	TWidget* CreateGuiWidget(MyGUI::Gui* gui, MyGUI::Widget* parent, const std::string& skin, int x, int y, int width, int height, const std::string& layer, const std::string& name)
	{
		if (parent)
		{
			return parent->createWidget<TWidget>(skin, x, y, width, height, MyGUI::Align::Default, name);
		}
		return gui->createWidget<TWidget>(skin, x, y, width, height, MyGUI::Align::Default, layer, name);
	}

	void FinalizeWidget(MyGUI::Widget* widget, bool visible, float alpha, bool promoteToTopLayer, bool allowMouseFocus, bool inheritPick)
	{
		if (!widget)
		{
			return;
		}

		widget->setVisible(visible);
		widget->setAlpha(alpha);
		widget->setNeedMouseFocus(allowMouseFocus);
		widget->setInheritsPick(inheritPick);
		if (promoteToTopLayer)
		{
			MyGUI::LayerManager::getInstance().upLayerItem(widget);
		}
	}

	void LogWidgetCreated(const GameGUIWidgetDef& def)
	{
		Root::Current().Debugger().LogMessage(
			std::string("GameGUI widget created: name='") + def.name +
			"', type='" + def.type +
			"', skin='" + def.skin +
			"', layer='" + def.layer + "'");
	}

	void LogSkinLookup(const std::string& widgetName, const std::string& skin)
	{
		const bool skinExists = MyGUI::SkinManager::getInstance().isExist(skin);
		const bool resourceExists = MyGUI::ResourceManager::getInstance().isExist(skin);
		Root::Current().Debugger().LogTagged(
			(skinExists || resourceExists) ? "MyGUI" : "MyGUI-WARN",
			"skin lookup: widget='" + widgetName + "', skin='" + skin +
			"', skinRegistered=" + (skinExists ? "true" : "false") +
			", layoutOrResourceRegistered=" + (resourceExists ? "true" : "false"));
	}

	std::string FormatBindableValue(float value)
	{
		if (std::fabs(value - std::round(value)) < 0.0001f)
		{
			return std::to_string(static_cast<int>(std::lround(value)));
		}

		std::ostringstream stream;
		stream << std::fixed << std::setprecision(2) << value;
		std::string formatted = stream.str();
		while (!formatted.empty() && formatted.back() == '0')
		{
			formatted.pop_back();
		}
		if (!formatted.empty() && formatted.back() == '.')
		{
			formatted.pop_back();
		}
		return formatted;
	}

}

void GameGUI::ConfigureProgressBar(MyGUI::ProgressBar* progress)
{
	// The current runtime treats progress bars as a normalized 0-100 display.
	progress->setProgressRange(100);
	progress->setProgressPosition(100);
}

void GameGUI::SetButtonVisualState(MyGUI::Button* button, const GameGUIWidgetDef& def)
{
	// Button chrome is driven separately from the nested text label.
	button->setCaption("");
	button->setTextColour(ParseColour(def.textColor, MyGUI::Colour::Black));
	m_buttonDefaultTextColours[button] = button->getTextColour();
	button->setColour(ParseColour(def.highlightColor, MyGUI::Colour::White));
	button->setStateSelected(false);
}

void GameGUI::SetButtonFocusState(MyGUI::Button* button, const GameGUIWidgetDef& def)
{
	// Only visible buttons participate in controller focus navigation.
	button->eventMouseSetFocus += MyGUI::newDelegate(this, &GameGUI::OnButtonMouseFocus);
	button->eventMouseLostFocus += MyGUI::newDelegate(this, &GameGUI::OnButtonMouseLostFocus);
	// The retry button is hidden until the death overlay is active, but it still
	// needs to be registered so controller navigation can discover it when shown.
	if (def.visible || def.name == "tryAgainButton" || def.name == "Try Again")
	{
		m_allControllerButtons.push_back(button);
	}
}

void GameGUI::SetButtonLabel(MyGUI::Button* button, const GameGUIWidgetDef& def, int buttonWidth, int buttonHeight)
{
	// The label is a nested widget so we can control text and skin separately.
	MyGUI::TextBox* label = button->createWidget<MyGUI::TextBox>("TextBoxSkin", 0, 0, buttonWidth, buttonHeight, MyGUI::Align::Stretch, def.name + "_label");
	if (!label)
	{
		return;
	}

	label->setCaption(def.text.empty() ? def.name : def.text);
	label->setTextColour(button->getTextColour());
	label->setTextAlign(MyGUI::Align::Center);
	if (def.fontSize > 0)
	{
		label->setFontHeight(def.fontSize);
	}
	if (!def.fontName.empty())
	{
		label->setFontName(def.fontName);
	}
	label->setNeedMouseFocus(false);
	label->setNeedKeyFocus(false);
	label->setInheritsPick(false);
	MyGUI::LayerManager::getInstance().upLayerItem(label);
	label->setTextColour(m_buttonDefaultTextColours[button]);
	m_buttonLabels[button] = label;
}

void GameGUI::HookButtonClick(MyGUI::Button* button, const GameGUIWidgetDef& def)
{
	// Clicks route back into GameGUI so widget data stays declarative.
	button->eventMouseButtonClick += MyGUI::newDelegate(this, &GameGUI::OnWidgetClicked);
	Root::Current().Debugger().LogMessage(std::string("GameGUI click handler bound for widget: ") + def.name);
}

void GameGUI::FinalizeAndLogWidget(MyGUI::Widget* widget, const GameGUIWidgetDef& def, bool promoteToTopLayer, bool allowMouseFocus, bool inheritPick)
{
	// Most widgets share the same final visibility and layering step.
	FinalizeWidget(widget, def.visible, def.alpha, promoteToTopLayer, allowMouseFocus, inheritPick);
	LogWidgetCreated(def);
}

void GameGUI::BindValueWidgetRefresh(const GameGUIWidgetDef& def, MyGUI::Widget* widget, const std::function<void(float)>& applyValue)
{
	// Value widgets subscribe to the shared member-value channel exposed by the component.
	Component* component = ResolveBoundComponent(def);
	if (!component)
	{
		Root::Current().Debugger().LogMessage(
			"GameGUI value binding could not resolve component: " +
			def.bindEntity + "." + def.bindComponent);
		return;
	}

	float value = 0.0f;
	if (!component->TryGetBindableValue(def.bindMember, value))
	{
		Root::Current().Debugger().LogMessage(
			"GameGUI value binding could not resolve member: " +
			def.bindEntity + "." + def.bindComponent + "." + def.bindMember);
		return;
	}

	applyValue(value);

	const std::string valueChannel = component->BindableValueChannel(def.bindMember);
	Root::Current().Events().GetEvent(valueChannel).Subscribe(widget, [widget, binding = def, applyValue]()
	{
		Component* currentComponent = ResolveBoundComponent(binding);
		if (!currentComponent)
		{
			return;
		}

		float currentValue = 0.0f;
		if (!currentComponent->TryGetBindableValue(binding.bindMember, currentValue))
		{
			return;
		}

		applyValue(currentValue);
	});
	Root::Current().Debugger().LogMessage("GameGUI bound value widget '" + def.name + "' to " + valueChannel);
}

void GameGUI::BindTextWidgetValue(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget)
{
	BindValueWidgetRefresh(def, textWidget, [textWidget](float currentValue)
	{
		textWidget->setCaption(FormatBindableValue(currentValue));
	});
}

void GameGUI::BindTextWidgetEvent(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget)
{
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
	Root::Current().Events().GetEvent(channel).Subscribe(textWidget, [textWidget, binding = def]()
	{
		Component* currentComponent = ResolveBoundComponent(binding);
		if (!currentComponent)
		{
			return;
		}

		textWidget->setCaption(currentComponent->GetBindableEventText(binding.bindEvent));
	});
	Root::Current().Debugger().LogMessage("GameGUI bound widget '" + def.name + "' to " + channel);
}

void GameGUI::BindTextWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::TextBox* textWidget)
{
	// Text widget binding is opt-in through its Bindable setting.
	if (!textWidget || !def.bindingEnabled)
	{
		return;
	}

	const bool hasValueBinding = !def.bindMember.empty();
	const bool hasEventBinding = !def.bindEvent.empty();

	if (hasValueBinding)
	{
		BindTextWidgetValue(def, textWidget);
	}
	if (hasEventBinding)
	{
		BindTextWidgetEvent(def, textWidget);
		return;
	}

	if (!hasValueBinding)
	{
		Root::Current().Debugger().LogMessage("GameGUI binding ignored for unsupported widget: " + def.name);
	}
}

void GameGUI::BindProgressBarFromDef(const GameGUIWidgetDef& def, MyGUI::ProgressBar* progress)
{
	if (!progress || def.bindEntity.empty() || def.bindComponent.empty() || def.bindMember.empty())
	{
		return;
	}
	// Progress bars normalize the numeric member to the 0-100 range.
	BindValueWidgetRefresh(def, progress, [progress](float currentValue)
	{
		const float clamped = std::clamp(currentValue, 0.0f, 100.0f);
		progress->setProgressPosition(static_cast<int>(std::lround(clamped)));
	});

	Component* component = ResolveBoundComponent(def);
	if (!component || def.bindMember != "Health")
	{
		return;
	}

	float maxValue = 0.0f;
	if (!component->TryGetBindableValue("MaxHealth", maxValue) || maxValue <= 0.0f)
	{
		return;
	}

	float currentValue = 0.0f;
	if (!component->TryGetBindableValue("Health", currentValue))
	{
		return;
	}

	const auto applyNormalizedValue = [progress, maxValue](float healthValue)
	{
		const float normalized = std::clamp((healthValue / maxValue) * 100.0f, 0.0f, 100.0f);
		progress->setProgressPosition(static_cast<int>(std::lround(normalized)));
	};

	applyNormalizedValue(currentValue);
	const std::string normalizedChannel = component->BindableValueChannel("Health");
	Root::Current().Events().GetEvent(normalizedChannel).Subscribe(progress, [progress, binding = def, applyNormalizedValue]()
	{
		Component* currentComponent = ResolveBoundComponent(binding);
		if (!currentComponent)
		{
			return;
		}

		float healthValue = 0.0f;
		float maxHealthValue = 0.0f;
		if (!currentComponent->TryGetBindableValue("Health", healthValue) || !currentComponent->TryGetBindableValue("MaxHealth", maxHealthValue) || maxHealthValue <= 0.0f)
		{
			return;
		}

		applyNormalizedValue(healthValue);
	});
	Root::Current().Debugger().LogMessage("GameGUI bound normalized progress bar '" + def.name + "' to " + normalizedChannel);
}

void GameGUI::UpdateDeathOverlay()
{
	MyGUI::Widget* deathText = RuntimeWidget("deathText");
	MyGUI::Widget* winText = RuntimeWidget("winText");
	MyGUI::Widget* tryAgainButton = RuntimeWidget("tryAgainButton");
	if (!tryAgainButton)
	{
		tryAgainButton = RuntimeWidget("Try Again");
	}
	if (!deathText && !winText && !tryAgainButton)
	{
		return;
	}

	bool playerDied = false;
	if (Scene* activeLevel = Root::Current().Scenes().ActiveLevel())
	{
		for (const auto& entity : activeLevel->Entities())
		{
			if (!entity || !entity->GetComponent<PlayerController>())
			{
				continue;
			}

			const EntityStateMachine* stateMachine = entity->GetEntityState();
			const Health* health = entity->GetComponent<Health>();
			playerDied = (stateMachine && stateMachine->CurrentState() == "death") ||
				(health && health->IsDead());
			break;
		}
	}
	const bool levelWon = Root::Current().Gameplay().LevelWon();

	if (deathText)
	{
		deathText->setVisible(playerDied);
	}
	if (winText)
	{
		winText->setVisible(levelWon);
	}
	if (tryAgainButton)
	{
		tryAgainButton->setVisible(playerDied);
	}

	// Death is a UI interaction state. Release the captured level cursor once the
	// overlay appears; restarting the level captures it again through EnterGameplay.
	const bool overlayChanged = playerDied != m_deathOverlayVisible;
	if (playerDied && overlayChanged)
	{
		Root::Current().InputRef().ReleaseCursorForUI();
	}
	if (overlayChanged)
	{
		RefreshVisibleControllerButtons();
	}
	m_deathOverlayVisible = playerDied;
}

MyGUI::Widget* GameGUI::CreatePanelWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	// Panels are containers, so they are mostly about layout and pick behavior.
	const std::string skin = def.useSkin ? (def.skin.empty() ? "PanelSkin" : def.skin) : "PanelEmpty";
	LogSkinLookup(def.name, skin);
	MyGUI::Widget* panel = CreateGuiWidget<MyGUI::Widget>(m_gui, parent, skin, def.x, def.y, def.width, def.height, def.layer, def.name);
	FinalizeAndLogWidget(panel, def, parent == nullptr, false, true);
	return panel;
}

std::string GameGUI::ResolveButtonSkin(const GameGUIWidgetDef& def) const
{
	const GameGUIWidgetDef* parentPanel = FindWidgetDef(m_loadedAsset, def.parentName);
	const bool panelHidesButtonSkin = parentPanel && !parentPanel->panelButtonUseSkin;
	if (!def.useSkin || panelHidesButtonSkin)
	{
		return "ButtonEmptySkin";
	}

	if (parentPanel)
	{
		return parentPanel->panelButtonSkin.empty() ? "MultiListButtonSkin" : parentPanel->panelButtonSkin;
	}

	return def.skin.empty() ? "ButtonSkin" : def.skin;
}

MyGUI::Button* GameGUI::CreateButtonWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	// Buttons use a nested TextBox for caption rendering so the visual skin can
	// stay separate from the editable text content.
	const std::string skin = ResolveButtonSkin(def);
	LogSkinLookup(def.name, skin);
	const int buttonWidth = std::max(1, def.width);
	const int buttonHeight = std::max(1, def.height);
	MyGUI::Button* button = CreateGuiWidget<MyGUI::Button>(m_gui, parent, skin, def.x, def.y, buttonWidth, buttonHeight, def.layer, def.name);

	if (!button)
	{
		return nullptr;
	}

	SetButtonVisualState(button, def);
	SetButtonFocusState(button, def);
	SetButtonLabel(button, def, buttonWidth, buttonHeight);
	std::string focusSound = def.focusSound;
	if (focusSound.empty())
	{
		if (const GameGUIWidgetDef* parentPanel = FindWidgetDef(m_loadedAsset, def.parentName))
		{
			focusSound = parentPanel->panelButtonFocusSound;
		}
	}
	if (!focusSound.empty() && Root::Current().State().IsGameMode())
	{
		const std::string soundName = "ui_focus_" + def.name;
		Audio::LoadSound(soundName, "assets/" + focusSound);
		m_buttonFocusSounds[button] = soundName;
	}
	HookButtonClick(button, def);
	// Buttons must be the actual mouse-pick target. In MyGUI, inheritsPick=true
	// makes a widget return only a picked child; the nested label deliberately
	// disables picking, so using it here causes the button to report no hit at
	// all. Containers keep inherited picking, but interactive buttons do not.
	FinalizeAndLogWidget(button, def, parent == nullptr, true, false);
	return button;
}

MyGUI::TextBox* GameGUI::CreateTextWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	const std::string skin = def.skin.empty() || def.skin == "TextBox" ? "TextBoxSkin" : def.skin;
	LogSkinLookup(def.name, skin);
	MyGUI::TextBox* text = CreateGuiWidget<MyGUI::TextBox>(m_gui, parent, skin, def.x, def.y, def.width, def.height, def.layer, def.name);

	if (!text)
	{
		return nullptr;
	}

	text->setFontName(def.fontName.empty() ? "Default" : def.fontName);
	text->setCaption(def.text);
	// Text widgets should draw glyphs without painting an opaque TextBox skin.
	text->setColour(MyGUI::Colour::Zero);
	text->setTextColour(ParseColour(def.textColor, MyGUI::Colour::Black));
	text->setTextAlign(MyGUI::Align::Center);
	if (def.fontSize > 0)
	{
		text->setFontHeight(def.fontSize);
	}
	if (parent)
	{
		// Nested text must render above its parent's skin, just like button labels.
		MyGUI::LayerManager::getInstance().upLayerItem(text);
		text->setNeedMouseFocus(false);
		text->setNeedKeyFocus(false);
		text->setInheritsPick(false);
	}
	FinalizeAndLogWidget(text, def, parent == nullptr, false, parent == nullptr);
	return text;
}

MyGUI::ImageBox* GameGUI::CreateImageWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	const std::string skin = def.solidWhiteBox ? "WhiteSkin"
		: def.skin.empty() ? "ImageBox" : def.skin;
	LogSkinLookup(def.name, skin);
	MyGUI::ImageBox* image = CreateGuiWidget<MyGUI::ImageBox>(m_gui, parent, skin, def.x, def.y, def.width, def.height, def.layer, def.name);

	if (!image)
	{
		return nullptr;
	}

	if (!def.solidWhiteBox && !def.texture.empty())
	{
		int imageWidth = 0;
		int imageHeight = 0;
		const std::filesystem::path imagePath = ResolveGameGUIImagePath(def.texture);
		if (GameGUICreatorHelpers::GetTextureDimensions(imagePath, imageWidth, imageHeight))
		{
			try
			{
				image->setImageTexture(def.texture);
			}
			catch (const std::exception& ex)
			{
				Root::Current().Debugger().LogMessage(
					"GameGUI image widget '" + def.name + "' texture assignment failed: " + ex.what());
			}
		}
		else
		{
			Root::Current().Debugger().LogMessage(
				"GameGUI image widget '" + def.name + "' has an empty or unreadable image file: " + def.texture);
		}
	}
	FinalizeAndLogWidget(image, def, parent == nullptr, false, true);
	return image;
}

MyGUI::Widget* GameGUI::CreateIconWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	static const char* const iconSkins[] = {
		"ButtonRightSkin", "ButtonLeftSkin", "ButtonUpSkin", "ButtonDownSkin"
	};
	bool validSkin = false;
	for (const char* iconSkin : iconSkins)
		validSkin = validSkin || def.skin == iconSkin;
	const std::string skin = validSkin ? def.skin : iconSkins[0];
	LogSkinLookup(def.name, skin);
	MyGUI::Widget* icon = CreateGuiWidget<MyGUI::Widget>(
		m_gui, parent, skin, def.x, def.y, def.width, def.height, def.layer, def.name);
	if (!icon)
		return nullptr;
	icon->setNeedKeyFocus(false);
	FinalizeAndLogWidget(icon, def, parent == nullptr, false, false);
	return icon;
}

MyGUI::ProgressBar* GameGUI::CreateProgressBarWidget(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	const std::string skin = def.skin.empty() ? "ProgressBar" : def.skin;
	LogSkinLookup(def.name, skin);
	MyGUI::ProgressBar* progress = CreateGuiWidget<MyGUI::ProgressBar>(m_gui, parent, skin, def.x, def.y, std::max(1, def.width), std::max(1, def.height), def.layer, def.name);

	if (!progress)
	{
		return nullptr;
	}

	// Progress bars use the same shared finalization path, but without mouse focus.
	ConfigureProgressBar(progress);
	FinalizeAndLogWidget(progress, def, parent == nullptr, false, false);
	if (!parent)
	{
		MyGUI::LayerManager::getInstance().upLayerItem(progress);
	}
	return progress;
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
		Root::Current().Debugger().LogTagged(
			"MyGUI", "image loaded: requested='" + _filename + "', resolved='" + resolvedPath.string() +
			"', size=" + std::to_string(image.getWidth()) + "x" + std::to_string(image.getHeight()));

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
		const auto logDirectory = [&resourceRoot](const std::filesystem::path& path)
		{
			std::error_code error;
			const bool exists = std::filesystem::exists(path, error);
			const bool directory = exists && std::filesystem::is_directory(path, error);
			std::size_t fileCount = 0;
			if (directory)
			{
				for (const auto& entry : std::filesystem::recursive_directory_iterator(path, error))
				{
					if (!error && entry.is_regular_file()) ++fileCount;
				}
			}
			Root::Current().Debugger().LogTagged(
				"MyGUI", "resource directory: '" + path.string() + "', exists=" +
				(exists ? "true" : "false") + ", files=" + std::to_string(fileCount));
		};
		logDirectory(resourceRoot);
		logDirectory(resourceRoot / "resources");
		logDirectory(resourceRoot / "assets");
		// Packaged games keep MyGUI media under resources/. The editor build
		// continues to receive media directly in its build output directory from
		// the CMake copy target.
#ifdef AQUANACT_GAME
		dataManager.addResourceLocation((resourceRoot / "resources").string(), false);
		Root::Current().Debugger().LogTagged("MyGUI", "registered resource location: '" + (resourceRoot / "resources").string() + "'");
#else
		dataManager.addResourceLocation(resourceRoot.string(), false);
		Root::Current().Debugger().LogTagged("MyGUI", "registered resource location: '" + resourceRoot.string() + "'");
#endif
		if (Root::HasCurrent() && !Root::Current().Projects().CurrentProjectPath().empty())
		{
			dataManager.addResourceLocation(Root::Current().Projects().ProjectAssetsDirectory().string(), true);
			Root::Current().Debugger().LogTagged("MyGUI", "registered project asset location: '" + Root::Current().Projects().ProjectAssetsDirectory().string() + "'");
		}
		// Gui has to exist only after the platform and resources are available. That
		// ordering fixed the runtime exceptions we saw during the first integration pass.
		m_gui = new MyGUI::Gui();
		m_gui->initialise();
		const char* requiredResources[] = {
			"MyGUI_Core.xml", "MyGUI_CommonSkins.xml", "MyGUI_BlueWhiteSkins.xml",
			"MyGUI_BlueWhiteTemplates.xml", "MyGUI_BlueWhiteImages.xml", "MyGUI_Layers.xml",
			"MyGUI_Fonts.xml"
		};
		for (const char* resource : requiredResources)
		{
			Root::Current().Debugger().LogTagged(
				dataManager.isDataExist(resource) ? "MyGUI" : "MyGUI-WARN",
				std::string("resource lookup: '") + resource + "', exists=" +
				(dataManager.isDataExist(resource) ? "true" : "false") +
				", path='" + dataManager.getDataPath(resource) + "'");
		}
		Root::Current().Debugger().LogTagged("MyGUI", "SkinManager default skin='" + MyGUI::SkinManager::getInstance().getDefaultSkin() + "'");
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

	UpdateDeathOverlay();
	UpdateWorldAnchoredWidgets();

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

void GameGUI::DrawCreatorDiagnosticsWindow(bool* open) const
{
	if (!open || !*open || !m_initialized || !m_platform || !m_window)
	{
		return;
	}

	bool visible = *open;
	ImGui::SetNextWindowSize(ImVec2(440.0f, 520.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("GameGUI Platform Diagnostics", &visible))
	{
		const GLubyte* vendor = glGetString(GL_VENDOR);
		const GLubyte* renderer = glGetString(GL_RENDERER);
		const GLubyte* version = glGetString(GL_VERSION);
		ImGui::Text("Platform: %s",
#if defined(__APPLE__)
			"macOS"
#elif defined(_WIN32)
			"Windows"
#else
			"Other"
#endif
		);
		ImGui::Text("GL vendor: %s", vendor ? reinterpret_cast<const char*>(vendor) : "<unavailable>");
		ImGui::Text("GL renderer: %s", renderer ? reinterpret_cast<const char*>(renderer) : "<unavailable>");
		ImGui::Text("GL version: %s", version ? reinterpret_cast<const char*>(version) : "<unavailable>");

		int windowWidth = 0;
		int windowHeight = 0;
		glfwGetWindowSize(m_window->GLFW(), &windowWidth, &windowHeight);
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
		const float scaleX = windowWidth > 0 ? static_cast<float>(framebufferWidth) / windowWidth : 0.0f;
		const float scaleY = windowHeight > 0 ? static_cast<float>(framebufferHeight) / windowHeight : 0.0f;
		ImGui::Text("Window logical size: %d x %d", windowWidth, windowHeight);
		ImGui::Text("Framebuffer size: %d x %d (scale %.2f x %.2f)",
			framebufferWidth, framebufferHeight, scaleX, scaleY);
		const MyGUI::IntSize& myguiSize = m_platform->getRenderManagerPtr()->getViewSize();
		ImGui::Text("MyGUI view size: %d x %d", myguiSize.width, myguiSize.height);
		GLint viewport[4] = { 0, 0, 0, 0 };
		glGetIntegerv(GL_VIEWPORT, viewport);
		ImGui::Text("GL viewport: %d, %d  %d x %d", viewport[0], viewport[1], viewport[2], viewport[3]);
		ImGui::Text("MyGUI initialized: yes | Active asset: %s",
			m_loadedAsset.name.empty() ? "<none>" : m_loadedAsset.name.c_str());

		GLint currentProgram = 0;
		GLint vertexArray = 0;
		GLint activeTexture = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		ImGui::Separator();
		ImGui::Text("Post-MyGUI state: program %d, VAO %d, active texture 0x%X",
			currentProgram, vertexArray, activeTexture);
		ImGui::Text("Definitions: %zu | live widgets: %zu",
			m_loadedAsset.widgets.size(), m_runtimeWidgetLookup.size());
		if (ImGui::Button("Write Snapshot to Runtime Log"))
		{
			auto& debugger = Root::Current().Debugger();
			auto logValue = [&debugger](const std::string& label, const std::string& value)
			{
				debugger.LogTagged("GameGUI-Diagnostics", label + ": " + value);
			};
			logValue("Snapshot", "begin");
			logValue("Platform",
#if defined(__APPLE__)
				"macOS"
#elif defined(_WIN32)
				"Windows"
#else
				"Other"
#endif
			);
			logValue("GL vendor", vendor ? reinterpret_cast<const char*>(vendor) : "<unavailable>");
			logValue("GL renderer", renderer ? reinterpret_cast<const char*>(renderer) : "<unavailable>");
			logValue("GL version", version ? reinterpret_cast<const char*>(version) : "<unavailable>");
			logValue("Window logical size", std::to_string(windowWidth) + " x " + std::to_string(windowHeight));
			logValue("Framebuffer size and scale", std::to_string(framebufferWidth) + " x " +
				std::to_string(framebufferHeight) + " (" + std::to_string(scaleX) + " x " + std::to_string(scaleY) + ")");
			logValue("MyGUI view size", std::to_string(myguiSize.width) + " x " + std::to_string(myguiSize.height));
			logValue("GL viewport", std::to_string(viewport[0]) + ", " + std::to_string(viewport[1]) + " " +
				std::to_string(viewport[2]) + " x " + std::to_string(viewport[3]));
			logValue("Active asset", m_loadedAsset.name.empty() ? "<none>" : m_loadedAsset.name);
			logValue("Post-MyGUI GL state", "program=" + std::to_string(currentProgram) +
				", VAO=" + std::to_string(vertexArray) + ", activeTexture=" + std::to_string(activeTexture));
			logValue("Definitions and live widgets", std::to_string(m_loadedAsset.widgets.size()) + " / " +
				std::to_string(m_runtimeWidgetLookup.size()));
			for (const GameGUIWidgetDef& definition : m_loadedAsset.widgets)
			{
				const auto runtimeIt = m_runtimeWidgetLookup.find(definition.name);
				const bool hasWidget = runtimeIt != m_runtimeWidgetLookup.end() && runtimeIt->second;
				std::ostringstream details;
				details << "type=" << definition.type
					<< ", authoredVisible=" << (definition.visible ? "yes" : "no")
					<< ", editorVisible=" << (definition.editorVisible ? "yes" : "no")
					<< ", alpha=" << definition.alpha
					<< ", authoredRect=" << definition.x << ',' << definition.y << ' '
					<< definition.width << 'x' << definition.height
					<< ", worldAnchored=" << (definition.worldAnchored ? "yes" : "no")
					<< ", anchor=" << (definition.worldAnchorEntity.empty() ? "<none>" : definition.worldAnchorEntity)
					<< ", submesh=" << definition.worldAnchorSubmeshIndex
					<< ", runtimeWidget=" << (hasWidget ? "yes" : "no");
				if (definition.type == "Image" || definition.type == "ImageBox")
				{
					const std::string skin = definition.solidWhiteBox ? "WhiteSkin"
						: definition.skin.empty() ? "ImageBox" : definition.skin;
					details << ", skin=" << skin
						<< ", skinRegistered=" << (MyGUI::SkinManager::getInstance().isExist(skin) ? "yes" : "no")
						<< ", image=" << (definition.solidWhiteBox ? "solid-white" :
							(definition.texture.empty() ? "<none>" : definition.texture));
					if (!definition.texture.empty())
					{
						const std::filesystem::path imagePath = ResolveGameGUIImagePath(definition.texture);
						std::error_code error;
						details << ", resolvedImage=" << imagePath.string()
							<< ", imageExists=" << (std::filesystem::exists(imagePath, error) && !error ? "yes" : "no");
					}
				}
				if (hasWidget)
				{
					const MyGUI::IntCoord actual = runtimeIt->second->getAbsoluteCoord();
					details << ", runtimeVisible=" << (runtimeIt->second->getVisible() ? "yes" : "no")
						<< ", runtimeRect=" << actual.left << ',' << actual.top << ' '
						<< actual.width << 'x' << actual.height;
					if (const auto* text = dynamic_cast<MyGUI::TextBox*>(runtimeIt->second))
					{
						const std::string runtimeFont(text->getFontName());
						const std::string caption = text->getCaption().asUTF8();
						const MyGUI::IntCoord textRegion = text->getTextRegion();
						const MyGUI::IntSize measuredText = text->getTextSize();
						const MyGUI::IntCoord local = text->getCoord();
						const MyGUI::Colour& colour = text->getTextColour();
						const std::string textSkin = definition.skin.empty() || definition.skin == "TextBox"
							? "TextBoxSkin" : definition.skin;
						const std::string& defaultFont = MyGUI::FontManager::getInstance().getDefaultFont();
						const std::string requestedFont = definition.fontName.empty() ? defaultFont : definition.fontName;
						details << ", savedCaption='" << definition.text << "'"
							<< ", liveCaption='" << caption << "'"
							<< ", captionMatchesSaved=" << (caption == definition.text ? "yes" : "no")
							<< ", captionBytes=" << caption.size()
							<< ", textSkin=" << textSkin
							<< ", textSkinRegistered=" << (MyGUI::SkinManager::getInstance().isExist(textSkin) ? "yes" : "no")
							<< ", requestedFont=" << requestedFont
							<< ", requestedFontRegistered=" << (MyGUI::FontManager::getInstance().getByName(requestedFont) ? "yes" : "no")
							<< ", font=" << (runtimeFont.empty() ? "<empty>" : runtimeFont)
							<< ", fontRegistered=" << (!runtimeFont.empty() && MyGUI::FontManager::getInstance().getByName(runtimeFont) ? "yes" : "no")
							<< ", fontHeight=" << text->getFontHeight()
							<< ", localRect=" << local.left << ',' << local.top << ' ' << local.width << 'x' << local.height
							<< ", textColour=" << colour.red << ' ' << colour.green << ' ' << colour.blue << ' ' << colour.alpha
							<< ", textRegion=" << textRegion.left << ',' << textRegion.top << ' '
							<< textRegion.width << 'x' << textRegion.height
							<< ", measuredText=" << measuredText.width << 'x' << measuredText.height
							<< ", inheritedVisible=" << (text->getInheritedVisible() ? "yes" : "no");
					}
				}
				logValue("Widget " + definition.name, details.str());
			}
			logValue("Snapshot", "end");
		}

		if (ImGui::CollapsingHeader("Text Rendering Diagnostics"))
		{
			const std::string defaultFont = MyGUI::FontManager::getInstance().getDefaultFont();
			ImGui::Text("MyGUI default font: %s (%s)", defaultFont.c_str(),
				MyGUI::FontManager::getInstance().getByName(defaultFont) ? "registered" : "MISSING");
			const std::filesystem::path runtimeLogPath =
				Root::Current().FileSystemRef().ExecutableDirectory() / "logs" / "aquanact-runtime.txt";
			ImGui::TextWrapped("Persistent log: %s", runtimeLogPath.string().c_str());
			if (ImGui::Button("Write Text Diagnostics to Runtime Log"))
			{
				auto& debugger = Root::Current().Debugger();
				debugger.LogTagged("GameGUI-Text", "Text diagnostics snapshot begin");
				debugger.LogTagged("GameGUI-Text", "Active asset='" + m_loadedAsset.name +
					"', defaultFont='" + defaultFont + "'");
				GLint blendSource = 0;
				GLint blendDestination = 0;
				glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSource);
				glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestination);
				debugger.LogTagged("GameGUI-Text",
					std::string("GL vendor='") + (vendor ? reinterpret_cast<const char*>(vendor) : "<unavailable>") +
					"', renderer='" + (renderer ? reinterpret_cast<const char*>(renderer) : "<unavailable>") +
					"', version='" + (version ? reinterpret_cast<const char*>(version) : "<unavailable>") +
					"', blendEnabled=" + (glIsEnabled(GL_BLEND) ? "yes" : "no") +
					", blendSrcAlpha=" + std::to_string(blendSource) +
					", blendDstAlpha=" + std::to_string(blendDestination));
				for (const GameGUIWidgetDef& definition : m_loadedAsset.widgets)
				{
					if (definition.type != "Text" && definition.type != "TextBox")
						continue;
					const auto runtimeIt = m_runtimeWidgetLookup.find(definition.name);
					auto* text = runtimeIt == m_runtimeWidgetLookup.end()
						? nullptr : dynamic_cast<MyGUI::TextBox*>(runtimeIt->second);
					const std::string skin = definition.skin.empty() || definition.skin == "TextBox"
						? "TextBoxSkin" : definition.skin;
					const std::string requestedFont = definition.fontName.empty() ? defaultFont : definition.fontName;
					std::ostringstream details;
					details << "widget='" << definition.name << "', parent='"
						<< (definition.parentName.empty() ? "<none>" : definition.parentName)
						<< "', savedCaption='" << definition.text << "', savedRect="
						<< definition.x << ',' << definition.y << ' ' << definition.width << 'x' << definition.height
						<< ", skin='" << skin << "', skinRegistered="
						<< (MyGUI::SkinManager::getInstance().isExist(skin) ? "yes" : "no")
						<< ", requestedFont='" << requestedFont << "', requestedFontRegistered="
						<< (MyGUI::FontManager::getInstance().getByName(requestedFont) ? "yes" : "no")
						<< ", savedFontHeight=" << definition.fontSize;
					if (text)
					{
						const std::string liveCaption = text->getCaption().asUTF8();
						const std::string liveFont(text->getFontName());
						const MyGUI::IntCoord local = text->getCoord();
						const MyGUI::IntCoord absolute = text->getAbsoluteCoord();
						const MyGUI::IntCoord textRegion = text->getTextRegion();
						const MyGUI::IntSize measured = text->getTextSize();
						const MyGUI::Colour& colour = text->getTextColour();
						MyGUI::IFont* font = liveFont.empty()
							? nullptr : MyGUI::FontManager::getInstance().getByName(liveFont);
						const MyGUI::GlyphInfo* glyph = font
							? font->getGlyphInfo(static_cast<MyGUI::Char>('e')) : nullptr;
						MyGUI::ITexture* atlas = font ? font->getTextureFont() : nullptr;
						details << ", liveCaption='" << liveCaption << "', captionMatchesSaved="
							<< (liveCaption == definition.text ? "yes" : "no")
							<< ", captionBytes=" << liveCaption.size()
							<< ", liveFont='" << (liveFont.empty() ? "<empty>" : liveFont)
							<< "', liveFontRegistered="
							<< (!liveFont.empty() && MyGUI::FontManager::getInstance().getByName(liveFont) ? "yes" : "no")
							<< ", liveFontHeight=" << text->getFontHeight()
							<< ", fontResource=" << (font ? font->getResourceName() : "<missing>")
							<< ", fontDefaultHeight=" << (font ? font->getDefaultHeight() : 0)
							<< ", glyphE=";
						if (glyph)
						{
							details << "codepoint=" << static_cast<unsigned int>(glyph->codePoint)
								<< ", size=" << glyph->width << 'x' << glyph->height
								<< ", advance=" << glyph->advance << ", uv=" << glyph->uvRect.left << ' '
								<< glyph->uvRect.top << ' ' << glyph->uvRect.right << ' ' << glyph->uvRect.bottom;
						}
						else
						{
							details << "MISSING";
						}
						details << ", fontAtlas=";
						if (atlas)
						{
							details << atlas->getName() << ' ' << atlas->getWidth() << 'x' << atlas->getHeight()
								<< ", format=" << atlas->getFormat().getValue()
								<< ", usage=" << atlas->getUsage().getValue()
								<< ", locked=" << (atlas->isLocked() ? "yes" : "no");
							if (const auto* glAtlas = dynamic_cast<MyGUI::OpenGLTexture*>(atlas))
							{
								details << ", glTextureId=" << glAtlas->getTextureId()
									<< ", glTextureObject=" << (glIsTexture(glAtlas->getTextureId()) ? "yes" : "no");
							}
						}
						else
						{
							details << "MISSING";
						}
						details << ", " << InspectFontAtlasOnGPU(atlas, glyph);
						details << ", textColourRGBA=" << colour.red << ' ' << colour.green << ' ' << colour.blue << ' ' << colour.alpha
							<< ", localRect=" << local.left << ',' << local.top << ' ' << local.width << 'x' << local.height
							<< ", absoluteRect=" << absolute.left << ',' << absolute.top << ' ' << absolute.width << 'x' << absolute.height
							<< ", textRegion=" << textRegion.left << ',' << textRegion.top << ' '
							<< textRegion.width << 'x' << textRegion.height
							<< ", measuredCaption=" << measured.width << 'x' << measured.height
							<< ", visible=" << (text->getVisible() ? "yes" : "no")
							<< ", inheritedVisible=" << (text->getInheritedVisible() ? "yes" : "no")
							<< ", alpha=" << text->getAlpha();
					}
					else
					{
						details << ", runtimeTextBox=MISSING";
					}
					debugger.LogTagged("GameGUI-Text", details.str());
				}
				debugger.LogTagged("GameGUI-Text", "Text diagnostics snapshot end");
			}
			bool foundText = false;
			for (const GameGUIWidgetDef& definition : m_loadedAsset.widgets)
			{
				if (definition.type != "Text" && definition.type != "TextBox")
					continue;
				foundText = true;
				ImGui::PushID((definition.name + "##TextDiagnostic").c_str());
				const auto runtimeIt = m_runtimeWidgetLookup.find(definition.name);
				auto* text = runtimeIt == m_runtimeWidgetLookup.end()
					? nullptr : dynamic_cast<MyGUI::TextBox*>(runtimeIt->second);
				if (ImGui::TreeNode(definition.name.c_str(), "%s (parent: %s)%s",
					definition.name.c_str(), definition.parentName.empty() ? "<none>" : definition.parentName.c_str(),
					text ? "" : " [TextBox missing]"))
				{
					const std::string skin = definition.skin.empty() || definition.skin == "TextBox"
						? "TextBoxSkin" : definition.skin;
					const std::string authoredFont = definition.fontName.empty() ? defaultFont : definition.fontName;
					ImGui::Text("Saved caption: '%s' | UTF-8 bytes: %zu", definition.text.c_str(), definition.text.size());
					ImGui::Text("Requested skin: %s (%s)", skin.c_str(),
						MyGUI::SkinManager::getInstance().isExist(skin) ? "registered" : "MISSING");
					ImGui::Text("Requested font: %s (%s) | saved height: %d",
						authoredFont.c_str(), MyGUI::FontManager::getInstance().getByName(authoredFont) ? "registered" : "MISSING",
						definition.fontSize);
					if (text)
					{
						const std::string liveCaption = text->getCaption().asUTF8();
						const std::string liveFont(text->getFontName());
						const MyGUI::IntCoord absolute = text->getAbsoluteCoord();
						const MyGUI::IntCoord textRegion = text->getTextRegion();
						const MyGUI::IntSize measured = text->getTextSize();
						const MyGUI::Colour& colour = text->getTextColour();
						MyGUI::IFont* font = liveFont.empty()
							? nullptr : MyGUI::FontManager::getInstance().getByName(liveFont);
						const MyGUI::GlyphInfo* glyph = font
							? font->getGlyphInfo(static_cast<MyGUI::Char>('e')) : nullptr;
						MyGUI::ITexture* atlas = font ? font->getTextureFont() : nullptr;
						ImGui::Text("Live caption: '%s' | matches saved: %s | UTF-8 bytes: %zu",
							liveCaption.c_str(), liveCaption == definition.text ? "yes" : "NO", liveCaption.size());
						ImGui::Text("Live font: %s (%s) | height: %d",
							liveFont.empty() ? "<empty>" : liveFont.c_str(),
							MyGUI::FontManager::getInstance().getByName(liveFont) ? "registered" : "MISSING",
							text->getFontHeight());
						ImGui::Text("Font resource: %s | default height: %d",
							font ? font->getResourceName().c_str() : "<missing>", font ? font->getDefaultHeight() : 0);
						if (glyph)
						{
							ImGui::Text("Glyph U+0065: codepoint U+%04X | %.1f x %.1f | advance %.1f | UV %.4f %.4f %.4f %.4f",
								static_cast<unsigned int>(glyph->codePoint), glyph->width, glyph->height, glyph->advance,
								glyph->uvRect.left, glyph->uvRect.top, glyph->uvRect.right, glyph->uvRect.bottom);
						}
						else
						{
							ImGui::TextUnformatted("Glyph U+0065: MISSING");
						}
						if (atlas)
						{
							ImGui::Text("Font atlas: %s | %d x %d | format %d | usage %d | locked %s",
								atlas->getName().c_str(), atlas->getWidth(), atlas->getHeight(),
								atlas->getFormat().getValue(), atlas->getUsage().getValue(), atlas->isLocked() ? "yes" : "no");
							if (const auto* glAtlas = dynamic_cast<MyGUI::OpenGLTexture*>(atlas))
							{
								ImGui::Text("Font atlas GL texture: id %u | object %s",
									glAtlas->getTextureId(), glIsTexture(glAtlas->getTextureId()) ? "valid" : "MISSING");
							}
						}
						else
						{
							ImGui::TextUnformatted("Font atlas: MISSING");
						}
						ImGui::Text("Text colour RGBA: %.2f %.2f %.2f %.2f",
							colour.red, colour.green, colour.blue, colour.alpha);
						ImGui::Text("Local rect: %d, %d  %d x %d | absolute rect: %d, %d  %d x %d",
							text->getLeft(), text->getTop(), text->getWidth(), text->getHeight(),
							absolute.left, absolute.top, absolute.width, absolute.height);
						ImGui::Text("Text region: %d, %d  %d x %d | measured caption: %d x %d",
							textRegion.left, textRegion.top, textRegion.width, textRegion.height,
							measured.width, measured.height);
						ImGui::Text("Visible: local %s / inherited %s | alpha %.2f",
							text->getVisible() ? "yes" : "no", text->getInheritedVisible() ? "yes" : "no", text->getAlpha());
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (!foundText)
				ImGui::TextUnformatted("No text widgets in the active GameGUI asset.");
		}

		for (const GameGUIWidgetDef& definition : m_loadedAsset.widgets)
		{
			ImGui::PushID(definition.name.c_str());
			const auto runtimeIt = m_runtimeWidgetLookup.find(definition.name);
			const bool hasRuntimeWidget = runtimeIt != m_runtimeWidgetLookup.end() && runtimeIt->second;
			const bool expanded = ImGui::TreeNode(definition.name.c_str(), "%s (%s)%s",
				definition.name.c_str(), definition.type.c_str(), hasRuntimeWidget ? "" : " [not created]");
			if (expanded)
			{
				ImGui::Text("Authored visible: %s | editor visible: %s | alpha: %.2f",
					definition.visible ? "yes" : "no",
					definition.editorVisible ? "yes" : "no", definition.alpha);
				ImGui::Text("Authored rect: %d, %d  %d x %d | layer: %s",
					definition.x, definition.y, definition.width, definition.height, definition.layer.c_str());
				ImGui::Text("World anchored: %s | anchor: %s | submesh: %d",
					definition.worldAnchored ? "yes" : "no",
					definition.worldAnchorEntity.empty() ? "<none>" : definition.worldAnchorEntity.c_str(),
					definition.worldAnchorSubmeshIndex);
				if (definition.worldAnchored)
				{
					ImGui::Text("World offset: %.2f, %.2f, %.2f",
						definition.worldX, definition.worldY, definition.worldZ);
				}
				if (definition.type == "Image" || definition.type == "ImageBox")
				{
					const std::string skin = definition.solidWhiteBox ? "WhiteSkin"
						: definition.skin.empty() ? "ImageBox" : definition.skin;
					ImGui::Text("MyGUI skin: %s (%s)", skin.c_str(),
						MyGUI::SkinManager::getInstance().isExist(skin) ? "registered" : "MISSING");
					ImGui::Text("Image: %s", definition.solidWhiteBox ? "solid white skin"
						: definition.texture.empty() ? "<no texture>" : definition.texture.c_str());
					if (!definition.texture.empty())
					{
						const std::filesystem::path resolved = ResolveGameGUIImagePath(definition.texture);
						std::error_code error;
						ImGui::Text("Resolved image: %s (%s)", resolved.string().c_str(),
							std::filesystem::exists(resolved, error) && !error ? "exists" : "MISSING");
					}
				}
				if (hasRuntimeWidget)
				{
					const MyGUI::IntCoord actual = runtimeIt->second->getAbsoluteCoord();
					ImGui::Text("Runtime visible: %s | rect: %d, %d  %d x %d",
						runtimeIt->second->getVisible() ? "yes" : "no",
						actual.left, actual.top, actual.width, actual.height);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
	*open = visible;
}

void GameGUI::UpdateWorldAnchoredWidgets()
{
	if (!m_window || m_runtimeWidgetLookup.empty())
		return;
	const bool creatorPreview = Root::Current().State().IsEditorMode()
		&& Root::Current().FrontEnd().FrontEndModeValue() == FrontEndMode::GameGUICreator;

	int viewportWidth = 0;
	int viewportHeight = 0;
	m_window->GetFramebufferSize(viewportWidth, viewportHeight);
	if (viewportWidth <= 0 || viewportHeight <= 0)
		return;

	const Camera& camera = Root::Current().Render().ActiveCamera();
	const glm::mat4 viewProjection = camera.GetProjectionMatrix() * camera.GetViewMatrix();
	for (const GameGUIWidgetDef& definition : m_loadedAsset.widgets)
	{
		if (!definition.worldAnchored)
			continue;

		const auto widgetIt = m_runtimeWidgetLookup.find(definition.name);
		if (widgetIt == m_runtimeWidgetLookup.end())
			continue;
		MyGUI::Widget* anchoredWidget = widgetIt->second;
		auto* image = dynamic_cast<MyGUI::ImageBox*>(anchoredWidget);

		glm::vec3 worldPosition(definition.worldX, definition.worldY, definition.worldZ);
		if (!definition.worldAnchorEntity.empty())
		{
			bool anchorFound = false;
			if (Scene* scene = Root::Current().Scenes().ActiveLevel())
			{
				for (const std::unique_ptr<Entity>& object : scene->Objects())
				{
					if (object && object->Name() == definition.worldAnchorEntity)
					{
						glm::vec3 anchorPosition = object->WorldPosition();
						if (definition.worldAnchorUseSubmesh)
						{
							Mesh* mesh = object->GetMesh();
							if (!mesh || definition.worldAnchorSubmeshIndex < 0
								|| definition.worldAnchorSubmeshIndex >= mesh->NumBuffers())
							{
								break;
							}
							const glm::vec3 localCenter =
								(mesh->SubMeshMinBounds(definition.worldAnchorSubmeshIndex)
									+ mesh->SubMeshMaxBounds(definition.worldAnchorSubmeshIndex)) * 0.5f;
							anchorPosition = glm::vec3(object->BuildModelMatrix() * glm::vec4(localCenter, 1.0f));
						}
						worldPosition = anchorPosition + worldPosition;
						anchorFound = true;
						break;
					}
				}
			}
			if (!anchorFound)
			{
				Root::Current().Debugger().LogOnce(
					"GameGUI-world-anchor-missing-" + definition.name,
					"GameGUI world-anchored widget '" + definition.name +
					"' is hidden because anchor '" + definition.worldAnchorEntity +
					"' or submesh " + std::to_string(definition.worldAnchorSubmeshIndex) + " was not found.");
				anchoredWidget->setVisible(false);
				continue;
			}
		}

		const glm::vec4 clipPosition = viewProjection * glm::vec4(
			worldPosition, 1.0f);
		if (!std::isfinite(clipPosition.w) || clipPosition.w <= 0.0f)
		{
			Root::Current().Debugger().LogOnce(
				"GameGUI-world-anchor-behind-camera-" + definition.name,
				"GameGUI world-anchored widget '" + definition.name + "' is behind the active camera.");
			anchoredWidget->setVisible(false);
			continue;
		}

		const glm::vec3 ndc = glm::vec3(clipPosition) / clipPosition.w;
		const bool inDepthRange = std::isfinite(ndc.x) && std::isfinite(ndc.y) && std::isfinite(ndc.z)
			&& ndc.z >= -1.0f && ndc.z <= 1.0f;
		// Keep a nearby world marker visible when its anchor drifts just beyond
		// the viewport edge. A generous limit still hides anchors far outside the
		// camera's view instead of pinning every marker to the screen border.
		constexpr float maximumEdgeOffsetNdc = 1.5f;
		const bool nearViewport = std::abs(ndc.x) <= maximumEdgeOffsetNdc
			&& std::abs(ndc.y) <= maximumEdgeOffsetNdc;
		const bool inView = inDepthRange && nearViewport;
		if (!inView)
		{
			Root::Current().Debugger().LogOnce(
				"GameGUI-world-anchor-out-of-view-" + definition.name,
				"GameGUI world-anchored widget '" + definition.name + "' is outside the visible edge range (NDC " +
				std::to_string(ndc.x) + ", " + std::to_string(ndc.y) + ", " + std::to_string(ndc.z) + ").");
		}
		const auto visibilityOverride = m_runtimeVisibilityOverrides.find(definition.name);
		const bool visible = creatorPreview
			? definition.editorVisible
			: visibilityOverride == m_runtimeVisibilityOverrides.end()
				? definition.visible
				: visibilityOverride->second;
		anchoredWidget->setVisible(visible && inView);
		if (visible && inView)
		{
			Root::Current().Debugger().LogOnce(
				"GameGUI-world-anchor-visible-" + definition.name,
				"GameGUI world-anchored widget '" + definition.name + "' projects onscreen at NDC " +
				std::to_string(ndc.x) + ", " + std::to_string(ndc.y) + ", " + std::to_string(ndc.z) + ".");
		}
		if (!inView)
			continue;

		// Images emulate perspective size changes in the 2D overlay. Text keeps
		// its authored bounds and font height so its resolution stays independent.
		const float cameraDistance = glm::distance(camera.GetPosition(), worldPosition);
		if (image && std::isfinite(cameraDistance) && cameraDistance > 0.001f)
		{
			constexpr float referenceDistance = 5.0f;
			const int baseWidth = std::max(1, definition.width);
			const int baseHeight = std::max(1, definition.height);
			const float distanceScale = std::clamp(referenceDistance / cameraDistance, 0.1f, 10.0f);
			// Keep the anchored image smaller than the viewport so its position
			// bounds stay ordered even when the authored image is very large or the
			// camera gets close to its anchor.
			const float viewportFitScale = 0.98f * std::min(
				static_cast<float>(viewportWidth) / baseWidth,
				static_cast<float>(viewportHeight) / baseHeight);
			const float fittedScale = std::min(distanceScale, viewportFitScale);
			const int scaledWidth = std::max(1, static_cast<int>(std::round(baseWidth * fittedScale)));
			const int scaledHeight = std::max(1, static_cast<int>(std::round(baseHeight * fittedScale)));
			if (image->getWidth() != scaledWidth || image->getHeight() != scaledHeight)
				image->setSize(scaledWidth, scaledHeight);
			for (const GameGUIWidgetDef& childDefinition : m_loadedAsset.widgets)
			{
				if (childDefinition.parentName != definition.name || childDefinition.worldAnchored)
					continue;
				const auto childIt = m_runtimeWidgetLookup.find(childDefinition.name);
				if (childIt == m_runtimeWidgetLookup.end() || !childIt->second)
					continue;
				childIt->second->setPosition(
					(image->getWidth() - std::max(1, childDefinition.width)) / 2 + childDefinition.x,
					(image->getHeight() - std::max(1, childDefinition.height)) / 2 + childDefinition.y);
			}
		}

		const float halfWidthNdc = static_cast<float>(anchoredWidget->getWidth()) / static_cast<float>(viewportWidth);
		const float halfHeightNdc = static_cast<float>(anchoredWidget->getHeight()) / static_cast<float>(viewportHeight);
		const float minScreenNdcX = std::min(-1.0f + halfWidthNdc, 0.0f);
		const float maxScreenNdcX = std::max(1.0f - halfWidthNdc, 0.0f);
		const float minScreenNdcY = std::min(-1.0f + halfHeightNdc, 0.0f);
		const float maxScreenNdcY = std::max(1.0f - halfHeightNdc, 0.0f);
		const float screenNdcX = std::clamp(ndc.x, minScreenNdcX, maxScreenNdcX);
		const float screenNdcY = std::clamp(ndc.y, minScreenNdcY, maxScreenNdcY);
		if (inView && (screenNdcX != ndc.x || screenNdcY != ndc.y))
		{
			Root::Current().Debugger().LogOnce(
				"GameGUI-world-anchor-clamped-" + definition.name,
				"GameGUI world-anchored widget '" + definition.name + "' was clamped to the viewport edge.");
		}
		const int screenX = static_cast<int>((screenNdcX * 0.5f + 0.5f) * viewportWidth);
		const int screenY = static_cast<int>((0.5f - screenNdcY * 0.5f) * viewportHeight);
		int parentOffsetX = 0;
		int parentOffsetY = 0;
		if (!definition.parentName.empty())
		{
			const auto parentIt = m_runtimeWidgetLookup.find(definition.parentName);
			if (parentIt != m_runtimeWidgetLookup.end())
			{
				const MyGUI::IntCoord parentCoord = parentIt->second->getAbsoluteCoord();
				parentOffsetX = parentCoord.left;
				parentOffsetY = parentCoord.top;
			}
		}
		anchoredWidget->setPosition(screenX - anchoredWidget->getWidth() / 2 - parentOffsetX,
			screenY - anchoredWidget->getHeight() / 2 - parentOffsetY);
	}
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
	m_menuNavigationMode = asset.navigationMode;
	m_boxSkin = asset.boxSkin;
	m_pointerSkin = asset.pointerSkin;
	m_boxPadding = asset.boxPadding;
	m_boxOffsetX = asset.boxOffsetX;
	m_boxOffsetY = asset.boxOffsetY;
	m_pointerWidth = asset.pointerWidth;
	m_pointerHeight = asset.pointerHeight;
	m_pointerGap = asset.pointerGap;
	m_highlightColour = MyGUI::Colour(asset.highlightR, asset.highlightG, asset.highlightB);
	m_selectedColour = MyGUI::Colour(asset.selectedR, asset.selectedG, asset.selectedB);
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

			GameGUIWidgetDef runtimeDefinition = widget;
			runtimeDefinition.x += (parentIt->second->getWidth() - std::max(1, widget.width)) / 2;
			runtimeDefinition.y += (parentIt->second->getHeight() - std::max(1, widget.height)) / 2;
			MyGUI::Widget* createdWidget = CreateWidgetFromDef(runtimeDefinition, parentIt->second);
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
		if (!widget.bindMember.empty() || !widget.bindEvent.empty())
		{
			auto it = m_runtimeWidgetLookup.find(widget.name);
			if (it != m_runtimeWidgetLookup.end())
			{
				BindWidgetFromDef(widget, it->second);
			}
		}
	}

	std::stable_sort(m_allControllerButtons.begin(), m_allControllerButtons.end(), [this](const MyGUI::Button* left, const MyGUI::Button* right)
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
	RefreshVisibleControllerButtons();

	// Keep the navigation pointer separate from the asset so it does not affect
	// layout editing or become an interactive widget in the menu.
	m_menuPointer = m_gui->createWidget<MyGUI::Widget>(
		m_pointerSkin, 0, 0, m_pointerWidth, m_pointerHeight, MyGUI::Align::Default, "Pointer", "__GameGUIMenuPointer");
	if (m_menuPointer)
	{
		m_menuPointer->setNeedMouseFocus(false);
		m_menuPointer->setNeedKeyFocus(false);
		m_menuPointer->setInheritsPick(false);
		m_menuPointer->setVisible(false);
		MyGUI::LayerManager::getInstance().upLayerItem(m_menuPointer);
	}
	// WindowFrameSkin supplies only the themed border; its center remains transparent.
	m_menuBox = m_gui->createWidget<MyGUI::Widget>(m_boxSkin, 0, 0, 100, 30, MyGUI::Align::Default, "Back", "__GameGUIMenuBox");
	if (m_menuBox)
	{
		m_menuBox->setNeedMouseFocus(false);
		m_menuBox->setNeedKeyFocus(false);
		m_menuBox->setInheritsPick(false);
		m_menuBox->setVisible(false);
	}
}

void GameGUI::ClearUI()
{
	m_deathOverlayVisible = false;
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
		m_menuBox = nullptr;
		m_runtimeWidgets.clear();
		m_subPanelHistory.clear();
		m_allControllerButtons.clear();
		m_controllerButtons.clear();
		m_runtimeWidgetLookup.clear();
		m_runtimeVisibilityOverrides.clear();
		m_runtimeVisibilitySources.clear();
		m_buttonDefaultTextColours.clear();
		m_buttonLabels.clear();
		m_buttonFocusSounds.clear();
		m_focusedControllerButton = -1;
		m_lastFocusSoundButton = nullptr;
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
	if (m_menuBox)
	{
		m_gui->destroyWidget(m_menuBox);
	}
	m_menuPointer = nullptr;
	m_menuBox = nullptr;
	m_buttonDefaultTextColours.clear();
	m_buttonLabels.clear();
	m_buttonFocusSounds.clear();
	m_runtimeWidgets.clear();
	m_subPanelHistory.clear();
	m_allControllerButtons.clear();
	m_controllerButtons.clear();
	m_runtimeWidgetLookup.clear();
	m_runtimeVisibilityOverrides.clear();
	m_runtimeVisibilitySources.clear();
	m_focusedControllerButton = -1;
	m_lastFocusSoundButton = nullptr;
}

MyGUI::Widget* GameGUI::CreateWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* parent)
{
	if (def.type == "Panel")
	{
		return CreatePanelWidget(def, parent);
	}
	if (def.type == "Button")
	{
		return CreateButtonWidget(def, parent);
	}
	if (def.type == "Icon")
	{
		return CreateIconWidget(def, parent);
	}
	if (def.type == "TextBox" || def.type == "Text")
	{
		return CreateTextWidget(def, parent);
	}
	if (def.type == "ImageBox" || def.type == "Image")
	{
		return CreateImageWidget(def, parent);
	}
	if (def.type == "ProgressBar")
	{
		return CreateProgressBarWidget(def, parent);
	}
	return nullptr;
}

void GameGUI::FocusFirstControllerButton()
{
	RefreshVisibleControllerButtons();
	if (m_controllerButtons.empty())
	{
		return;
	}

	ClearControllerFocus();
	m_focusedControllerButton = 0;
	PositionMenuPointer(m_controllerButtons[0]);
}

void GameGUI::RefreshVisibleControllerButtons()
{
	ClearControllerFocus();
	m_controllerButtons.clear();
	for (MyGUI::Button* button : m_allControllerButtons)
	{
		if (button && button->getInheritedVisible())
		{
			m_controllerButtons.push_back(button);
		}
	}
	if (m_menuPointer)
	{
		m_menuPointer->setVisible(false);
	}
	if (m_menuBox)
	{
		m_menuBox->setVisible(false);
	}
}

void GameGUI::FocusFirstControllerButtonInPanel(const std::string& panelName)
{
	for (std::size_t i = 0; i < m_controllerButtons.size(); ++i)
	{
		const GameGUIWidgetDef* buttonDef = FindWidgetDef(m_loadedAsset, m_controllerButtons[i]->getName());
		const GameGUIWidgetDef* panelDef = buttonDef ? GameGUIAssetUtils::FindOwningPanel(m_loadedAsset, *buttonDef) : nullptr;
		if (panelDef && panelDef->name == panelName)
		{
			m_focusedControllerButton = static_cast<int>(i);
			PositionMenuPointer(m_controllerButtons[i]);
			return;
		}
	}
}

bool GameGUI::NavigateBackFromSubPanel()
{
	if (m_subPanelHistory.empty())
	{
		return false;
	}

	const auto [currentPanel, previousPanel] = m_subPanelHistory.back();
	m_subPanelHistory.pop_back();
	MyGUI::Widget* currentWidget = RuntimeWidget(currentPanel);
	MyGUI::Widget* previousWidget = RuntimeWidget(previousPanel);
	if (!currentWidget || !previousWidget)
	{
		return false;
	}

	currentWidget->setVisible(false);
	previousWidget->setVisible(true);
	RefreshVisibleControllerButtons();
	const Input& input = Root::Current().InputRef();
	if (input.ControllerConnected() && input.ActiveDevice() == Input::ActiveInputDevice::Gamepad)
	{
		FocusFirstControllerButtonInPanel(previousPanel);
	}
	return true;
}

void GameGUI::ClearControllerFocus()
{
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()))
	{
		MyGUI::Button* button = m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)];
		ApplyTextHighlight(button, false);
	}
	m_focusedControllerButton = -1;
}

void GameGUI::RelinquishControllerFocusToMouse()
{
	MyGUI::Button* controllerButton = nullptr;
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()))
	{
		controllerButton = m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)];
	}

	// Mouse movement has already been injected into MyGUI by Input. Drop only
	// the controller-selection bookkeeping; MyGUI remains the owner of real
	// mouse hover focus.
	m_focusedControllerButton = -1;
	MyGUI::Button* hoveredButton = dynamic_cast<MyGUI::Button*>(
		MyGUI::InputManager::getInstance().getMouseFocusWidget());
	if (controllerButton && controllerButton != hoveredButton)
	{
		ApplyTextHighlight(controllerButton, false);
	}
	if (hoveredButton)
	{
		PositionMenuPointer(hoveredButton);
	}
	else
	{
		if (m_menuPointer) m_menuPointer->setVisible(false);
		if (m_menuBox) m_menuBox->setVisible(false);
	}
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
	ApplyTextHighlight(m_controllerButtons[static_cast<std::size_t>(previous)], false);
	PositionMenuPointer(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
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

void GameGUI::OnButtonMouseLostFocus(MyGUI::Widget* sender, MyGUI::Widget* newFocus)
{
	if (auto* button = dynamic_cast<MyGUI::Button*>(sender))
	{
		ApplyTextHighlight(button, false);
	}
	// Moving directly to another menu button should keep the pointer visible.
	if (m_focusedControllerButton < 0 && !dynamic_cast<MyGUI::Button*>(newFocus) && m_menuPointer)
	{
		m_menuPointer->setVisible(false);
	}
}

void GameGUI::PositionMenuPointer(MyGUI::Widget* button)
{
	MyGUI::Button* menuButton = dynamic_cast<MyGUI::Button*>(button);
	if (!menuButton)
	{
		return;
	}
	if (m_lastFocusSoundButton != menuButton && Root::Current().State().IsGameMode())
	{
		const auto soundIt = m_buttonFocusSounds.find(menuButton);
		if (soundIt != m_buttonFocusSounds.end())
		{
			Audio::PlayUISound(soundIt->second, 65.0f);
		}
		m_lastFocusSoundButton = menuButton;
	}
	ApplyTextHighlight(menuButton, true);
	if (m_menuPointer)
	{
		m_menuPointer->setVisible(false);
	}
	if (m_menuBox)
	{
		m_menuBox->setVisible(false);
	}
	if (m_menuNavigationMode == MenuNavigationMode::TextHighlight)
	{
		return;
	}
	const MyGUI::IntCoord buttonCoord = menuButton->getAbsoluteCoord();
	if (m_menuNavigationMode == MenuNavigationMode::Boxed)
	{
		if (m_menuBox)
		{
			m_menuBox->setCoord(buttonCoord.left - m_boxPadding + m_boxOffsetX,
				buttonCoord.top - m_boxPadding + m_boxOffsetY,
				buttonCoord.width + m_boxPadding * 2,
				buttonCoord.height + m_boxPadding * 2);
			m_menuBox->setVisible(true);
		}
		return;
	}
	if (!m_menuPointer)
	{
		return;
	}

	const int pointerWidth = m_pointerWidth;
	const int pointerHeight = m_pointerHeight;
	const int gap = m_pointerGap;
	const int pointerX = std::max(0, buttonCoord.left - pointerWidth - gap);
	const int pointerY = buttonCoord.top + (buttonCoord.height - pointerHeight) / 2;
	m_menuPointer->setCoord(pointerX, pointerY, pointerWidth, pointerHeight);
	m_menuPointer->setVisible(true);
	MyGUI::LayerManager::getInstance().upLayerItem(m_menuPointer);
}

void GameGUI::ApplyTextHighlight(MyGUI::Button* button, bool highlighted)
{
	if (!button || m_menuNavigationMode != MenuNavigationMode::TextHighlight)
	{
		return;
	}
	const auto it = m_buttonDefaultTextColours.find(button);
	if (it == m_buttonDefaultTextColours.end())
	{
		return;
	}
	const MyGUI::Colour colour = highlighted
		? (button->getStateSelected() ? m_selectedColour : m_highlightColour)
		: it->second;
	button->setTextColour(colour);
	const auto label = m_buttonLabels.find(button);
	if (label != m_buttonLabels.end() && label->second)
	{
		label->second->setTextColour(colour);
	}
}

void GameGUI::SetMenuNavigationMode(MenuNavigationMode mode)
{
	if (m_menuNavigationMode == mode)
	{
		return;
	}
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()))
	{
		ApplyTextHighlight(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)], false);
	}
	m_menuNavigationMode = mode;
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()))
	{
		PositionMenuPointer(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
	}
}

void GameGUI::SetBoxStyle(int padding, int offsetX, int offsetY)
{
	m_boxPadding = std::max(0, padding);
	m_boxOffsetX = offsetX;
	m_boxOffsetY = offsetY;
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()) && m_menuNavigationMode == MenuNavigationMode::Boxed)
	{
		PositionMenuPointer(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
	}
}

void GameGUI::SetBoxSkin(const std::string& skin)
{
	if (!m_gui || skin.empty() || skin == m_boxSkin)
	{
		return;
	}
	m_boxSkin = skin;
	if (m_menuBox)
	{
		m_gui->destroyWidget(m_menuBox);
	}
	m_menuBox = m_gui->createWidget<MyGUI::Widget>(m_boxSkin, 0, 0, 100, 30, MyGUI::Align::Default, "Back", "__GameGUIMenuBox");
	if (m_menuBox)
	{
		m_menuBox->setNeedMouseFocus(false);
		m_menuBox->setNeedKeyFocus(false);
		m_menuBox->setInheritsPick(false);
		m_menuBox->setVisible(false);
	}
	if (m_focusedControllerButton >= 0 && m_focusedControllerButton < static_cast<int>(m_controllerButtons.size()) && m_menuNavigationMode == MenuNavigationMode::Boxed)
	{
		PositionMenuPointer(m_controllerButtons[static_cast<std::size_t>(m_focusedControllerButton)]);
	}
}
void GameGUI::SetPointerStyle(int width, int height, int gap) { m_pointerWidth = std::max(8, width); m_pointerHeight = std::max(8, height); m_pointerGap = std::max(0, gap); }
void GameGUI::SetHighlightColour(float r, float g, float b) { m_highlightColour = MyGUI::Colour(r, g, b); }
void GameGUI::SetPointerSkin(const std::string& skin)
{
	if (skin.empty() || skin == m_pointerSkin) return;
	m_pointerSkin = skin;
	if (!m_gui) return;
	if (m_menuPointer) m_gui->destroyWidget(m_menuPointer);
	m_menuPointer = m_gui->createWidget<MyGUI::Widget>(m_pointerSkin, 0, 0, m_pointerWidth, m_pointerHeight, MyGUI::Align::Default, "Pointer", "__GameGUIMenuPointer");
	if (m_menuPointer)
	{
		m_menuPointer->setNeedMouseFocus(false);
		m_menuPointer->setNeedKeyFocus(false);
		m_menuPointer->setInheritsPick(false);
		m_menuPointer->setVisible(false);
	}
}

MyGUI::Widget* GameGUI::RuntimeWidget(const std::string& name) const
{
	auto it = m_runtimeWidgetLookup.find(name);
	return it != m_runtimeWidgetLookup.end() ? it->second : nullptr;
}

void GameGUI::SetRuntimeWidgetVisible(const std::string& name, bool visible, const std::string& source)
{
	MyGUI::Widget* widget = RuntimeWidget(name);
	if (!widget)
		return;

	auto& activeSources = m_runtimeVisibilitySources[name];
	if (visible)
		activeSources.insert(source);
	else
		activeSources.erase(source);
	const bool hasActiveSource = !activeSources.empty();
	if (hasActiveSource)
	{
		m_runtimeVisibilityOverrides[name] = true;
	}
	else
	{
		m_runtimeVisibilitySources.erase(name);
		m_runtimeVisibilityOverrides.erase(name);
	}

	const auto definitionIt = std::find_if(m_loadedAsset.widgets.begin(), m_loadedAsset.widgets.end(),
		[&name](const GameGUIWidgetDef& definition) { return definition.name == name; });
	const bool defaultVisible = definitionIt != m_loadedAsset.widgets.end() && definitionIt->visible;
	widget->setVisible(hasActiveSource || defaultVisible);
}

void GameGUI::BindWidgetFromDef(const GameGUIWidgetDef& def, MyGUI::Widget* widget)
{
	// Check whether any binding data exists before we inspect the widget type.
	if (!widget || def.bindEntity.empty() || def.bindComponent.empty())
	{
		return;
	}

	if (auto* progress = dynamic_cast<MyGUI::ProgressBar*>(widget))
	{
		BindProgressBarFromDef(def, progress);
		return;
	}

	auto* textWidget = dynamic_cast<MyGUI::TextBox*>(widget);
	if (textWidget)
	{
		BindTextWidgetFromDef(def, textWidget);
		return;
	}

	if (!def.bindingEnabled || def.bindMember.empty())
		return;
	if (Root::Current().State().IsEditorMode()
		&& Root::Current().FrontEnd().FrontEndModeValue() == FrontEndMode::GameGUICreator)
	{
		// The HUD editor preview uses editorVisible as its authored visibility.
		// Keep runtime visibility bindings from hiding a widget during editing.
		return;
	}

	// Non-text widgets use a boolean bindable to control visibility. This keeps
	// static images and icons configurable without giving them button behavior.
	BindValueWidgetRefresh(def, widget, [widget](float visible)
	{
		widget->setVisible(visible >= 0.5f);
	});
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
		// Capture before resolving or reloading the destination. Those operations
		// can be synchronous, so the cursor must already be hidden while the game
		// launch is in progress.
		Root::Current().InputRef().CaptureCursorForLevel();
		Root::Current().FrontEnd().RuntimeGUI().HideAll();
		Scene* targetScene = FindNamedLevel(Root::Current().Scenes(), launchLevel);
		if (!targetScene && !launchLevel.empty())
		{
			Scene* namedScene = Root::Current().Scenes().FindLevel(launchLevel);
			if (namedScene && Root::Current().Scenes().SceneKindFor(launchLevel) == SceneManager::SceneKind::Cutscene)
				targetScene = namedScene;
		}
		Root::Current().Debugger().LogMessage(
			"GameGUI NewGame initial scene lookup: " +
			std::string(targetScene ? targetScene->Name() : "<not found>"));
		std::cout << "GameGUI NewGame initial scene lookup: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		if (!targetScene)
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame reloading project to retry scene lookup.");
			std::cout << "GameGUI NewGame reloading project to retry scene lookup.\n";
			if (!Root::Current().Projects().LoadProject(Root::Current().Projects().CurrentProjectPath(), Root::Current().Scenes()))
			{
				Root::Current().Debugger().LogMessage("GameGUI NewGame failed: could not reload the current project.");
				break;
			}
			targetScene = FindNamedLevel(Root::Current().Scenes(), launchLevel);
			Root::Current().Debugger().LogMessage(
				"GameGUI NewGame post-reload scene lookup: " +
				std::string(targetScene ? targetScene->Name() : "<not found>"));
			std::cout << "GameGUI NewGame post-reload scene lookup: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		}
		if (!targetScene)
		{
			targetScene = FindPlayableScene(Root::Current().Scenes());
			Root::Current().Debugger().LogMessage(
				"GameGUI NewGame fallback playable scene: " +
				std::string(targetScene ? targetScene->Name() : "<not found>"));
			std::cout << "GameGUI NewGame fallback playable scene: " << (targetScene ? targetScene->Name() : "<not found>") << '\n';
		}
		if (targetScene && Root::Current().Scenes().SceneKindFor(targetScene->Name()) == SceneManager::SceneKind::Cutscene)
		{
			std::string nextLevel = Root::Current().Scenes().StartupLevelName();
			if (nextLevel.empty() || Root::Current().Scenes().SceneKindFor(nextLevel) != SceneManager::SceneKind::Level)
			{
				if (Scene* fallbackLevel = FindPlayableScene(Root::Current().Scenes()))
					nextLevel = fallbackLevel->Name();
			}
			if (!Root::Current().Gameplay().StartCutscene(targetScene->Name(), nextLevel, Root::Current().FrontEnd(), Root::Current().Debugger()))
			{
				Root::Current().Debugger().LogMessage("GameGUI NewGame failed: cutscene target or next level is invalid.");
				break;
			}
			Root::Current().FrontEnd().RuntimeGUI().RecordClick("New Game cutscene started");
			break;
		}
		if (targetScene)
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame launching scene: " + targetScene->Name());
			std::cout << "GameGUI NewGame launching scene: " << targetScene->Name() << '\n';
			Root::Current().Scenes().SetActiveLevel(targetScene->Name());
			Root::Current().Scenes().SetStartupLevelName(targetScene->Name());
		}
		if (!Root::Current().Gameplay().BootPlayableLevel(Root::Current().FrontEnd(), Root::Current().Debugger()))
		{
			Root::Current().Debugger().LogMessage("GameGUI NewGame failed: no playable Scene could be selected.");
			break;
		}
		Root::Current().FrontEnd().RuntimeGUI().RecordClick("New Game started");
		break;
	}
	case GameGUIActionType::Pause:
		Root::Current().Gameplay().ExecuteCommand(
			GameplayCommand::Pause,
			Root::Current().FrontEnd(),
			Root::Current().Debugger());
		Root::Current().FrontEnd().RuntimeGUI().RecordClick("Pause action requested");
		Root::Current().Debugger().LogMessage("GameGUI Pause action requested");
		break;
	case GameGUIActionType::Resume:
		Root::Current().InputActions().SuppressControllerInputUntilRelease();
		Root::Current().Gameplay().ExecuteCommand(
			GameplayCommand::Resume,
			Root::Current().FrontEnd(),
			Root::Current().Debugger());
		Root::Current().FrontEnd().RuntimeGUI().RecordClick("Resume action requested");
		Root::Current().Debugger().LogMessage("GameGUI Resume action requested");
		break;
	case GameGUIActionType::SubPanel:
	{
		const GameGUIPanelTransition transition = GameGUIAssetUtils::ResolveSubPanelTransition(m_loadedAsset, name);
		MyGUI::Widget* sourcePanel = transition.sourcePanel ? RuntimeWidget(transition.sourcePanel->name) : nullptr;
		MyGUI::Widget* targetPanel = transition.targetPanel ? RuntimeWidget(transition.targetPanel->name) : nullptr;
		if (!transition || !sourcePanel || !targetPanel)
		{
			const std::string reason = transition.error.empty() ? "runtime Panel widget is missing" : transition.error;
			const std::string message = "GameGUI SubPanel action ignored for button " + name + ": " + reason;
			Root::Current().FrontEnd().RuntimeGUI().RecordClick(message);
			Root::Current().Debugger().LogMessage(message);
			break;
		}

		sourcePanel->setVisible(false);
		targetPanel->setVisible(true);
		m_subPanelHistory.emplace_back(transition.targetPanel->name, transition.sourcePanel->name);
		RefreshVisibleControllerButtons();
		const Input& input = Root::Current().InputRef();
		if (input.ControllerConnected() && input.ActiveDevice() == Input::ActiveInputDevice::Gamepad)
		{
			FocusFirstControllerButtonInPanel(transition.targetPanel->name);
		}
		const std::string message = "Sub Panel opened: " + transition.sourcePanel->name + " -> " + transition.targetPanel->name;
		Root::Current().FrontEnd().RuntimeGUI().RecordClick(message);
		Root::Current().Debugger().LogMessage("GameGUI " + message);
		break;
	}
	case GameGUIActionType::None:
	default:
		break;
	}
}







