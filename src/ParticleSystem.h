#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

#include <random>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.h"

// CPU-simulated particles drawn as camera-facing billboards via INSTANCED
// rendering: one four-vertex quad in the buffer, drawn N times with per-
// instance centre, size and colour. That is one draw call for the whole
// plume instead of one per mote.
//
// Additively blended, so no depth sorting is needed - addition is
// commutative, and overlapping motes simply accumulate into brighter cores.
class ParticleSystem
{
public:
    bool init(int maxParticles = 700);
    void shutdown();

    // strength 0..1 scales both the spawn rate and the plume's brightness.
    void setEmitter(const glm::vec3& position, float strength);

    void update(float deltaTime, float time);

    // A one-shot puff at an arbitrary point, independent of the emitter.
    // Used when the treasure is taken.
    void burst(const glm::vec3& origin, int count, const glm::vec3& tint,
               float speed = 3.2f);

    // Dust trickling DOWN out of a crack: spread over a box of the given half
    // size, drifting slowly downward rather than puffing outward.
    void sprinkle(const glm::vec3& origin, const glm::vec3& halfExtent,
                  int count, const glm::vec3& tint);

    // Needs the camera basis to orient the billboards. Extract it from the
    // view matrix rather than the camera object: the view matrix is the
    // authority on what "right" and "up" mean on screen.
    void render(const glm::mat4& view, const glm::mat4& projection);

    int aliveCount() const { return m_aliveCount; }

private:
    struct Particle
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        glm::vec3 tint{ 1.0f };

        // Each particle curls around its OWN origin. Burst particles are
        // nowhere near the lamp, and swirling them about the emitter would
        // fling them across the room.
        glm::vec3 swirlOrigin{ 0.0f };
        float life    = 0.0f;    // counts down to zero
        float maxLife = 1.0f;
        float size    = 0.1f;
        float growth  = 0.1f;
        float swirl   = 1.0f;    // radians per second around the plume axis

        // Only the Djinn's own plume fades with the emitter. Bursts and dust
        // belong to whatever made them - they used to vanish whenever the
        // lamp's column was down, which is the whole of the escape.
        bool fromEmitter = false;
    };

    Shader m_shader;

    std::vector<Particle> m_particles;
    std::vector<float>    m_instanceData;   // x,y,z, size, r,g,b,a per instance

    GLuint m_vao         = 0;
    GLuint m_quadVbo     = 0;
    GLuint m_instanceVbo = 0;

    int m_maxParticles = 0;
    int m_aliveCount   = 0;

    glm::vec3 m_emitterPosition{ 0.0f };
    float m_emitterStrength = 0.0f;
    float m_spawnCarry      = 0.0f;

    std::mt19937 m_rng{ 20260903u };

    float randomRange(float low, float high);
    void respawn(Particle& particle);
};

#endif // PARTICLESYSTEM_H
