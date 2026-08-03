#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Camera.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <functional>

void GameGUICreator::Draw(const Camera&)
{
	if (!m_initialized)
	{
		return;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::BeginMenu("Navigation Mode"))
			{
				const bool pointerSelected = m_menuNavigationMode == MenuNavigationMode::Pointer;
				const bool textSelected = m_menuNavigationMode == MenuNavigationMode::TextHighlight;
				const bool boxedSelected = m_menuNavigationMode == MenuNavigationMode::Boxed;
				if (ImGui::MenuItem("Pointer", nullptr, pointerSelected))
				{
					SetMenuNavigationMode(MenuNavigationMode::Pointer);
				}
				if (ImGui::MenuItem("Text Highlight", nullptr, textSelected))
				{
					SetMenuNavigationMode(MenuNavigationMode::TextHighlight);
				}
				if (ImGui::MenuItem("Boxed", nullptr, boxedSelected))
				{
					SetMenuNavigationMode(MenuNavigationMode::Boxed);
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
				SaveSelectedRoleGUI();
				Root::Current().Debugger().LogMessage("Save Current GUI requested");
			}
			if (ImGui::MenuItem("Load Current GUI"))
			{
				LoadSelectedRoleGUI();
				SyncRuntimePreview();
				Root::Current().Debugger().LogMessage("Load Current GUI requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Create Button"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = false;
				m_newWidgetIsPanel = false;
				m_newButtonParentPanel.clear();
				m_newWidgetAction = GameGUIActionType::None;
				m_newWidgetLaunchLevel.clear();
				m_newWidgetName[0] = '\0';
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Image"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = true;
				m_newWidgetIsProgressBar = false;
				m_newWidgetIsPanel = false;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			if (ImGui::MenuItem("Create Progress Bar"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = true;
				m_newWidgetIsPanel = false;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			if (ImGui::MenuItem("Create Panel"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = false;
				m_newWidgetIsPanel = true;
				m_newWidgetName[0] = '\0';
				m_newWidgetTexture[0] = '\0';
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Let ImGui fit this utility window to the controls it contains.
	ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	if (ImGui::Begin("Navigation Mode"))
	{
		ImGui::TextUnformatted("Choose how the active menu button is shown.");
		ImGui::Separator();

		int mode = static_cast<int>(m_menuNavigationMode);
		if (ImGui::RadioButton("Pointer", mode == static_cast<int>(MenuNavigationMode::Pointer)))
		{
			mode = static_cast<int>(MenuNavigationMode::Pointer);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Text Highlight", mode == static_cast<int>(MenuNavigationMode::TextHighlight)))
		{
			mode = static_cast<int>(MenuNavigationMode::TextHighlight);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Boxed", mode == static_cast<int>(MenuNavigationMode::Boxed)))
		{
			mode = static_cast<int>(MenuNavigationMode::Boxed);
		}

		const MenuNavigationMode selectedMode = static_cast<MenuNavigationMode>(mode);
		if (selectedMode != m_menuNavigationMode)
		{
			SetMenuNavigationMode(selectedMode);
		}
		if (m_menuNavigationMode == MenuNavigationMode::Boxed)
		{
			GameGUIAsset& asset = CurrentRoleGUI();
			ImGui::PushItemWidth(120.0f);
			const bool paddingChanged = ImGui::SliderInt("Box padding", &m_boxPadding, 0, 40);
			const bool offsetXChanged = ImGui::SliderInt("Box offset X", &m_boxOffsetX, -40, 40);
			const bool offsetYChanged = ImGui::SliderInt("Box offset Y", &m_boxOffsetY, -40, 40);
			ImGui::PopItemWidth();
			if (paddingChanged || offsetXChanged || offsetYChanged)
			{
				asset.boxPadding = m_boxPadding;
				asset.boxOffsetX = m_boxOffsetX;
				asset.boxOffsetY = m_boxOffsetY;
				SyncRuntimePreview();
			}
			static constexpr const char* boxSkins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
			const char* skinNames[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
			ImGui::PushItemWidth(170.0f);
			const bool boxSkinChanged = ImGui::Combo("Box skin", &m_boxSkinIndex, skinNames, IM_ARRAYSIZE(skinNames));
			ImGui::PopItemWidth();
			if (boxSkinChanged)
			{
				asset.boxSkin = boxSkins[m_boxSkinIndex];
				SyncRuntimePreview();
			}
		}
		else if (m_menuNavigationMode == MenuNavigationMode::Pointer)
		{
			GameGUIAsset& asset = CurrentRoleGUI();
			const bool sizeChanged = ImGui::SliderInt("Pointer size", &m_pointerWidth, 8, 100);
			const bool offsetChanged = ImGui::SliderInt("Pointer offset", &m_pointerGap, 0, 80);
			if (sizeChanged || offsetChanged)
			{
				m_pointerHeight = m_pointerWidth;
				asset.pointerWidth = m_pointerWidth;
				asset.pointerHeight = m_pointerWidth;
				asset.pointerGap = m_pointerGap;
				SyncRuntimePreview();
			}
			const char* pointerSkins[] = { "NavigationArrowRight1", "NavigationArrowRight2", "NavigationArrowRight3", "NavigationArrowRight4" };
			if (ImGui::Combo("Pointer skin", &m_pointerSkinIndex, pointerSkins, IM_ARRAYSIZE(pointerSkins)))
			{
				asset.pointerSkin = pointerSkins[m_pointerSkinIndex];
				SyncRuntimePreview();
			}
		}
		else
		{
			GameGUIAsset& asset = CurrentRoleGUI();
			const bool redChanged = ImGui::SliderFloat("Highlight red", &m_highlightR, 0.0f, 1.0f);
			const bool greenChanged = ImGui::SliderFloat("Highlight green", &m_highlightG, 0.0f, 1.0f);
			const bool blueChanged = ImGui::SliderFloat("Highlight blue", &m_highlightB, 0.0f, 1.0f);
			if (redChanged || greenChanged || blueChanged)
			{
				asset.highlightR = m_highlightR;
				asset.highlightG = m_highlightG;
				asset.highlightB = m_highlightB;
				SyncRuntimePreview();
			}
		}

	}
	ImGui::End();

	DrawCreateWidgetPopup();
	DrawBindingPopup();
	DrawTexturePickerPopup();

	DrawWidgetList();
	DrawWidgetDetails();
}

