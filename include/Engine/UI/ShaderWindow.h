#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <glm/vec4.hpp>

class SceneManager;

class ShaderWindow
{
public:
	void Draw(SceneManager& sceneManager, unsigned int selectedEntityId, bool& open);

private:
	struct UniformDefinition
	{
		std::string type;
		std::string name;
	};

	void ReloadShaders();
	void ReloadUniforms();

	std::vector<std::string> m_shaderFiles;
	std::vector<UniformDefinition> m_uniforms;
	std::unordered_map<std::string, glm::vec4> m_values;
	std::string m_selectedShader;
	std::string m_error;
	bool m_loaded = false;
};
