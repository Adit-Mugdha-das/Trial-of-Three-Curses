#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

Shader::~Shader()
{
    if (m_id != 0)
    {
        glDeleteProgram(m_id);
    }
}

bool Shader::readFile(const std::string& path, std::string& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "[Shader] Could not open file: " << path << '\n';
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

GLuint Shader::compileStage(GLenum type, const std::string& source,
                            const std::string& label)
{
    GLuint stage = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(stage, 1, &src, nullptr);
    glCompileShader(stage);

    GLint success = 0;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLint length = 0;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &length);

        std::vector<char> log(length > 1 ? length : 1);
        glGetShaderInfoLog(stage, static_cast<GLsizei>(log.size()), nullptr,
                           log.data());

        std::cout << "[Shader] Compile failed (" << label << "):\n"
                  << log.data() << '\n';

        glDeleteShader(stage);
        return 0;
    }

    return stage;
}

bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath)
{
    m_vertexPath = vertexPath;
    m_fragmentPath = fragmentPath;

    std::string vertexSource;
    std::string fragmentSource;
    if (!readFile(vertexPath, vertexSource) ||
        !readFile(fragmentPath, fragmentSource))
    {
        return false;
    }

    GLuint vertex = compileStage(GL_VERTEX_SHADER, vertexSource, vertexPath);
    if (vertex == 0)
    {
        return false;
    }

    GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (fragment == 0)
    {
        glDeleteShader(vertex);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

        std::vector<char> log(length > 1 ? length : 1);
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr,
                            log.data());

        std::cout << "[Shader] Link failed:\n" << log.data() << '\n';

        glDeleteProgram(program);
        return false;
    }

    // Only swap in the new program once we know it is good.
    if (m_id != 0)
    {
        glDeleteProgram(m_id);
    }
    m_id = program;
    m_uniformCache.clear();

    return true;
}

bool Shader::reload()
{
    if (m_vertexPath.empty() || m_fragmentPath.empty())
    {
        return false;
    }

    if (load(m_vertexPath, m_fragmentPath))
    {
        std::cout << "[Shader] Reloaded " << m_vertexPath << " + "
                  << m_fragmentPath << '\n';
        return true;
    }

    std::cout << "[Shader] Reload failed, keeping previous program.\n";
    return false;
}

void Shader::use() const
{
    glUseProgram(m_id);
}

GLint Shader::uniformLocation(const std::string& name) const
{
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end())
    {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_id, name.c_str());
    if (location == -1)
    {
        // Warn once per name; an unused uniform gets optimised out, which is
        // usually harmless but worth seeing when something looks wrong.
        std::cout << "[Shader] Uniform not found: " << name << '\n';
    }

    m_uniformCache[name] = location;
    return location;
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(uniformLocation(name), static_cast<int>(value));
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(uniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(uniformLocation(name), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& v) const
{
    glUniform2fv(uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) const
{
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setVec4(const std::string& name, const glm::vec4& v) const
{
    glUniform4fv(uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setMat3(const std::string& name, const glm::mat3& m) const
{
    glUniformMatrix3fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setMat4(const std::string& name, const glm::mat4& m) const
{
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}
