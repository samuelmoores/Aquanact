#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Camera.h"

#include <imgui.h>
#include <algorithm>
#include <functional>

void GameGUICreator::DrawWidgetList()
{
	ImGui::Begin("GameGUIs", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const bool selected = GUIIndex(m_selectedGUI) == i;
		if (ImGui::Selectable(GUIName(static_cast<GUIRole>(i)), selected))
		{
			m_selectedGUI = static_cast<GUIRole>(i);
			m_selectedWidgetIndex = m_assets[i].widgets.empty() ? -1 : 0;
			SyncRuntimePreview();
		}
	}
	ImGui::End();

	ImGui::Begin("Widget List", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (!CurrentRoleGUI().widgets.empty())
	{
		GameGUIAsset& asset = CurrentRoleGUI();
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

				const bool selected = m_selectedWidgetIndex == static_cast<int>(i);
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

				const std::string label = widget.name + " (" + widget.type + ")##WidgetTree" + std::to_string(i);
				const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					m_selectedWidgetIndex = static_cast<int>(i);
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

