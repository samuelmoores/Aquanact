#include "ShaderProgram.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

ShaderProgram::ShaderProgram()
    : m_programId(-1) {

}

void ShaderProgram::load(const std::string& vertexShaderPath, const std::string& fragmentShaderPath)
{
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        // open files
        vShaderFile.open(vertexShaderPath);
        fShaderFile.open(fragmentShaderPath);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        throw std::runtime_error("Failed to locate vertex or fragment shader files");
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    // print compile errors if any
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        throw std::runtime_error(infoLog);
    };

    // similiar for Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    // print compile errors if any
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        throw std::runtime_error(infoLog);
    };

    // shader Program
    m_programId = glCreateProgram();
    glAttachShader(m_programId, vertex);
    glAttachShader(m_programId, fragment);
    glLinkProgram(m_programId);
    // print linking errors if any
    glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_programId, 512, NULL, infoLog);
        throw std::runtime_error(infoLog);
    }

    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void ShaderProgram::activate() const
{
    glUseProgram(m_programId);
}

uint32_t ShaderProgram::GetID()
{
    return m_programId;
}

void ShaderProgram::setUniform(const std::string& uniformName, bool value) const
{
    glUniform1i(glGetUniformLocation(m_programId, uniformName.c_str()), (int32_t)value);
}

void ShaderProgram::setUniform(const std::string& uniformName, int32_t value) const
{
    glUniform1i(glGetUniformLocation(m_programId, uniformName.c_str()), value);
}

void ShaderProgram::setUniform(const std::string& uniformName, float value) const
{
    glUniform1f(glGetUniformLocation(m_programId, uniformName.c_str()), (int)value);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::vec2& value) const
{
    glUniform2fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, &value[0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, &value[0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, &value[0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::mat2& value) const
{
    glUniformMatrix2fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, false, &value[0][0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::mat3& value) const
{
    glUniformMatrix3fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, false, &value[0][0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_programId, uniformName.c_str()), 1, false, &value[0][0]);
}

void ShaderProgram::setUniform(const std::string& uniformName, const std::vector<glm::mat4>& values) const
{
    GLint boneUniformLocation = glGetUniformLocation(m_programId, uniformName.c_str());
    glUniformMatrix4fv(boneUniformLocation, values.size(), GL_FALSE, glm::value_ptr(values[0]));
}


