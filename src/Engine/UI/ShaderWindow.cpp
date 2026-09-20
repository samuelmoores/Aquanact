#include "Engine/UI/ShaderWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ShaderProgram.h"
#include "Engine/Core/RenderManager.h"

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

	glm::vec4 DefaultValue(const std::string& shader, const std::string& uniform)
	{
		if (shader != "balatro.frag") return glm::vec4(0.0f);
		if (uniform == "SPIN_ROTATION") return glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "SPIN_SPEED") return glm::vec4(7.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "COLOUR_1") return glm::vec4(0.871f, 0.267f, 0.231f, 1.0f);
		if (uniform == "COLOUR_2") return glm::vec4(0.0f, 0.42f, 0.706f, 1.0f);
		if (uniform == "COLOUR_3") return glm::vec4(0.086f, 0.137f, 0.145f, 1.0f);
		if (uniform == "CONTRAST") return glm::vec4(3.5f, 0.0f, 0.0f, 0.0f);
		if (uniform == "LIGTHING") return glm::vec4(0.4f, 0.0f, 0.0f, 0.0f);
		if (uniform == "SPIN_AMOUNT") return glm::vec4(0.25f, 0.0f, 0.0f, 0.0f);
		if (uniform == "PIXEL_FILTER") return glm::vec4(745.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "SPIN_EASE") return glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "SHAPE_SCALE") return glm::vec4(30.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "RADIAL_TWIST") return glm::vec4(20.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "WARP_STRENGTH") return glm::vec4(0.5f, 0.0f, 0.0f, 0.0f);
		if (uniform == "WARP_FREQUENCY") return glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "WARP_ITERATIONS") return glm::vec4(5.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "BAND_WIDTH") return glm::vec4(5.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "LIGHTING_THRESHOLD") return glm::vec4(4.0f, 0.0f, 0.0f, 0.0f);
		if (uniform == "BACKGROUND_BLEND") return glm::vec4(0.3f, 0.0f, 0.0f, 0.0f);
		if (uniform == "LOOP_TIME") return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		return glm::vec4(0.0f);
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
		if (m_selectedShader == "balatro.frag")
			resetShader = Root::Current().Render().MainMenuShader();
		else if (Scene* scene = Root::Current().Scenes().ActiveLevel())
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
	if (m_selectedShader == "balatro.frag")
		shader = Root::Current().Render().MainMenuShader();
	else if (Scene* scene = Root::Current().Scenes().ActiveLevel())
	{
		for (const auto& object : scene->Objects())
			if (object && object->Id() == selectedEntityId) { shader = object->GetShader(); break; }
	}

	for (const UniformDefinition& uniform : m_uniforms)
	{
		const std::string valueKey = m_selectedShader + ":" + uniform.name;
		auto valueIt = m_values.find(valueKey);
		if (valueIt == m_values.end())
			valueIt = m_values.emplace(valueKey, DefaultValue(m_selectedShader, uniform.name)).first;
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
