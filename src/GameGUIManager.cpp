#include "GameGUIManager.h"

#include "GameGUI.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <sstream>

namespace {
	std::filesystem::path AssetDirectory()
	{
#ifdef AQUANACT_SOURCE_ROOT
		// Keep authored GameGUI assets under the source tree so they survive
		// rebuilds and remain editable outside the build output directory.
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "gameGUI";
#else
		return std::filesystem::current_path() / "assets" / "gameGUI";
#endif
	}

	GameGUIAsset LoadAssetFile(const std::filesystem::path& assetPath)
	{
		GameGUIAsset asset;
		asset.name = assetPath.stem().string();
		asset.savedOnDisk = true;

		std::ifstream file(assetPath);
		if (!file.is_open())
		{
			asset.savedOnDisk = false;
			return asset;
		}

		std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const std::size_t namePos = contents.find("\"name\":");
		if (namePos != std::string::npos)
		{
			const std::size_t firstQuote = contents.find('"', namePos + 7);
			const std::size_t secondQuote = firstQuote == std::string::npos ? std::string::npos : contents.find('"', firstQuote + 1);
			if (firstQuote != std::string::npos && secondQuote != std::string::npos)
			{
				asset.name = contents.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			}
		}

		std::size_t widgetPos = contents.find("\"type\": \"");
		while (widgetPos != std::string::npos)
		{
			GameGUIWidgetDef widget;
			const auto readField = [&contents](const std::string& key, std::size_t start) -> std::string
			{
				const std::size_t keyPos = contents.find(key, start);
				if (keyPos == std::string::npos)
				{
					return {};
				}
				const std::size_t valueStart = contents.find_first_not_of(" \t", keyPos + key.size());
				if (valueStart == std::string::npos)
				{
					return {};
				}
				if (contents[valueStart] == '"')
				{
					const std::size_t valueEnd = contents.find('"', valueStart + 1);
					return valueEnd == std::string::npos ? std::string{} : contents.substr(valueStart + 1, valueEnd - valueStart - 1);
				}
				const std::size_t valueEnd = contents.find_first_of(",\n}", valueStart);
				return contents.substr(valueStart, valueEnd - valueStart);
			};

			widget.type = readField("\"type\":", widgetPos);
			widget.name = readField("\"name\":", widgetPos);
			widget.skin = readField("\"skin\":", widgetPos);
			widget.text = readField("\"text\":", widgetPos);
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = std::stoi(readField("\"x\":", widgetPos));
			widget.y = std::stoi(readField("\"y\":", widgetPos));
			widget.width = std::stoi(readField("\"width\":", widgetPos));
			widget.height = std::stoi(readField("\"height\":", widgetPos));
			widget.visible = readField("\"visible\":", widgetPos).find("true") != std::string::npos;
			widget.alpha = std::stof(readField("\"alpha\":", widgetPos));
			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
		}

		return asset;
	}
}

GameGUIManager::GameGUIManager()
{
}

GameGUIManager::~GameGUIManager()
{
}

void GameGUIManager::startUp(Window& window)
{
	if (!m_runtime)
	{
		m_runtime = std::make_unique<GameGUI>();
	}
	// The runtime wrapper owns the live MyGUI session; the manager layers asset
	// loading and scene placement on top of that runtime instance.
	m_runtime->startUp(window);

	m_assets.clear();
	const std::filesystem::path assetDirectory = AssetDirectory();
	std::error_code ec;
	if (std::filesystem::exists(assetDirectory, ec) && !ec)
	{
		for (const auto& entry : std::filesystem::directory_iterator(assetDirectory, ec))
		{
			if (ec)
			{
				break;
			}
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}
			m_assets.push_back(LoadAssetFile(entry.path()));
		}
	}
}

void GameGUIManager::shutDown()
{
	if (m_runtime)
	{
		m_runtime->shutDown();
	}
	m_assets.clear();
	m_activeAssetIndex = -1;
}

void GameGUIManager::BeginFrame()
{
	if (m_runtime)
	{
		m_runtime->BeginFrame();
	}
}

void GameGUIManager::Draw()
{
	if (m_runtime)
	{
		m_runtime->Draw();
	}
}

void GameGUIManager::EndFrame()
{
	if (m_runtime)
	{
		m_runtime->EndFrame();
	}
}

void GameGUIManager::LoadUIAsset(const GameGUIAsset& asset)
{
	auto existing = std::find_if(m_assets.begin(), m_assets.end(), [&asset](const GameGUIAsset& other)
	{
		return other.name == asset.name;
	});
	if (existing == m_assets.end())
	{
		m_assets.push_back(asset);
		m_activeAssetIndex = static_cast<int>(m_assets.size() - 1);
	}
	else
	{
		*existing = asset;
		m_activeAssetIndex = static_cast<int>(std::distance(m_assets.begin(), existing));
	}
	ApplyActiveAsset();
}

bool GameGUIManager::AddSceneAsset(const std::string& name)
{
	if (name.empty())
	{
		return false;
	}
	if (std::find(m_sceneAssets.begin(), m_sceneAssets.end(), name) != m_sceneAssets.end())
	{
		return false;
	}

	m_sceneAssets.push_back(name);
	ActivateAsset(name);
	return true;
}

void GameGUIManager::RemoveSceneAsset(std::size_t index)
{
	if (index >= m_sceneAssets.size())
	{
		return;
	}
	const std::string removedName = m_sceneAssets[index];
	m_sceneAssets.erase(m_sceneAssets.begin() + static_cast<std::ptrdiff_t>(index));
	if (m_sceneAssets.empty())
	{
		m_activeAssetIndex = -1;
		if (m_runtime)
		{
			m_runtime->ClearUI();
		}
		return;
	}
	if (m_activeAssetIndex >= 0 && m_activeAssetIndex < static_cast<int>(m_assets.size()) && m_assets[static_cast<std::size_t>(m_activeAssetIndex)].name == removedName)
	{
		m_activeAssetIndex = 0;
		ApplyActiveAsset();
	}
}

void GameGUIManager::SetSceneAssets(const std::vector<std::string>& names)
{
	// This is the project-level placement list, not the on-disk asset library.
	m_sceneAssets.clear();
	for (const auto& name : names)
	{
		if (name.empty())
		{
			continue;
		}
		if (std::find(m_sceneAssets.begin(), m_sceneAssets.end(), name) != m_sceneAssets.end())
		{
			continue;
		}
		m_sceneAssets.push_back(name);
	}
	if (m_sceneAssets.empty())
	{
		m_activeAssetIndex = -1;
		if (m_runtime)
		{
			m_runtime->ClearUI();
		}
		return;
	}
	ActivateAsset(m_sceneAssets.front());
}

bool GameGUIManager::ActivateAsset(const std::string& name)
{
	auto existing = std::find_if(m_assets.begin(), m_assets.end(), [&name](const GameGUIAsset& asset)
	{
		return asset.name == name;
	});
	if (existing == m_assets.end())
	{
		return false;
	}

	m_activeAssetIndex = static_cast<int>(std::distance(m_assets.begin(), existing));
	ApplyActiveAsset();
	return true;
}

std::vector<std::string> GameGUIManager::AssetNames() const
{
	std::vector<std::string> names;
	names.reserve(m_assets.size());
	for (const auto& asset : m_assets)
	{
		names.push_back(asset.name);
	}
	return names;
}

const std::vector<std::string>& GameGUIManager::SceneAssets() const
{
	return m_sceneAssets;
}

void GameGUIManager::DrawEditorWindow()
{
	ImGui::Begin("GameGUI");
	ImGui::TextUnformatted("Placed GameGUI assets");
	for (std::size_t i = 0; i < m_sceneAssets.size(); ++i)
	{
		const std::string& name = m_sceneAssets[i];
		ImGui::Selectable((name + "##GameGUIPlaced" + std::to_string(i)).c_str(), false);
		ImGui::SameLine();
		if (ImGui::SmallButton(("Remove##GameGUIPlaced" + std::to_string(i)).c_str()))
		{
			RemoveSceneAsset(i);
			break;
		}
	}
	ImGui::Separator();
	if (ImGui::BeginCombo("Add Asset", "<select asset>"))
	{
		for (const auto& assetName : AssetNames())
		{
			if (ImGui::Selectable(assetName.c_str(), false))
			{
				AddSceneAsset(assetName);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::End();
}

void GameGUIManager::DrawDiagnosticsWindow()
{
	ImGui::Begin("GameGUIDiagnostics");
	ImGui::Text("Runtime wrapper: %s", m_runtime ? "ready" : "missing");
	ImGui::Text("Loaded assets: %zu", m_assets.size());
	ImGui::Text("Placed assets: %zu", m_sceneAssets.size());
	ImGui::Text("Active asset index: %d", m_activeAssetIndex);
	ImGui::Text("Active asset name: %s", ActiveAssetName().empty() ? "<none>" : ActiveAssetName().c_str());
	ImGui::Separator();
	ImGui::TextUnformatted("Loaded asset names:");
	for (const auto& asset : m_assets)
	{
		ImGui::BulletText("%s", asset.name.c_str());
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Placed scene assets:");
	for (const auto& name : m_sceneAssets)
	{
		ImGui::BulletText("%s", name.c_str());
	}
	ImGui::End();
}

void GameGUIManager::UnloadUIAsset(const std::string& name)
{
	auto existing = std::find_if(m_assets.begin(), m_assets.end(), [&name](const GameGUIAsset& asset)
	{
		return asset.name == name;
	});
	if (existing == m_assets.end())
	{
		return;
	}
	const int removedIndex = static_cast<int>(std::distance(m_assets.begin(), existing));
	m_assets.erase(existing);
	if (m_assets.empty())
	{
		m_activeAssetIndex = -1;
		if (m_runtime)
		{
			m_runtime->ClearUI();
		}
		return;
	}
	if (m_activeAssetIndex == removedIndex)
	{
		m_activeAssetIndex = 0;
		ApplyActiveAsset();
	}
	else if (m_activeAssetIndex > removedIndex)
	{
		--m_activeAssetIndex;
	}
}

void GameGUIManager::ClearUI()
{
	m_assets.clear();
	m_sceneAssets.clear();
	m_activeAssetIndex = -1;
	if (m_runtime)
	{
		m_runtime->ClearUI();
	}
}

std::size_t GameGUIManager::LoadedAssetCount() const
{
	return m_assets.size();
}

std::size_t GameGUIManager::PlacedAssetCount() const
{
	return m_sceneAssets.size();
}

std::string GameGUIManager::ActiveAssetName() const
{
	if (m_activeAssetIndex < 0 || m_activeAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return {};
	}
	return m_assets[static_cast<std::size_t>(m_activeAssetIndex)].name;
}

bool GameGUIManager::HasRuntime() const
{
	return m_runtime != nullptr;
}

void GameGUIManager::ApplyActiveAsset()
{
	if (!m_runtime)
	{
		return;
	}
	if (m_activeAssetIndex < 0 || m_activeAssetIndex >= static_cast<int>(m_assets.size()))
	{
		m_runtime->ClearUI();
		return;
	}
	m_runtime->LoadUIAsset(m_assets[static_cast<std::size_t>(m_activeAssetIndex)]);
}
