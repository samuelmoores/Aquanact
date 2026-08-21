#include "Engine/UI/GameGUICreatorView.h"
#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Camera.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <functional>

void GameGUICreatorView::DrawWidgetList(GameGUICreator& creator)
{
	ImGui::Begin("Widget List", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	GameGUIAsset& asset = creator.CurrentGameGUI();
	creator.RefreshActiveEditingPanel();
	if (!creator.CurrentGameGUI().widgets.empty())
	{
		const auto hasChildren = [&asset](const GameGUIWidgetDef& widget)
		{
			return std::any_of(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& child)
			{
				return child.parentName == widget.name;
			});
		};

		std::function<void(const std::string&)> drawChildren;
		drawChildren = [&](const std::string& parentName)
		{
			for (std::size_t i = 0; i < asset.widgets.size(); ++i)
			{
				GameGUIWidgetDef& widget = asset.widgets[i];
				if (widget.parentName != parentName)
				{
					continue;
				}

				const bool selected = creator.m_selectedWidgetIndex == static_cast<int>(i);
				const bool widgetHasChildren = hasChildren(widget);
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (selected)
				{
					flags |= ImGuiTreeNodeFlags_Selected;
				}
				if (!widgetHasChildren)
				{
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				std::string annotation;
				if (widget.type == "Panel")
				{
					annotation = widget.visible ? " [visible on load]" : " [hidden on load]";
				}
				else if (widget.type == "Button" && widget.action == GameGUIActionType::SubPanel)
				{
					annotation = " -> " + (widget.targetPanel.empty() ? std::string("<missing panel>") : widget.targetPanel);
				}
				const std::string label = widget.name + " (" + widget.type + ")" + annotation + "##WidgetTree" + std::to_string(i);
				const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					creator.m_selectedWidgetIndex = static_cast<int>(i);
					const std::string panelName = creator.OwningPanelName(asset, widget);
					if (!panelName.empty() && panelName != creator.m_activeEditingPanel)
					{
						creator.m_activeEditingPanel = panelName;
						creator.SyncRuntimePreview();
					}
				}

				if (widgetHasChildren && open)
				{
					drawChildren(widget.name);
					ImGui::TreePop();
				}
			}
		};

		drawChildren("");
	}
	else
	{
		ImGui::TextUnformatted("No widgets.");
	}
	ImGui::End();
}

void GameGUICreatorView::DrawWidgetDetails(GameGUICreator& creator)
{
	ImGui::Begin("Widget Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (creator.CurrentGameGUI().widgets.empty())
	{
		ImGui::TextUnformatted("No widget selected.");
		ImGui::End();
		return;
	}

	GameGUIAsset& asset = creator.CurrentGameGUI();
	if (creator.m_selectedWidgetIndex < 0 || creator.m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		ImGui::TextUnformatted("No widget selected.");
		ImGui::End();
		return;
	}

	GameGUIWidgetDef& widget = asset.widgets[static_cast<std::size_t>(creator.m_selectedWidgetIndex)];

	if (widget.type == "Button")
	{
		creator.DrawButtonWidgetDetails(asset, widget);
	}
	else if (widget.type == "Panel")
	{
		creator.DrawPanelWidgetDetails(asset, widget);
	}
	else if (widget.type == "ImageBox" || widget.type == "Image")
	{
		creator.DrawImageWidgetDetails(asset, widget);
	}
	else if (widget.type == "ProgressBar")
	{
		DrawProgressBarWidgetDetails(creator, asset, widget);
	}
	else if (widget.type == "TextBox" || widget.type == "Text")
	{
		creator.DrawTextWidgetDetails(asset, widget);
	}

	ImGui::Separator();
	if (ImGui::Button("Delete Widget"))
	{
		creator.DeleteSelectedWidget();
	}

	ImGui::End();
}

void GameGUICreatorView::Draw(GameGUICreator& creator, const Camera&)
{
	if (!creator.m_initialized)
	{
		return;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::BeginMenu("GameGUIs"))
			{
				for (std::size_t i = 0; i < creator.m_assets.size(); ++i)
				{
					const bool selected = creator.GUIIndex(creator.m_selectedGUI) == i;
					if (ImGui::MenuItem(creator.GUIName(static_cast<GameGUICreator::GUIRole>(i)), nullptr, selected))
					{
						creator.SelectGUIAsset(creator.m_assets[i].name);
						creator.SyncRuntimePreview();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Navigation Mode"))
			{
				const bool pointerSelected = creator.m_menuNavigationMode == GameGUICreator::MenuNavigationMode::Pointer;
				const bool textSelected = creator.m_menuNavigationMode == GameGUICreator::MenuNavigationMode::TextHighlight;
				const bool boxedSelected = creator.m_menuNavigationMode == GameGUICreator::MenuNavigationMode::Boxed;
				if (ImGui::MenuItem("Pointer", nullptr, pointerSelected))
				{
					creator.SetMenuNavigationMode(GameGUICreator::MenuNavigationMode::Pointer);
				}
				if (ImGui::MenuItem("Text Highlight", nullptr, textSelected))
				{
					creator.SetMenuNavigationMode(GameGUICreator::MenuNavigationMode::TextHighlight);
				}
				if (ImGui::MenuItem("Boxed", nullptr, boxedSelected))
				{
					creator.SetMenuNavigationMode(GameGUICreator::MenuNavigationMode::Boxed);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Leave Creator"))
			{
				Root::Current().FrontEnd().ReturnToEngineGUIEditor();
				Root::Current().Render().SetActiveCamera(Root::Current().Render().GetEngineCamera());
				Root::Current().Debugger().LogMessage("Leave Creator requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Current GUI"))
			{
				creator.SaveSelectedRoleGUI();
				Root::Current().Debugger().LogMessage("Save Current GUI requested");
			}
			if (ImGui::MenuItem("Load Current GUI"))
			{
				creator.LoadSelectedRoleGUI();
				creator.SyncRuntimePreview();
				Root::Current().Debugger().LogMessage("Load Current GUI requested");
			}
			ImGui::EndMenu();
		}
			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Create Button"))
				{
					creator.OpenCreateWidgetPopup(GameGUICreator::NewWidgetType::Button);
				}
				if (ImGui::MenuItem("Create Image"))
				{
					creator.OpenCreateWidgetPopup(GameGUICreator::NewWidgetType::Image);
					std::snprintf(creator.m_newWidgetTexture, sizeof(creator.m_newWidgetTexture), "textures/example.png");
				}
				if (ImGui::MenuItem("Create Progress Bar"))
				{
					creator.OpenCreateWidgetPopup(GameGUICreator::NewWidgetType::ProgressBar);
					std::snprintf(creator.m_newWidgetTexture, sizeof(creator.m_newWidgetTexture), "textures/progress_fill.png");
				}
				if (ImGui::MenuItem("Create Panel"))
				{
					creator.OpenCreateWidgetPopup(GameGUICreator::NewWidgetType::Panel);
				}
				if (ImGui::MenuItem("Create Text"))
				{
					creator.OpenCreateWidgetPopup(GameGUICreator::NewWidgetType::Text);
				}
				ImGui::EndMenu();
			}
		ImGui::EndMainMenuBar();
	}

	ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	if (ImGui::Begin("Navigation Mode", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Choose how the active menu button is shown.");
		ImGui::Separator();

		int mode = static_cast<int>(creator.m_menuNavigationMode);
		if (ImGui::RadioButton("Pointer", mode == static_cast<int>(GameGUICreator::MenuNavigationMode::Pointer)))
		{
			mode = static_cast<int>(GameGUICreator::MenuNavigationMode::Pointer);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Text Highlight", mode == static_cast<int>(GameGUICreator::MenuNavigationMode::TextHighlight)))
		{
			mode = static_cast<int>(GameGUICreator::MenuNavigationMode::TextHighlight);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Boxed", mode == static_cast<int>(GameGUICreator::MenuNavigationMode::Boxed)))
		{
			mode = static_cast<int>(GameGUICreator::MenuNavigationMode::Boxed);
		}

		const GameGUICreator::MenuNavigationMode selectedMode = static_cast<GameGUICreator::MenuNavigationMode>(mode);
		if (selectedMode != creator.m_menuNavigationMode)
		{
			creator.SetMenuNavigationMode(selectedMode);
		}
		if (creator.m_menuNavigationMode == GameGUICreator::MenuNavigationMode::Boxed)
		{
			GameGUIAsset& asset = creator.CurrentGameGUI();
			ImGui::PushItemWidth(120.0f);
			const bool paddingChanged = ImGui::SliderInt("Box padding", &creator.m_boxPadding, 0, 40);
			const bool offsetXChanged = ImGui::SliderInt("Box offset X", &creator.m_boxOffsetX, -40, 40);
			const bool offsetYChanged = ImGui::SliderInt("Box offset Y", &creator.m_boxOffsetY, -40, 40);
			ImGui::PopItemWidth();
			if (paddingChanged || offsetXChanged || offsetYChanged)
			{
				asset.boxPadding = creator.m_boxPadding;
				asset.boxOffsetX = creator.m_boxOffsetX;
				asset.boxOffsetY = creator.m_boxOffsetY;
				creator.SyncRuntimePreview();
			}
			static constexpr const char* boxSkins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
			const char* skinNames[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
			ImGui::PushItemWidth(170.0f);
			const bool boxSkinChanged = ImGui::Combo("Box skin", &creator.m_boxSkinIndex, skinNames, IM_ARRAYSIZE(skinNames));
			ImGui::PopItemWidth();
			if (boxSkinChanged)
			{
				asset.boxSkin = boxSkins[creator.m_boxSkinIndex];
				creator.SyncRuntimePreview();
			}
		}
		else if (creator.m_menuNavigationMode == GameGUICreator::MenuNavigationMode::Pointer)
		{
			GameGUIAsset& asset = creator.CurrentGameGUI();
			const bool sizeChanged = ImGui::SliderInt("Pointer size", &creator.m_pointerWidth, 8, 100);
			const bool offsetChanged = ImGui::SliderInt("Pointer offset", &creator.m_pointerGap, 0, 80);
			if (sizeChanged || offsetChanged)
			{
				creator.m_pointerHeight = creator.m_pointerWidth;
				asset.pointerWidth = creator.m_pointerWidth;
				asset.pointerHeight = creator.m_pointerWidth;
				asset.pointerGap = creator.m_pointerGap;
				creator.SyncRuntimePreview();
			}
			const char* pointerSkins[] = { "NavigationArrowRight1", "NavigationArrowRight2", "NavigationArrowRight3", "NavigationArrowRight4" };
			if (ImGui::Combo("Pointer skin", &creator.m_pointerSkinIndex, pointerSkins, IM_ARRAYSIZE(pointerSkins)))
			{
				asset.pointerSkin = pointerSkins[creator.m_pointerSkinIndex];
				creator.SyncRuntimePreview();
			}
		}
		else
		{
			GameGUIAsset& asset = creator.CurrentGameGUI();
			float highlightColour[3] = { creator.m_highlightR, creator.m_highlightG, creator.m_highlightB };
			float selectedColour[3] = { creator.m_selectedR, creator.m_selectedG, creator.m_selectedB };
			const bool highlightChanged = ImGui::ColorEdit3("Highlight color", highlightColour);
			const bool selectedChanged = ImGui::ColorEdit3("Selected color", selectedColour);
			if (highlightChanged || selectedChanged)
			{
				creator.m_highlightR = highlightColour[0]; creator.m_highlightG = highlightColour[1]; creator.m_highlightB = highlightColour[2];
				creator.m_selectedR = selectedColour[0]; creator.m_selectedG = selectedColour[1]; creator.m_selectedB = selectedColour[2];
				asset.highlightR = creator.m_highlightR;
				asset.highlightG = creator.m_highlightG;
				asset.highlightB = creator.m_highlightB;
				asset.selectedR = creator.m_selectedR; asset.selectedG = creator.m_selectedG; asset.selectedB = creator.m_selectedB;
				creator.SyncRuntimePreview();
			}
		}
	}
	ImGui::End();

	creator.DrawCreateWidgetPopup();
	creator.DrawBindingPopup();
	DrawWidgetList(creator);
	DrawWidgetDetails(creator);
}
