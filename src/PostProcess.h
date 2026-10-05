#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.h"

// Off-screen colour target plus a full-screen resolve pass.
//
// The scene is rendered into a texture instead of straight to the window,
// then that texture is drawn over the whole screen through a shader that can
// operate on the finished image: desaturating it, darkening the corners,
// tinting it. Effects that need to see the final pixel are only possible
// once the image exists as a texture.
class PostProcess
{
public:
    bool init(int width, int height);
    void shutdown();

    // Recreates the attachments at a new size. Cheap enough to call on every
    // resize event, and necessary - a stale target would be stretched.
    bool resize(int width, int height);

    void beginScene();                    // bind the off-screen target
    void endSceneAndResolve();            // back to the window, then draw

    void setEffects(float desaturate, float vignette,
                    const glm::vec3& tint, float tintAmount);

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool valid() const { return m_fbo != 0; }

private:
    Shader m_shader;

    GLuint m_fbo          = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthBuffer  = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    int m_width  = 0;
    int m_height = 0;

    float m_desaturate = 0.0f;
    float m_vignette   = 0.0f;
    glm::vec3 m_tint{ 1.0f };
    float m_tintAmount = 0.0f;

    void releaseTargets();
};

#endif // POSTPROCESS_H
