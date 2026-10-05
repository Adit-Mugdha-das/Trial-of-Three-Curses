#include "Pursuer.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"
#include "EscapeTuning.h"
#include "SceneNode.h"

namespace
{
    // She is far larger than the traveller, so she turns far more slowly.
    constexpr float kTurnRate = 260.0f;    // degrees per second

    // Her body is wide; this keeps her off the corridor walls.
    constexpr float kBodyRadius = 1.15f;
}

void Pursuer::reset(const glm::vec3& home, float homeHeading, bool blessedVerdict)
{
    m_position = home;
    m_heading  = homeHeading;
    m_velocity = glm::vec3(0.0f);
    m_chaseTime = 0.0f;
    m_active   = false;

    m_speedStart = Tuning::kMedusaSpeedStart;
    m_speedEnd   = Tuning::kMedusaSpeedEnd;

    if (blessedVerdict)
    {
        // The whole point of keeping the weighing: a true heart buys you a
        // slower pursuer and a longer lead, rather than just a different
        // cutscene.
        m_speedStart += Tuning::kBlessedSpeedBonus;
        m_speedEnd   += Tuning::kBlessedSpeedBonus;

        // Further back along the corridor's axis, so the lead is real.
        m_position.z -= Tuning::kBlessedHeadStart;
    }

    m_speed = m_speedStart;
}

bool Pursuer::windingUp() const
{
    return m_active && m_chaseTime < Tuning::kMedusaStartDelay;
}

float Pursuer::windUp() const
{
    if (!m_active) { return 0.0f; }
    return Easing::clamp01(m_chaseTime / Tuning::kMedusaStartDelay);
}

float Pursuer::speedFraction() const
{
    if (m_speedEnd <= m_speedStart)
    {
        return 0.0f;
    }
    return Easing::clamp01((m_speed - m_speedStart) / (m_speedEnd - m_speedStart));
}

float Pursuer::distanceTo(const glm::vec3& target) const
{
    const float dx = m_position.x - target.x;
    const float dz = m_position.z - target.z;
    return std::sqrt(dx * dx + dz * dz);
}

bool Pursuer::hasCaught(const glm::vec3& target) const
{
    if (!m_active)
    {
        return false;
    }

    // Squared comparison: the number is never shown, so the square root
    // would be paid for nothing.
    const float dx = m_position.x - target.x;
    const float dz = m_position.z - target.z;
    return (dx * dx + dz * dz) < (Tuning::kCatchRadius * Tuning::kCatchRadius);
}

void Pursuer::update(const glm::vec3& target, float deltaTime)
{
    if (!m_active)
    {
        m_velocity = glm::vec3(0.0f);
        return;
    }

    m_chaseTime += deltaTime;

    // --- head straight for him ----------------------------------------------
    glm::vec3 toTarget = target - m_position;
    toTarget.y = 0.0f;

    // --- uncoiling -----------------------------------------------------------
    // For the first beat she rears and turns to face him without moving. It
    // gives the player a head start he can see being granted, which is far
    // better than one he simply has.
    if (m_chaseTime < Tuning::kMedusaStartDelay)
    {
        m_velocity = glm::vec3(0.0f);
        m_speed = 0.0f;

        const float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
        if (d > 0.001f)
        {
            const float aim = glm::degrees(std::atan2(toTarget.x, toTarget.z));
            const float maxTurn = kTurnRate * deltaTime;
            const float delta = Easing::shortestAngleDelta(m_heading, aim);

            m_heading += std::clamp(delta, -maxTurn, maxTurn);
            m_heading = std::fmod(m_heading + 360.0f, 360.0f);
        }
        return;
    }

    // --- wind up -------------------------------------------------------------
    // She starts slower than the traveller and ends faster. That ordering is
    // the whole design: a pursuer who is always faster is hopeless, one who
    // is always slower is boring.
    const float chasing = m_chaseTime - Tuning::kMedusaStartDelay;
    m_speed = Easing::mix(m_speedStart, m_speedEnd,
                          Easing::clamp01(chasing / Tuning::kMedusaRampTime));

    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    if (distance > 0.001f)
    {
        const glm::vec3 direction = toTarget / distance;
        m_velocity = direction * m_speed;
        m_position += m_velocity * deltaTime;
    }
    else
    {
        m_velocity = glm::vec3(0.0f);
    }

    // Same walls as the player, from the same description of the world.
    m_world.collide(m_position, m_velocity, kBodyRadius);

    // --- face the way she is going -------------------------------------------
    const float planarSpeed =
        std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);

    if (planarSpeed > 0.05f)
    {
        const float targetHeading =
            glm::degrees(std::atan2(m_velocity.x, m_velocity.z));

        const float maxTurn = kTurnRate * deltaTime;
        const float delta = Easing::shortestAngleDelta(m_heading, targetHeading);

        m_heading += std::clamp(delta, -maxTurn, maxTurn);
        m_heading = std::fmod(m_heading + 360.0f, 360.0f);
    }
}

void Pursuer::applyTo(SceneNode& node) const
{
    node.position = m_position;
    node.rotation.y = m_heading;
}
