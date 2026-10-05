#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#include <glad/glad.h>
#include <glm/glm.hpp>

// A depth-only framebuffer rendered from a light's point of view.
//
// The scene is drawn once into this from the light, recording how far the
// nearest surface is in every direction. The main pass then re-projects each
// fragment into the same space: if something was closer to the light than the
// fragment is, the fragment is in shadow.
//
// One caster only - the left torch. Point lights strictly need a cube map for
// omnidirectional shadows; a single perspective frustum aimed at the chamber
// is far cheaper and, since the torches are wall-mounted and face inward,
// visually equivalent here.
class ShadowMap
{
public:
    bool init(int resolution = 1024);
    void shutdown();

    // Binds the FBO and sets the viewport. Remember to restore the viewport.
    void beginCapture();
    void endCapture(int windowWidth, int windowHeight);

    void bindTexture(unsigned int unit) const;

    // Builds and stores the light's view-projection for this frame.
    void setLight(const glm::vec3& lightPosition, const glm::vec3& target);
    const glm::mat4& lightSpaceMatrix() const { return m_lightSpace; }

    int resolution() const { return m_resolution; }
    bool valid() const { return m_fbo != 0; }

private:
    GLuint m_fbo = 0;
    GLuint m_depthTexture = 0;
    int m_resolution = 1024;

    glm::mat4 m_lightSpace{ 1.0f };
};

#endif // SHADOWMAP_H
