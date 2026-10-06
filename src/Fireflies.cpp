#include "Fireflies.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.2831853f;

    // He scatters them as he walks through: anything closer than this is
    // pushed away.
    constexpr float kShyRadius = 2.2f;

    // How strongly each one is pulled back towards where it wants to be, and
    // how quickly its motion dies away. Together a soft spring, so a push from
    // the traveller swings it aside and it drifts back rather than snapping.
    constexpr float kSpring = 2.2f;
    constexpr float kDamping = 2.0f;

    // The fraction of each cycle spent lit. Real fireflies flash briefly and
    // are dark most of the time.
    constexpr float kFlashFraction = 0.40f;

    // Faintly visible even when dark, so the swarm never vanishes entirely.
    constexpr float kEmber = 0.05f;
}

float Fireflies::lightLevel() const
{
    if (m_lit < 0) { return 0.0f; }
    return std::max(0.0f, (m_flies[m_lit].glow - kEmber) / (1.0f - kEmber));
}

float Fireflies::randomRange(float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    return dist(m_rng);
}

void Fireflies::init(int count, const glm::vec3& boxMin, const glm::vec3& boxMax,
                     unsigned seed)
{
    m_rng.seed(seed);
    m_min = boxMin;
    m_max = boxMax;
    m_clock = 0.0f;
    m_flies.clear();

    for (int i = 0; i < count; ++i)
    {
        Fly fly;
        fly.home = { randomRange(boxMin.x, boxMax.x),
                     randomRange(boxMin.y, boxMax.y),
                     randomRange(boxMin.z, boxMax.z) };

        // A slow, steady drift of the home point, so the swarm moves about
        // the room over time instead of hovering in fixed spots.
        const float heading = randomRange(0.0f, kTwoPi);
        const float speed = randomRange(0.15f, 0.45f);
        fly.homeDrift = { std::cos(heading) * speed, randomRange(-0.08f, 0.08f),
                          std::sin(heading) * speed };

        fly.wanderFreq  = { randomRange(0.25f, 0.7f), randomRange(0.3f, 0.9f),
                            randomRange(0.25f, 0.7f) };
        fly.wanderPhase = { randomRange(0.0f, kTwoPi), randomRange(0.0f, kTwoPi),
                            randomRange(0.0f, kTwoPi) };
        fly.wanderSize  = randomRange(0.6f, 1.4f);

        fly.blinkPeriod = randomRange(2.2f, 5.0f);
        fly.blinkPhase  = randomRange(0.0f, 1.0f);

        fly.position = fly.home;
        m_flies.push_back(fly);
    }
}

void Fireflies::update(float deltaTime, const glm::vec3& traveller, float panic)
{
    panic = std::clamp(panic, 0.0f, 1.0f);

    // Panic speeds up their own sense of time: the wander, the drift and the
    // flashing all quicken together.
    const float pace = 1.0f + 2.5f * panic;
    m_clock += deltaTime * pace;
    const float t = m_clock;

    for (std::size_t i = 0; i < m_flies.size(); ++i)
    {
        Fly& fly = m_flies[i];

        // --- the home point drifts, and bounces off the room's bounds -------
        fly.home += fly.homeDrift * (deltaTime * pace);
        for (int axis = 0; axis < 3; ++axis)
        {
            if (fly.home[axis] < m_min[axis])
            {
                fly.home[axis] = m_min[axis];
                fly.homeDrift[axis] = std::fabs(fly.homeDrift[axis]);
            }
            else if (fly.home[axis] > m_max[axis])
            {
                fly.home[axis] = m_max[axis];
                fly.homeDrift[axis] = -std::fabs(fly.homeDrift[axis]);
            }
        }

        // --- where it wants to be: a lazy loop around home --------------------
        // Two sines per axis at unrelated rates never quite repeat, which is
        // what keeps the path from looking like an orbit.
        const glm::vec3& f = fly.wanderFreq;
        const glm::vec3& p = fly.wanderPhase;
        glm::vec3 wander(
            std::sin(t * f.x + p.x) + 0.5f * std::sin(t * f.x * 2.3f + p.y),
            0.6f * std::sin(t * f.y + p.y) + 0.3f * std::sin(t * f.y * 1.7f + p.z),
            std::sin(t * f.z + p.z) + 0.5f * std::sin(t * f.z * 2.1f + p.x));
        wander *= fly.wanderSize * (1.0f + 0.6f * panic);

        const glm::vec3 wanted = fly.home + wander;

        // --- soft spring towards it ---------------------------------------------
        glm::vec3 accel = (wanted - fly.position) * kSpring * (1.0f + 1.5f * panic)
                        - fly.velocity * kDamping;

        // --- shy of the traveller ---------------------------------------------------
        // Measured from his chest, so they part around his body rather than
        // his feet.
        const glm::vec3 chest = traveller + glm::vec3(0.0f, 1.5f, 0.0f);
        const glm::vec3 away = fly.position - chest;
        const float distance = glm::length(away);
        if (distance < kShyRadius && distance > 1e-3f)
        {
            const float push = (kShyRadius - distance) / kShyRadius;
            accel += (away / distance) * (push * push * 18.0f);
        }

        // --- panic: sudden darts ---------------------------------------------------
        if (panic > 0.01f)
        {
            const float s = static_cast<float>(i) * 1.37f;
            accel += glm::vec3(std::sin(t * 9.1f + s), 0.5f * std::sin(t * 7.3f + s * 2.0f),
                               std::sin(t * 8.3f + s * 3.0f)) * (6.0f * panic);
        }

        fly.velocity += accel * deltaTime;
        fly.position += fly.velocity * deltaTime;

        // Never into the floor, never above the walls.
        fly.position.y = std::clamp(fly.position.y, 0.3f, m_max.y + 1.0f);

        // --- the flash -----------------------------------------------------------
        // A brief swell and fade once per cycle, dark in between. In a panic
        // they flicker more than they flash.
        const float cycle = t / fly.blinkPeriod + fly.blinkPhase;
        const float u = cycle - std::floor(cycle);
        float flash = 0.0f;
        if (u < kFlashFraction)
        {
            const float s = std::sin(3.14159265f * u / kFlashFraction);
            flash = s * s;
        }
        if (panic > 0.01f)
        {
            const float flicker = 0.5f + 0.5f * std::sin(t * 23.0f + static_cast<float>(i));
            flash = std::max(flash, panic * flicker * 0.8f);
        }

        fly.lastGlow = fly.glow;
        fly.glow = kEmber + (1.0f - kEmber) * flash;
    }

    // --- who carries the light ------------------------------------------------
    if (m_lit >= 0 && m_flies[m_lit].glow < 0.08f)
    {
        m_lit = -1;     // its flash is over
    }
    if (m_lit < 0 && !m_flies.empty())
    {
        // The next one just starting to brighten, searching from a moving
        // start point so the light visits the whole swarm.
        const int n = static_cast<int>(m_flies.size());
        for (int k = 0; k < n; ++k)
        {
            const int i = (m_cursor + k) % n;
            const Fly& fly = m_flies[i];
            if (fly.glow > fly.lastGlow && fly.glow < 0.10f)
            {
                m_lit = i;
                m_cursor = (i + 1) % n;
                break;
            }
        }
    }
}
