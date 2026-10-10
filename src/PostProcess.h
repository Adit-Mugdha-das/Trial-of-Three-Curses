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
//
// Three more things happen on the way, all for image quality:
//  - Anti-aliasing. The scene is drawn into a 4x multisampled target and
//    resolved into the texture, so edges are smooth instead of stair-stepped.
//  - Ambient occlusion (SSAO). From the depth buffer alone, each pixel checks
//    how much nearby geometry surrounds it - corners, creases, the ground
//    under an object - and is darkened by that much. That soft contact
//    shading is what makes objects look grounded rather than pasted on.
//  - Bloom. The brightest parts of the picture - flames, glowing eyes, gems,
//    the sun - are blurred at half size and added back, so light seems to
//    spill into the air around them.
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

    // The camera's projection this frame: SSAO needs it to turn depth back
    // into positions, and to project its samples back onto the screen.
    void setProjection(const glm::mat4& projection) { m_projection = projection; }

    void setOcclusion(bool on) { m_occlusionOn = on; }
    bool occlusion() const { return m_occlusionOn; }
    void setBloom(bool on) { m_bloomOn = on; }
    bool bloom() const { return m_bloomOn; }
    int  samples() const { return m_samples; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool valid() const { return m_fbo != 0; }

private:
    Shader m_shader;
    Shader m_ssaoShader;
    Shader m_blurShader;
    bool   m_ssaoReady = false;
    Shader m_brightShader;
    Shader m_bloomBlurShader;
    bool   m_bloomReady = false;

    // The multisampled target the scene is drawn into.
    GLuint m_msFbo   = 0;
    GLuint m_msColor = 0;
    GLuint m_msDepth = 0;
    int    m_samples = 0;

    // What it resolves into: the colour the final pass reads, and the depth
    // SSAO reads.
    GLuint m_fbo          = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthTexture = 0;

    // SSAO, then its blur.
    GLuint m_aoFbo       = 0;
    GLuint m_aoTexture   = 0;
    GLuint m_blurFbo     = 0;
    GLuint m_blurTexture = 0;

    // Bloom, at half size: the bright parts, then blurred back and forth.
    GLuint m_bloomFbo[2] = { 0, 0 };
    GLuint m_bloomTexture[2] = { 0, 0 };
    int    m_bloomWidth = 0;
    int    m_bloomHeight = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    int m_width  = 0;
    int m_height = 0;

    float m_desaturate = 0.0f;
    float m_vignette   = 0.0f;
    glm::vec3 m_tint{ 1.0f };
    float m_tintAmount = 0.0f;

    glm::mat4 m_projection{ 1.0f };
    bool      m_occlusionOn = true;
    bool      m_bloomOn = true;
    glm::vec3 m_kernel[16];

    void releaseTargets();
    void drawTriangle() const;
};

#endif // POSTPROCESS_H
