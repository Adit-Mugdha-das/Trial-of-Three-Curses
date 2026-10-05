#include "ShadowMap.h"

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

bool ShadowMap::init(int resolution)
{
    m_resolution = resolution;

    glGenFramebuffers(1, &m_fbo);

    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 m_resolution, m_resolution, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Anything outside the light's frustum samples the border value of 1.0,
    // the maximum depth, and is therefore treated as lit. With GL_REPEAT the
    // map would tile and stamp phantom shadows across the whole chamber.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_depthTexture, 0);

    // Depth only. Without saying so explicitly the FBO is incomplete, because
    // it has no colour attachment to draw to or read from.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "[ShadowMap] Framebuffer incomplete (0x"
                  << std::hex << status << std::dec << ")\n";
        shutdown();
        return false;
    }

    std::cout << "[ShadowMap] " << m_resolution << "x" << m_resolution
              << " depth buffer ready\n";
    return true;
}

void ShadowMap::shutdown()
{
    if (m_depthTexture != 0) { glDeleteTextures(1, &m_depthTexture); m_depthTexture = 0; }
    if (m_fbo != 0) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
}

void ShadowMap::setLight(const glm::vec3& lightPosition, const glm::vec3& target)
{
    // Wide enough to take in the whole chamber from a wall-mounted torch.
    // Near plane kept as far out as possible: depth precision is worst near
    // the eye, and shadow acne follows from exactly that.
    const glm::mat4 projection =
        glm::perspective(glm::radians(108.0f), 1.0f, 1.5f, 45.0f);

    const glm::mat4 view =
        glm::lookAt(lightPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));

    m_lightSpace = projection * view;
}

void ShadowMap::beginCapture()
{
    glViewport(0, 0, m_resolution, m_resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::endCapture(int windowWidth, int windowHeight)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowWidth, windowHeight);
}

void ShadowMap::bindTexture(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
}
