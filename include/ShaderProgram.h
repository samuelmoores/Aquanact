#pragma once
#include <glm/ext.hpp>
#include <string>
class ShaderProgram {
	uint32_t m_programId;

public:
	ShaderProgram();
	void load(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

	void activate() const;

	uint32_t GetID();

	void setUniform(const std::string& uniformName, bool value) const;
	void setUniform(const std::string& uniformName, int32_t value) const;
	void setUniform(const std::string& uniformName, float value) const;
	void setUniform(const std::string& uniformName, const glm::vec2& value) const;
	void setUniform(const std::string& uniformName, const glm::vec3& value) const;
	void setUniform(const std::string& uniformName, const glm::vec4& value) const;
	void setUniform(const std::string& uniformName, const glm::mat2& value) const;
	void setUniform(const std::string& uniformName, const glm::mat3& value) const;
	void setUniform(const std::string& uniformName, const glm::mat4& value) const;
};