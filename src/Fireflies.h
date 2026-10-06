#ifndef FIREFLIES_H
#define FIREFLIES_H

#include <random>
#include <vector>

#include <glm/glm.hpp>

// A swarm of fireflies drifting about the chamber.
//
// Each one wanders around a home point that itself drifts slowly, flashes on
// its own rhythm and is dark the rest of the time, and shies away from the
// traveller as he walks through them. When the chamber starts to come down
// they panic: faster, jumpier, flashing out of time.
//
// No GL here: main hands the positions and brightness to the particle
// renderer as extra glowing points, one draw call for the lot.
class Fireflies
{
public:
    void init(int count, const glm::vec3& boxMin, const glm::vec3& boxMax,
              unsigned seed = 7u);

    // `panic` 0..1 - the chamber trembling.
    void update(float deltaTime, const glm::vec3& traveller, float panic);

    int       count() const { return static_cast<int>(m_flies.size()); }
    glm::vec3 position(int i) const { return m_flies[i].position; }
    float     glow(int i) const { return m_flies[i].glow; }      // 0..1

    // The one firefly casting a real light right now, or -1. It keeps the
    // light through its whole flash and hands it on to one that is only just
    // starting to glow - jumping to whichever was brightest made the light
    // pop from place to place.
    int lit() const { return m_lit; }

    // How bright that light should be, 0..1. Measured from the faint glow a
    // dark firefly still has, so a firefly between flashes lights nothing and
    // the light fades in and out instead of switching on.
    float lightLevel() const;

private:
    struct Fly
    {
        glm::vec3 home{ 0.0f };       // drifts slowly around the room
        glm::vec3 homeDrift{ 0.0f };
        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };

        glm::vec3 wanderFreq{ 1.0f }; // two sines per axis around home
        glm::vec3 wanderPhase{ 0.0f };
        float     wanderSize = 1.0f;

        float blinkPeriod = 3.0f;
        float blinkPhase  = 0.0f;
        float glow        = 0.0f;
        float lastGlow    = 0.0f;
    };

    std::vector<Fly> m_flies;
    glm::vec3 m_min{ 0.0f };
    glm::vec3 m_max{ 0.0f };
    float     m_clock = 0.0f;     // runs faster when they panic
    int       m_lit = -1;
    int       m_cursor = 0;       // where the search for the next one starts

    std::mt19937 m_rng{ 7u };
    float randomRange(float low, float high);
};

#endif // FIREFLIES_H
