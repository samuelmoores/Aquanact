#include "Engine/UI/ShaderWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ShaderProgram.h"

#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <sstream>

namespace
{
	std::filesystem::path ShaderDirectory()
	{
		return std::filesystem::current_path() / "shaders";
	}

}

void ShaderWindow::ReloadShaders()
{
	m_shaderFiles.clear();
	m_error.clear();
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(ShaderDirectory(), error))
	{
		const auto extension = entry.path().extension();
		if (entry.is_regular_file(error) &&
			(extension == ".frag" || extension == ".vert" || extension == ".geom"))
			m_shaderFiles.push_back(entry.path().filename().string());
	}
	if (error)
		m_error = "Could not scan shaders directory.";
	std::sort(m_shaderFiles.begin(), m_shaderFiles.end());
	if (m_selectedShader.empty() && !m_shaderFiles.empty())
		m_selectedShader = m_shaderFiles.front();
	if (!m_selectedShader.empty())
		ReloadUniforms();
	m_loaded = true;
}

void ShaderWindow::ReloadUniforms()
{
	m_uniforms.clear();
	if (m_selectedShader.empty()) return;
	std::ifstream file(ShaderDirectory() / m_selectedShader);
	if (!file)
	{
		m_error = "Could not open selected shader.";
		return;
	}
	static const std::regex uniformPattern(R"(^\s*uniform\s+(float|int|bool|vec2|vec3|vec4)\s+([A-Za-z_]\w*)\s*;)");
	std::ostringstream source;
	source << file.rdbuf();
	static const std::regex blockComment(R"(/\*[\s\S]*?\*/)");
	static const std::regex lineComment(R"(//.*$)");
	const std::string uncommented = std::regex_replace(
		std::regex_replace(source.str(), blockComment, ""), lineComment, "");
	std::istringstream lines(uncommented);
	std::string line;
	while (std::getline(lines, line))
	{
		std::smatch match;
		if (std::regex_search(line, match, uniformPattern))
		{
			const std::string name = match[2].str();
			if (name != "iResolution" && name != "iTime")
				m_uniforms.push_back({ match[1].str(), name });
		}
	}
}

void ShaderWindow::Draw(SceneManager&, unsigned int selectedEntityId, bool& open)
{
	if (!open) return;
	if (!m_loaded) ReloadShaders();
	if (!ImGui::Begin("Shader Window", &open))
	{
		ImGui::End();
		return;
	}
	if (ImGui::Button("Rescan shaders")) ReloadShaders();
	ImGui::SameLine();
	ImGui::TextUnformatted("Selected entity uniforms");

	if (ImGui::BeginCombo("Shader", m_selectedShader.empty() ? "<none>" : m_selectedShader.c_str()))
	{
		for (const std::string& file : m_shaderFiles)
		{
			const bool selected = file == m_selectedShader;
			if (ImGui::Selectable(file.c_str(), selected))
			{
				m_selectedShader = file;
				ReloadUniforms();
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::Button("Reset to defaults"))
	{
		ShaderProgram* resetShader = nullptr;
		if (Scene* scene = Root::Current().Scenes().ActiveLevel())
		{
			for (const auto& object : scene->Objects())
				if (object && object->Id() == selectedEntityId) { resetShader = object->GetShader(); break; }
		}
		if (resetShader) resetShader->ClearEditorUniforms();
		const std::string prefix = m_selectedShader + ":";
		for (auto it = m_values.begin(); it != m_values.end();)
		{
			if (it->first.rfind(prefix, 0) == 0) it = m_values.erase(it);
			else ++it;
		}
	}

	if (!m_error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", m_error.c_str());
	if (m_uniforms.empty()) ImGui::TextUnformatted("No supported uniforms found.");

	ShaderProgram* shader = nullptr;
	if (Scene* scene = Root::Current().Scenes().ActiveLevel())
	{
		for (const auto& object : scene->Objects())
			if (object && object->Id() == selectedEntityId) { shader = object->GetShader(); break; }
	}

	for (const UniformDefinition& uniform : m_uniforms)
	{
		const std::string valueKey = m_selectedShader + ":" + uniform.name;
		auto valueIt = m_values.find(valueKey);
		if (valueIt == m_values.end())
			valueIt = m_values.emplace(valueKey, glm::vec4(0.0f)).first;
		glm::vec4& storedValue = valueIt->second;
		if (!shader) { ImGui::Text("%s %s (select a matching target)", uniform.type.c_str(), uniform.name.c_str()); continue; }
		if (uniform.type == "float")
		{
			float value = storedValue.x;
			if (ImGui::DragFloat(uniform.name.c_str(), &value, 0.001f))
			{
				storedValue.x = value;
				shader->SetEditorUniformFloat(uniform.name, value);
			}
		}
		else if (uniform.type == "int")
		{
			int value = static_cast<int>(storedValue.x);
			if (ImGui::DragInt(uniform.name.c_str(), &value))
			{
				storedValue.x = static_cast<float>(value);
				shader->SetEditorUniformInt(uniform.name, value);
			}
		}
		else if (uniform.type == "bool")
		{
			bool value = storedValue.x != 0.0f;
			if (ImGui::Checkbox(uniform.name.c_str(), &value))
			{
				storedValue.x = value ? 1.0f : 0.0f;
				shader->SetEditorUniformBool(uniform.name, value);
			}
		}
		else
		{
			const int count = uniform.type == "vec2" ? 2 : (uniform.type == "vec3" ? 3 : 4);
			bool changed = false;
			if (count == 2) changed = ImGui::DragFloat2(uniform.name.c_str(), &storedValue.x, 0.001f);
			else if (count == 3) changed = ImGui::DragFloat3(uniform.name.c_str(), &storedValue.x, 0.001f);
			else changed = ImGui::DragFloat4(uniform.name.c_str(), &storedValue.x, 0.001f);
			if (changed)
			{
				if (count == 2) shader->SetEditorUniformVec2(uniform.name, glm::vec2(storedValue));
				else if (count == 3) shader->SetEditorUniformVec3(uniform.name, glm::vec3(storedValue));
				else shader->SetEditorUniformVec4(uniform.name, storedValue);
			}
		}
	}
	ImGui::End();
}
