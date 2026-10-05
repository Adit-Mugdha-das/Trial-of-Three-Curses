#ifndef MESH_H
#define MESH_H

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    // Points along increasing U across the surface. Together with the normal
    // it defines the tangent frame a normal map's values are expressed in.
    glm::vec3 tangent{ 1.0f, 0.0f, 0.0f };
};

// Plain geometry, no GL objects. Primitives produce this; Mesh uploads it.
// Keeping them separate means geometry can be generated and inspected without
// a live GL context.
struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

// Owns a VAO/VBO/EBO triple. Move-only: copying would double-free the buffers.
class Mesh
{
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const MeshData& data);
    void draw() const;

    GLsizei indexCount() const { return m_indexCount; }
    bool valid() const { return m_vao != 0; }

private:
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ebo = 0;
    GLsizei m_indexCount = 0;

    void release();
};

#endif // MESH_H
