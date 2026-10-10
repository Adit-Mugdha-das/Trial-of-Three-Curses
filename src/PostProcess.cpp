#include "PostProcess.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>

namespace
{
    // A colour target for one of the extra passes: single-channel and sharp
    // for SSAO, full colour and smoothly filtered for bloom.
    void makeAoTarget(GLuint fbo, GLuint& texture, int width, int height, bool colour = false)
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        if (colour) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); }
        else        { glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr); }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, colour ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, colour ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    }
}

bool PostProcess::init(int width, int height)
{
    if (!m_shader.load("shaders/post.vert", "shaders/post.frag"))
    {
        return false;
    }

    // SSAO is an extra: if its shaders fail, the picture is simply drawn
    // without it.
    m_ssaoReady = m_ssaoShader.load("shaders/post.vert", "shaders/ssao.frag")
               && m_blurShader.load("shaders/post.vert", "shaders/ssao_blur.frag");
    m_bloomReady = m_brightShader.load("shaders/post.vert", "shaders/bloom_bright.frag")
                && m_bloomBlurShader.load("shaders/post.vert", "shaders/bloom_blur.frag");

    // The sample directions: points inside a hemisphere, more of them close
    // to the centre, where occlusion matters most. Fixed seed, so every run
    // shades the same.
    std::mt19937 rng(1234u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int i = 0; i < 16; ++i)
    {
        glm::vec3 s(unit(rng) * 2.0f - 1.0f, unit(rng) * 2.0f - 1.0f, unit(rng));
        s = glm::normalize(s) * unit(rng);
        float scale = static_cast<float>(i) / 16.0f;
        scale = 0.1f + 0.9f * scale * scale;
        m_kernel[i] = s * scale;
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
    glGenFramebuffers(1, &m_msFbo);
    glGenFramebuffers(1, &m_aoFbo);
    glGenFramebuffers(1, &m_blurFbo);
    glGenFramebuffers(2, m_bloomFbo);

    if (!resize(width, height))
    {
        shutdown();
        return false;
    }

    std::cout << "[PostProcess] " << m_width << "x" << m_height << " target ready, "
              << (m_samples > 0 ? std::to_string(m_samples) + "x anti-aliasing" : std::string("no anti-aliasing"))
              << ", ambient occlusion " << (m_ssaoReady ? "ready" : "unavailable")
              << ", bloom " << (m_bloomReady ? "ready" : "unavailable") << "\n";
    return true;
}

void PostProcess::releaseTargets()
{
    if (m_colorTexture != 0) { glDeleteTextures(1, &m_colorTexture); m_colorTexture = 0; }
    if (m_depthTexture != 0) { glDeleteTextures(1, &m_depthTexture); m_depthTexture = 0; }
    if (m_msColor != 0)      { glDeleteRenderbuffers(1, &m_msColor); m_msColor = 0; }
    if (m_msDepth != 0)      { glDeleteRenderbuffers(1, &m_msDepth); m_msDepth = 0; }
    if (m_aoTexture != 0)    { glDeleteTextures(1, &m_aoTexture); m_aoTexture = 0; }
    if (m_blurTexture != 0)  { glDeleteTextures(1, &m_blurTexture); m_blurTexture = 0; }
    for (int i = 0; i < 2; ++i)
    {
        if (m_bloomTexture[i] != 0) { glDeleteTextures(1, &m_bloomTexture[i]); m_bloomTexture[i] = 0; }
    }
}

bool PostProcess::resize(int width, int height)
{
    // A minimised window reports zero, and a zero-sized attachment makes the
    // framebuffer incomplete.
    m_width  = std::max(1, width);
    m_height = std::max(1, height);

    releaseTargets();

    // --- the resolved image: colour, and depth as a texture SSAO can read --------
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamp, so the vignette's edge taps cannot wrap to the far side.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_colorTexture, 0);

    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_depthTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::cout << "[PostProcess] Framebuffer incomplete (0x"
                  << std::hex << status << std::dec << ")\n";
        return false;
    }

    // --- the multisampled target the scene is drawn into -------------------------
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    m_samples = std::min(4, static_cast<int>(maxSamples));
    if (m_samples >= 2)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_msFbo);
        glGenRenderbuffers(1, &m_msColor);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_RGB8, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msColor);

        glGenRenderbuffers(1, &m_msDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_DEPTH_COMPONENT24, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msDepth);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            // No multisampling here: draw straight into the resolved target.
            std::cout << "[PostProcess] multisampled target unavailable, drawing without anti-aliasing\n";
            glDeleteRenderbuffers(1, &m_msColor); m_msColor = 0;
            glDeleteRenderbuffers(1, &m_msDepth); m_msDepth = 0;
            m_samples = 0;
        }
    }
    else
    {
        m_samples = 0;
    }

    // --- SSAO and its blur ---------------------------------------------------------
    if (m_ssaoReady)
    {
        makeAoTarget(m_aoFbo, m_aoTexture, m_width, m_height);
        makeAoTarget(m_blurFbo, m_blurTexture, m_width, m_height);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "[PostProcess] ambient occlusion target incomplete, drawing without it\n";
            m_ssaoReady = false;
        }
    }

    if (m_bloomReady)
    {
        m_bloomWidth  = std::max(1, m_width / 2);
        m_bloomHeight = std::max(1, m_height / 2);
        for (int i = 0; i < 2; ++i)
        {
            makeAoTarget(m_bloomFbo[i], m_bloomTexture[i], m_bloomWidth, m_bloomHeight, true);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cout << "[PostProcess] bloom target incomplete, drawing without it\n";
                m_bloomReady = false;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void PostProcess::shutdown()
{
    releaseTargets();

    if (m_fbo != 0)     { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_msFbo != 0)   { glDeleteFramebuffers(1, &m_msFbo); m_msFbo = 0; }
    if (m_aoFbo != 0)   { glDeleteFramebuffers(1, &m_aoFbo); m_aoFbo = 0; }
    if (m_blurFbo != 0) { glDeleteFramebuffers(1, &m_blurFbo); m_blurFbo = 0; }
    if (m_bloomFbo[0] != 0) { glDeleteFramebuffers(2, m_bloomFbo); m_bloomFbo[0] = m_bloomFbo[1] = 0; }
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
    glBindFramebuffer(GL_FRAMEBUFFER, m_samples > 0 ? m_msFbo : m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void PostProcess::drawTriangle() const
{
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostProcess::endSceneAndResolve()
{
    // --- 1. resolve the multisampled picture, colour and depth ---------------------
    if (m_samples > 0)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height,
                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    // The passes below cover every pixel, so there is nothing to clear, no
    // depth to test against, and nothing to cull.
    glDisable(GL_DEPTH_TEST);

    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);
    glDisable(GL_CULL_FACE);

    glViewport(0, 0, m_width, m_height);

    // --- 2. ambient occlusion, then a blur to smooth its grain ---------------------
    const bool occlude = m_ssaoReady && m_occlusionOn;
    if (occlude)
    {
        // Near and far, read back out of the projection, for linear depth.
        const float a = m_projection[2][2];
        const float b = m_projection[3][2];
        const float nearPlane = b / (a - 1.0f);
        const float farPlane  = b / (a + 1.0f);

        glBindFramebuffer(GL_FRAMEBUFFER, m_aoFbo);
        m_ssaoShader.use();
        m_ssaoShader.setInt("uDepth", 0);
        m_ssaoShader.setMat4("uProjection", m_projection);
        m_ssaoShader.setMat4("uInvProjection", glm::inverse(m_projection));
        m_ssaoShader.setFloat("uRadius", 0.85f);
        for (int i = 0; i < 16; ++i)
        {
            m_ssaoShader.setVec3("uKernel[" + std::to_string(i) + "]", m_kernel[i]);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        drawTriangle();

        glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo);
        m_blurShader.use();
        m_blurShader.setInt("uAO", 0);
        m_blurShader.setInt("uDepth", 1);
        m_blurShader.setFloat("uNear", nearPlane);
        m_blurShader.setFloat("uFar", farPlane);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_aoTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        drawTriangle();
    }

    // --- 3. bloom: the bright parts at half size, blurred across and down twice ----
    const bool glow = m_bloomReady && m_bloomOn;
    if (glow)
    {
        glViewport(0, 0, m_bloomWidth, m_bloomHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
        m_brightShader.use();
        m_brightShader.setInt("uScene", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        drawTriangle();

        m_bloomBlurShader.use();
        m_bloomBlurShader.setInt("uImage", 0);
        for (int pass = 0; pass < 4; ++pass)
        {
            const bool across = (pass % 2) == 0;
            const float spread = (pass < 2) ? 1.0f : 2.2f;     // a tight glow, then a wide one
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[across ? 1 : 0]);
            m_bloomBlurShader.setVec2("uDirection", across ? glm::vec2(spread, 0.0f) : glm::vec2(0.0f, spread));
            glBindTexture(GL_TEXTURE_2D, m_bloomTexture[across ? 0 : 1]);
            drawTriangle();
        }

        glViewport(0, 0, m_width, m_height);
    }

    // --- 4. the final picture, to the window ---------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_shader.use();
    m_shader.setInt("uScene", 0);
    m_shader.setInt("uAO", 1);
    m_shader.setFloat("uAOStrength", occlude ? 1.0f : 0.0f);
    m_shader.setInt("uBloom", 2);
    m_shader.setFloat("uBloomStrength", glow ? 0.6f : 0.0f);
    m_shader.setFloat("uDesaturate", m_desaturate);
    m_shader.setFloat("uVignette", m_vignette);
    m_shader.setVec3("uTint", m_tint);
    m_shader.setFloat("uTintAmount", m_tintAmount);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glow ? m_bloomTexture[0] : m_colorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, occlude ? m_blurTexture : m_colorTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);

    drawTriangle();

    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }

    glEnable(GL_DEPTH_TEST);
}
