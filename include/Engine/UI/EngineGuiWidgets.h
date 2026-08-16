#pragma once

#include <imgui.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace EngineGuiWidgets
{
	class WindowScope
	{
	public:
		WindowScope(const char* title, bool* open, ImGuiWindowFlags flags = 0)
			: m_visible(ImGui::Begin(title, open, flags))
		{
		}

		~WindowScope()
		{
			ImGui::End();
		}

		explicit operator bool() const { return m_visible; }

		WindowScope(const WindowScope&) = delete;
		WindowScope& operator=(const WindowScope&) = delete;

	private:
		bool m_visible;
	};

	class ModalScope
	{
	public:
		ModalScope(const char* title, ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize)
			: m_visible(ImGui::BeginPopupModal(title, nullptr, flags))
		{
		}

		~ModalScope()
		{
			if (m_visible)
			{
				ImGui::EndPopup();
			}
		}

		explicit operator bool() const { return m_visible; }

		ModalScope(const ModalScope&) = delete;
		ModalScope& operator=(const ModalScope&) = delete;

	private:
		bool m_visible;
	};

	inline void ConsumePopupRequest(const char* popupId, bool& requested)
	{
		if (requested)
		{
			ImGui::OpenPopup(popupId);
			requested = false;
		}
	}

	inline bool CloseButton(const char* label = "Close")
	{
		if (!ImGui::Button(label))
		{
			return false;
		}
		ImGui::CloseCurrentPopup();
		return true;
	}

	inline bool LabeledFloat(
		const char* label,
		float& value,
		float speed,
		float minimum,
		float maximum,
		const char* format = "%.2f",
		float width = 220.0f)
	{
		ImGui::SetNextItemWidth(width);
		return ImGui::DragFloat(label, &value, speed, minimum, maximum, format);
	}

	inline bool Vector3Editor(
		const char* label,
		glm::vec3& value,
		float speed = 0.1f,
		float minimum = -FLT_MAX,
		float maximum = FLT_MAX,
		const char* format = "%.2f")
	{
		return ImGui::DragFloat3(label, &value.x, speed, minimum, maximum, format);
	}

	enum class DialogAction
	{
		None,
		Confirm,
		Cancel
	};

	inline void SetComboWidthToContents(const char* text)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SetNextItemWidth(
			ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f + ImGui::GetFrameHeight());
	}

	// Draws a checkbox-style menu item and reports whether its value changed.
	inline bool ToggleMenuItem(const char* label, bool& value)
	{
		return ImGui::Checkbox(label, &value);
	}

	// Draws a combo backed by a string list and preserves ImGui's default-focus
	// behavior for the currently selected item.
	inline bool StringCombo(
		const char* label,
		int& selectedIndex,
		const std::vector<std::string>& options,
		const char* emptyPreview = "<select>")
	{
		const bool validSelection = selectedIndex >= 0
			&& selectedIndex < static_cast<int>(options.size());
		const char* preview = validSelection
			? options[static_cast<std::size_t>(selectedIndex)].c_str()
			: emptyPreview;
		bool changed = false;

		if (ImGui::BeginCombo(label, preview))
		{
			for (int index = 0; index < static_cast<int>(options.size()); ++index)
			{
				const bool selected = selectedIndex == index;
				if (ImGui::Selectable(options[static_cast<std::size_t>(index)].c_str(), selected))
				{
					selectedIndex = index;
					changed = true;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		return changed;
	}

	inline void SectionHeading(const char* label)
	{
		ImGui::TextDisabled(label);
		ImGui::Separator();
	}

	inline void StatusMessage(std::string_view message)
	{
		if (message.empty())
		{
			return;
		}
		ImGui::Separator();
		ImGui::TextUnformatted(message.data(), message.data() + message.size());
	}

	struct AssetFilePickerOptions
	{
		std::filesystem::path directory;
		std::string relativePrefix;
		std::vector<std::string> extensions;
		bool directoriesOnly = false;
		const char* emptyPreview = "<select asset>";
	};

	inline std::filesystem::path SourceAssetDirectory(const std::filesystem::path& relativePath)
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / relativePath;
#else
		return std::filesystem::current_path() / relativePath;
#endif
	}

	// Draws a combo for files or directories in an asset folder. The selected
	// value is stored as a project-relative path, while the combo displays only
	// the asset filename/directory name.
	inline bool AssetFileCombo(
		const char* label,
		std::string& selectedPath,
		const AssetFilePickerOptions& options)
	{
		const std::string preview = selectedPath.empty()
			? options.emptyPreview
			: std::filesystem::path(selectedPath).filename().string();
		bool changed = false;

		if (!ImGui::BeginCombo(label, preview.c_str()))
		{
			return false;
		}

		struct AssetOption
		{
			std::string displayName;
			std::string relativePath;
		};
		std::vector<AssetOption> assets;
		std::error_code error;
		if (std::filesystem::exists(options.directory, error) && !error)
		{
			for (const auto& entry : std::filesystem::directory_iterator(options.directory, error))
			{
				if (error)
				{
					break;
				}

				const bool isDirectory = entry.is_directory(error);
				if (error || (options.directoriesOnly ? !isDirectory : !entry.is_regular_file(error)))
				{
					error.clear();
					continue;
				}
				if (!options.directoriesOnly && !options.extensions.empty())
				{
					const std::string extension = entry.path().extension().string();
					if (std::find(options.extensions.begin(), options.extensions.end(), extension) == options.extensions.end())
					{
						continue;
					}
				}

				const std::string name = entry.path().filename().string();
				assets.push_back({ name, options.relativePrefix + entry.path().filename().generic_string() });
			}
		}
		std::sort(assets.begin(), assets.end(), [](const AssetOption& left, const AssetOption& right)
		{
			return left.displayName < right.displayName;
		});

		for (const AssetOption& asset : assets)
		{
			const bool selected = selectedPath == asset.relativePath;
			if (ImGui::Selectable(asset.displayName.c_str(), selected))
			{
				selectedPath = asset.relativePath;
				changed = true;
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
		return changed;
	}

	inline DialogAction ConfirmationButtons(
		const char* confirmLabel,
		const char* cancelLabel = "Cancel",
		bool confirmEnabled = true)
	{
		DialogAction action = DialogAction::None;
		if (!confirmEnabled)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(confirmLabel))
		{
			action = DialogAction::Confirm;
		}
		if (!confirmEnabled)
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (ImGui::Button(cancelLabel))
		{
			action = DialogAction::Cancel;
		}
		return action;
	}
}
