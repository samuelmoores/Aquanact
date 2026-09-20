#pragma once
#include <glm/ext.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <assimp/matrix4x4.h>


class ShaderProgram {
	struct EditorUniform {
		enum class Type { Float, Int, Bool, Vec2, Vec3, Vec4 } type = Type::Float;
		glm::vec4 value{0.0f};
	};
	uint32_t m_programId;
	std::unordered_map<std::string, EditorUniform> m_editorUniforms;

public:
	ShaderProgram();
	~ShaderProgram();
	void load(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
	void load(const std::string& vertexShaderPath, const std::string& geometryShaderPath, const std::string& fragmentShaderPath);

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
	void setUniform(const std::string& uniformName, const std::vector<glm::mat4>& values) const;
	void SetEditorUniformFloat(const std::string& name, float value);
	void SetEditorUniformInt(const std::string& name, int32_t value);
	void SetEditorUniformBool(const std::string& name, bool value);
	void SetEditorUniformVec2(const std::string& name, const glm::vec2& value);
	void SetEditorUniformVec3(const std::string& name, const glm::vec3& value);
	void SetEditorUniformVec4(const std::string& name, const glm::vec4& value);
	void ClearEditorUniforms();
	void ApplyEditorUniforms() const;
};
