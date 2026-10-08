#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.28318530718f;

    // Per-instance stride: vec3 centre + float size + vec4 colour.
    constexpr int kFloatsPerInstance = 8;
}

float ParticleSystem::randomRange(float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    return dist(m_rng);
}

bool ParticleSystem::init(int maxParticles)
{
    if (!m_shader.load("shaders/particle.vert", "shaders/particle.frag"))
    {
        return false;
    }

    m_maxParticles = maxParticles;
    m_particles.assign(static_cast<std::size_t>(maxParticles), Particle{});
    // Room for the glows on the end of the same buffer.
    m_instanceData.resize(static_cast<std::size_t>(maxParticles + kMaxGlows)
                          * kFloatsPerInstance);

    // Unit quad centred on the origin, expanded to face the camera in the
    // vertex shader.
    const float corners[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quadVbo);
    glGenBuffers(1, &m_instanceVbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instanceData.size() * sizeof(float)),
                 nullptr, GL_STREAM_DRAW);

    const GLsizei stride = kFloatsPerInstance * sizeof(float);

    // location 1: centre
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);

    // location 2: size
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // location 3: colour
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // The divisor is what makes this instanced: these three advance once per
    // INSTANCE rather than once per vertex. Without it every particle would
    // read the first instance's data.
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    return true;
}

void ParticleSystem::shutdown()
{
    if (m_instanceVbo != 0) { glDeleteBuffers(1, &m_instanceVbo); m_instanceVbo = 0; }
    if (m_quadVbo != 0)     { glDeleteBuffers(1, &m_quadVbo); m_quadVbo = 0; }
    if (m_vao != 0)         { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

void ParticleSystem::setEmitter(const glm::vec3& position, float strength)
{
    m_emitterPosition = position;
    m_emitterStrength = std::min(1.0f, std::max(0.0f, strength));
}

void ParticleSystem::respawn(Particle& particle)
{
    // Start on a small disc around the lamp mouth rather than a single point,
    // so the plume has a base rather than a pinch.
    const float angle  = randomRange(0.0f, kTwoPi);
    const float radius = randomRange(0.0f, 0.34f);

    particle.position = m_emitterPosition + glm::vec3(
        std::cos(angle) * radius,
        randomRange(-0.05f, 0.10f),
        std::sin(angle) * radius);

    particle.velocity = {
        randomRange(-0.16f, 0.16f),
        randomRange(0.75f, 1.65f),
        randomRange(-0.16f, 0.16f)
    };

    particle.swirlOrigin = m_emitterPosition;
    particle.fromEmitter = true;

    particle.maxLife = randomRange(1.5f, 3.0f);
    particle.life    = particle.maxLife;
    particle.size    = randomRange(0.12f, 0.30f);
    particle.growth  = randomRange(0.22f, 0.55f);

    // Alternating spin directions read as turbulence rather than a vortex.
    particle.swirl = randomRange(0.7f, 2.1f) * (randomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);

    // Djinn palette: cyan core drifting toward deep blue.
    const float warmth = randomRange(0.0f, 1.0f);
    particle.tint = glm::mix(glm::vec3(0.35f, 0.95f, 1.00f),
                             glm::vec3(0.12f, 0.45f, 0.90f),
                             warmth);
}

void ParticleSystem::burst(const glm::vec3& origin, int count,
                           const glm::vec3& tint, float speed)
{
    for (Particle& particle : m_particles)
    {
        if (count <= 0)
        {
            break;
        }

        // Only claim slots that are already dead - a burst must never cut
        // short the plume that is currently running.
        if (particle.life > 0.0f)
        {
            continue;
        }

        // A direction on the unit sphere, biased upward so the puff lifts.
        const float theta = randomRange(0.0f, kTwoPi);
        const float y     = randomRange(0.10f, 1.00f);
        const float r     = std::sqrt(std::max(0.0f, 1.0f - y * y));

        particle.position    = origin;
        particle.swirlOrigin = origin;
        particle.velocity    = glm::vec3(std::cos(theta) * r, y, std::sin(theta) * r)
                             * randomRange(speed * 0.45f, speed);

        particle.maxLife = randomRange(0.55f, 1.25f);
        particle.life    = particle.maxLife;
        particle.size    = randomRange(0.10f, 0.26f);
        particle.growth  = randomRange(0.30f, 0.80f);
        particle.swirl   = randomRange(-0.6f, 0.6f);
        particle.tint    = tint * randomRange(0.75f, 1.15f);
        particle.fromEmitter = false;

        --count;
    }
}

void ParticleSystem::sprinkle(const glm::vec3& origin, const glm::vec3& halfExtent,
                              int count, const glm::vec3& tint)
{
    for (Particle& particle : m_particles)
    {
        if (count <= 0)
        {
            break;
        }
        if (particle.life > 0.0f)
        {
            continue;
        }

        particle.position = origin + glm::vec3(
            randomRange(-halfExtent.x, halfExtent.x),
            randomRange(-halfExtent.y, halfExtent.y),
            randomRange(-halfExtent.z, halfExtent.z));
        particle.swirlOrigin = particle.position;
        particle.velocity = { randomRange(-0.25f, 0.25f),
                              -randomRange(0.9f, 2.4f),
                              randomRange(-0.25f, 0.25f) };

        particle.maxLife = randomRange(1.0f, 1.8f);
        particle.life    = particle.maxLife;
        particle.size    = randomRange(0.08f, 0.17f);
        particle.growth  = randomRange(0.10f, 0.30f);
        particle.swirl   = 0.0f;
        particle.tint    = tint * randomRange(0.8f, 1.15f);
        particle.fromEmitter = false;

        --count;
    }
}

void ParticleSystem::addGlow(const glm::vec3& position, float size, const glm::vec4& color)
{
    if (m_glowCount >= kMaxGlows || m_maxParticles == 0)
    {
        return;
    }

    // Straight after the live particles, which update() packs at the front.
    const std::size_t index = static_cast<std::size_t>(m_aliveCount + m_glowCount);
    float* out = &m_instanceData[index * kFloatsPerInstance];
    out[0] = position.x;
    out[1] = position.y;
    out[2] = position.z;
    out[3] = size;
    out[4] = color.r;
    out[5] = color.g;
    out[6] = color.b;
    out[7] = color.a;

    ++m_glowCount;
}

void ParticleSystem::addGlowOnTop(const glm::vec3& position, float size, const glm::vec4& color)
{
    if (static_cast<int>(m_topGlows.size()) >= kMaxGlows * 8) { return; }
    m_topGlows.insert(m_topGlows.end(), { position.x, position.y, position.z, size,
                                          color.r, color.g, color.b, color.a });
}

void ParticleSystem::update(float deltaTime, float /*time*/)
{
    m_glowCount = 0;
    m_topGlows.clear();

    if (m_maxParticles == 0)
    {
        return;
    }

    // Fractional spawns are carried across frames, so the rate stays honest
    // at any frame time instead of rounding down to zero every frame.
    const float spawnRate = 220.0f * m_emitterStrength;
    m_spawnCarry += spawnRate * deltaTime;

    int budget = static_cast<int>(m_spawnCarry);
    m_spawnCarry -= static_cast<float>(budget);

    m_aliveCount = 0;
    std::size_t writeIndex = 0;

    for (Particle& particle : m_particles)
    {
        if (particle.life <= 0.0f)
        {
            // Recycle a dead slot if there is spawn budget left. The pool is
            // fixed, so nothing is ever allocated during the simulation.
            if (budget > 0 && m_emitterStrength > 0.0f)
            {
                respawn(particle);
                --budget;
            }
            else
            {
                continue;
            }
        }

        particle.life -= deltaTime;
        if (particle.life <= 0.0f)
        {
            continue;
        }

        // Swirl about the plume's vertical axis: rotate the horizontal offset
        // from the emitter, which curls the smoke as it climbs.
        const glm::vec3 offset = particle.position - particle.swirlOrigin;
        const float angle = particle.swirl * deltaTime;
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        const glm::vec3 curled(
            offset.x * c - offset.z * s,
            offset.y,
            offset.x * s + offset.z * c);

        particle.position = particle.swirlOrigin + curled;
        particle.position += particle.velocity * deltaTime;

        // Rising smoke spreads and slows.
        particle.velocity.y -= 0.22f * deltaTime;
        particle.size += particle.growth * deltaTime;

        const float t = particle.life / particle.maxLife;

        // Fade in quickly, out slowly: sharp at the source, soft at the top.
        const float alpha = std::min(1.0f, (1.0f - t) * 6.0f) * t * t;

        float* out = &m_instanceData[writeIndex * kFloatsPerInstance];
        out[0] = particle.position.x;
        out[1] = particle.position.y;
        out[2] = particle.position.z;
        out[3] = particle.size;
        out[4] = particle.tint.r;
        out[5] = particle.tint.g;
        out[6] = particle.tint.b;
        out[7] = particle.fromEmitter ? alpha * m_emitterStrength : alpha;

        ++writeIndex;
        ++m_aliveCount;
    }
}

void ParticleSystem::render(const glm::mat4& view, const glm::mat4& projection)
{
    const int total = m_aliveCount + m_glowCount;
    const int onTop = static_cast<int>(m_topGlows.size() / 8);
    if ((total <= 0 && onTop <= 0) || m_vao == 0)
    {
        return;
    }

    // The view matrix's first two columns are the camera's right and up axes
    // in world space - exactly what a billboard needs.
    const glm::vec3 cameraRight(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 cameraUp   (view[0][1], view[1][1], view[2][1]);

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);

    // Orphan the buffer before writing: the driver hands back fresh storage
    // instead of stalling until the previous frame's draw has finished with
    // the old contents.
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instanceData.size() * sizeof(float)),
                 nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(total * kFloatsPerInstance * sizeof(float)),
                    m_instanceData.data());

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", projection);
    m_shader.setVec3("uCameraRight", cameraRight);
    m_shader.setVec3("uCameraUp", cameraUp);

    // Additive: glowing energy adds to what is behind it rather than
    // replacing it, and the result is order-independent.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Test against the scene so motes hide behind the lamp, but do not write
    // depth - particles must never occlude one another.
    glDepthMask(GL_FALSE);

    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(m_vao);
    if (total > 0)
    {
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, total);
    }

    // The on-top glows: their own upload, drawn with the depth test off.
    if (onTop > 0)
    {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_topGlows.size() * sizeof(float)),
                     m_topGlows.data(), GL_STREAM_DRAW);
        glDisable(GL_DEPTH_TEST);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, onTop);
        glEnable(GL_DEPTH_TEST);
    }
    glBindVertexArray(0);

    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}
