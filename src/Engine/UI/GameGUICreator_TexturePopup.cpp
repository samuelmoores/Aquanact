#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"

#include <imgui.h>
#include <algorithm>
#include <vector>

namespace {
	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}

	bool IsSupportedTextureFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga";
	}

	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath)
	{
		std::error_code ec;
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, SourceRoot() / "assets", ec);
		if (!ec && !relativeToAssets.empty())
		{
			return relativeToAssets.generic_string();
		}
		return absolutePath.generic_string();
	}
}

void GameGUICreator::DrawTexturePickerPopup()
{
	if (m_showTexturePickerPopup)
	{
		ImGui::OpenPopup("Select Texture");
		m_showTexturePickerPopup = false;
	}

	if (!ImGui::BeginPopupModal("Select Texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	if (m_texturePickerRootDirectory.empty())
	{
		m_texturePickerRootDirectory = std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "textures";
		m_texturePickerCurrentDirectory = m_texturePickerRootDirectory;
	}

	ImGui::Text("Root: %s", m_texturePickerRootDirectory.generic_string().c_str());
	ImGui::Text("Current: %s", m_texturePickerCurrentDirectory.generic_string().c_str());
	const bool canGoUp = !m_texturePickerCurrentDirectory.empty() && m_texturePickerCurrentDirectory != m_texturePickerRootDirectory;
	if (ImGui::Button("Up") && canGoUp)
	{
		m_texturePickerCurrentDirectory = m_texturePickerCurrentDirectory.parent_path();
		m_texturePickerSelectedPath.clear();
	}

	ImGui::BeginChild("TextureFileExplorer", ImVec2(640.0f, 320.0f), true);
	std::error_code ec;
	if (!std::filesystem::exists(m_texturePickerCurrentDirectory, ec) || ec)
	{
		ImGui::TextDisabled("Texture directory does not exist.");
	}
	else
	{
		std::vector<std::filesystem::directory_entry> directories;
		std::vector<std::filesystem::directory_entry> files;
		for (const auto& entry : std::filesystem::directory_iterator(m_texturePickerCurrentDirectory, ec))
		{
			if (ec) { break; }
			if (entry.is_directory()) directories.push_back(entry);
			else if (entry.is_regular_file() && IsSupportedTextureFile(entry.path())) files.push_back(entry);
		}
		std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b){ return a.path().filename().string() < b.path().filename().string(); });
		std::sort(files.begin(), files.end(), [](const auto& a, const auto& b){ return a.path().filename().string() < b.path().filename().string(); });
		for (const auto& entry : directories)
		{
			const std::string label = "[Dir] " + entry.path().filename().string();
			if (ImGui::Selectable(label.c_str(), false))
			{
				m_texturePickerCurrentDirectory = entry.path();
				m_texturePickerSelectedPath.clear();
			}
		}
		for (const auto& entry : files)
		{
			const bool selected = m_texturePickerSelectedPath == entry.path();
			if (ImGui::Selectable(entry.path().filename().string().c_str(), selected))
			{
				m_texturePickerSelectedPath = entry.path();
			}
		}
		if (directories.empty() && files.empty())
		{
			ImGui::TextDisabled("No texture files found in this directory.");
		}
	}
	ImGui::EndChild();

	if (!m_texturePickerSelectedPath.empty())
	{
		ImGui::Text("Selected: %s", MakePortableTexturePath(m_texturePickerSelectedPath).c_str());
	}
	else
	{
		ImGui::TextDisabled("Selected: <None>");
	}

	const bool canSelect = !m_texturePickerSelectedPath.empty();
	if (ImGui::Button("Select") && canSelect)
	{
		const TexturePickerTarget target = m_texturePickerTarget;
		const std::string portablePath = MakePortableTexturePath(m_texturePickerSelectedPath);
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			if (m_pendingProgressBarCreation)
			{
				m_pendingProgressBarWidget.texture = portablePath;
				AddProgressBarWidget();
				m_pendingProgressBarCreation = false;
				m_pendingProgressBarBindingComplete = false;
				SyncRuntimePreview();
			}
			else
			{
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "%s", portablePath.c_str());
			}
		}
		else if (target == TexturePickerTarget::SelectedWidgetTexture)
		{
			GameGUIAsset& asset = CurrentRoleGUI();
			if (m_selectedWidgetIndex >= 0 && m_selectedWidgetIndex < static_cast<int>(asset.widgets.size()))
			{
				asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)].texture = portablePath;
				SyncRuntimePreview();
			}
		}
		m_texturePickerTarget = TexturePickerTarget::None;
		m_texturePickerSelectedPath.clear();
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			m_showCreateWidgetPopup = true;
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		const TexturePickerTarget target = m_texturePickerTarget;
		m_texturePickerTarget = TexturePickerTarget::None;
		m_texturePickerSelectedPath.clear();
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			m_showCreateWidgetPopup = true;
		}
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

