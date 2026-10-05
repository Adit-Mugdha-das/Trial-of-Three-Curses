#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <unordered_map>

#include <glad/glad.h>
#include <glm/glm.hpp>

// Compiles and links a vertex + fragment shader pair, and caches uniform
// locations. Keeps the source paths around so shaders can be reloaded at
// runtime without restarting the program.
class Shader
{
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Compiles and links. Returns false and leaves any previously working
    // program intact if compilation fails, so a typo never blanks the screen.
    bool load(const std::string& vertexPath, const std::string& fragmentPath);

    // Recompiles from the paths given to the last load(). Bound to F5.
    bool reload();

    void use() const;
    GLuint id() const { return m_id; }
    bool valid() const { return m_id != 0; }

    void setBool (const std::string& name, bool value) const;
    void setInt  (const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2 (const std::string& name, const glm::vec2& v) const;
    void setVec3 (const std::string& name, const glm::vec3& v) const;
    void setVec4 (const std::string& name, const glm::vec4& v) const;
    void setMat3 (const std::string& name, const glm::mat3& m) const;
    void setMat4 (const std::string& name, const glm::mat4& m) const;

private:
    GLuint m_id = 0;
    std::string m_vertexPath;
    std::string m_fragmentPath;

    // Cached so we are not doing a driver string lookup per uniform per frame.
    mutable std::unordered_map<std::string, GLint> m_uniformCache;

    static bool readFile(const std::string& path, std::string& out);
    static GLuint compileStage(GLenum type, const std::string& source,
                               const std::string& label);

    GLint uniformLocation(const std::string& name) const;
};

#endif // SHADER_H
