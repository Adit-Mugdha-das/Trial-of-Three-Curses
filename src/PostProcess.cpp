#include "PostProcess.h"

#include <algorithm>
#include <iostream>

bool PostProcess::init(int width, int height)
{
    if (!m_shader.load("shaders/post.vert", "shaders/post.frag"))
    {
        return false;
    }

    // A single oversized triangle rather than two triangles forming a quad.
    // It covers the screen with no diagonal seam, and shades each pixel once
    // instead of twice along the quad's shared edge.
    const float triangle[] = {
        // position     // uv
        -1.0f, -1.0f,   0.0f, 0.0f,
         3.0f, -1.0f,   2.0f, 0.0f,
        -1.0f,  3.0f,   0.0f, 2.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    glGenFramebuffers(1, &m_fbo);

    if (!resize(width, height))
    {
        shutdown();
        return false;
    }

    std::cout << "[PostProcess] " << m_width << "x" << m_height
              << " colour target ready\n";
    return true;
}

void PostProcess::releaseTargets()
{
    if (m_colorTexture != 0) { glDeleteTextures(1, &m_colorTexture); m_colorTexture = 0; }
    if (m_depthBuffer != 0)  { glDeleteRenderbuffers(1, &m_depthBuffer); m_depthBuffer = 0; }
}

bool PostProcess::resize(int width, int height)
{
    // A minimised window reports zero, and a zero-sized attachment makes the
    // framebuffer incomplete.
    m_width  = std::max(1, width);
    m_height = std::max(1, height);

    releaseTargets();

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamp, so the vignette's edge taps cannot wrap to the far side.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_colorTexture, 0);

    // Depth as a renderbuffer, not a texture: nothing samples it, and
    // renderbuffers are the cheaper option when you only need to render to it.
    glGenRenderbuffers(1, &m_depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_depthBuffer);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "[PostProcess] Framebuffer incomplete (0x"
                  << std::hex << status << std::dec << ")\n";
        return false;
    }

    return true;
}

void PostProcess::shutdown()
{
    releaseTargets();

    if (m_fbo != 0) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao != 0) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

void PostProcess::setEffects(float desaturate, float vignette,
                             const glm::vec3& tint, float tintAmount)
{
    m_desaturate = desaturate;
    m_vignette   = vignette;
    m_tint       = tint;
    m_tintAmount = tintAmount;
}

void PostProcess::beginScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void PostProcess::endSceneAndResolve()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);

    // The resolve covers every pixel, so there is nothing to clear, no depth
    // to test against, and nothing to cull.
    glDisable(GL_DEPTH_TEST);

    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);
    glDisable(GL_CULL_FACE);

    m_shader.use();
    m_shader.setInt("uScene", 0);
    m_shader.setFloat("uDesaturate", m_desaturate);
    m_shader.setFloat("uVignette", m_vignette);
    m_shader.setVec3("uTint", m_tint);
    m_shader.setFloat("uTintAmount", m_tintAmount);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }

    glEnable(GL_DEPTH_TEST);
}
