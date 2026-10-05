#include "Gaze.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"
#include "EscapeTuning.h"

namespace
{
    glm::vec3 yawToDirection(float yawDegrees)
    {
        const float r = glm::radians(yawDegrees);
        return { std::sin(r), 0.0f, std::cos(r) };
    }

    float normaliseAngle(float degrees)
    {
        return std::fmod(std::fmod(degrees, 360.0f) + 360.0f, 360.0f);
    }
}

void Gaze::addCover(float x, float z, float radius, float height)
{
    if (m_coverCount >= kMaxCover)
    {
        return;
    }
    m_cover[m_coverCount++] = { x, z, radius, height };
}

void Gaze::clearCover()
{
    m_coverCount = 0;
}

void Gaze::setDoorway(float gateZ, float doorHalfWidth)
{
    m_gateZ         = gateZ;
    m_doorHalfWidth = doorHalfWidth;
    m_hasDoorway    = true;
}

void Gaze::reset()
{
    m_phase = Phase::Dormant;
    m_t = 0.0f;
    m_attacks = 0;
    m_side = 1;
    m_exposure = 0.0f;
    m_exposedNow = false;
    m_visibility = 1.0f;
    m_eyeLevel = 0.3f;
    m_beamWidth = 0.0f;
    m_beamLength = 0.0f;
}

const char* Gaze::phaseName() const
{
    switch (m_phase)
    {
        case Phase::Dormant:   return "dormant";
        case Phase::Cooldown:  return "cooldown";
        case Phase::Telegraph: return "telegraph";
        case Phase::Lock:      return "LOCK";
        case Phase::Sweep:     return "SWEEP";
    }
    return "?";
}

bool Gaze::beamLive() const
{
    return m_phase == Phase::Lock || m_phase == Phase::Sweep;
}

float Gaze::warning() const
{
    return (m_phase == Phase::Telegraph || m_phase == Phase::Lock
            || m_phase == Phase::Sweep) ? 1.0f : 0.0f;
}

// ---------------------------------------------------------------- line of sight

float Gaze::coverHit(const Cover& c, const glm::vec3& a, const glm::vec3& b)
{
    // The segment against a vertical cylinder: a circle test in XZ to find
    // where the line passes through the footprint, then a height test to see
    // whether it is still low enough there to hit the shaft.
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float fx = a.x - c.x;
    const float fz = a.z - c.z;

    const float A = dx * dx + dz * dz;
    const float C = fx * fx + fz * fz - c.radius * c.radius;

    if (A < 1e-8f)
    {
        // Straight up or down: only matters if the point is inside.
        return (C < 0.0f && a.y < c.height) ? 0.0f : 2.0f;
    }

    const float B = 2.0f * (fx * dx + fz * dz);
    const float discriminant = B * B - 4.0f * A * C;
    if (discriminant < 0.0f)
    {
        return 2.0f;
    }

    const float root = std::sqrt(discriminant);
    const float t1 = (-B - root) / (2.0f * A);
    const float t2 = (-B + root) / (2.0f * A);

    if (t2 < 0.0f || t1 > 1.0f)
    {
        return 2.0f;
    }

    const float tIn  = std::max(t1, 0.0f);
    const float tOut = std::min(t2, 1.0f);

    // The line is straight, so its lowest point inside the footprint is at
    // one end or the other.
    const float yIn  = a.y + (b.y - a.y) * tIn;
    const float yOut = a.y + (b.y - a.y) * tOut;

    if (std::min(yIn, yOut) >= c.height)
    {
        return 2.0f;       // passes clean over the top
    }

    return tIn;
}

bool Gaze::lineBlocked(const glm::vec3& a, const glm::vec3& b) const
{
    for (int i = 0; i < m_coverCount; ++i)
    {
        if (coverHit(m_cover[i], a, b) <= 1.0f)
        {
            return true;
        }
    }

    // The chamber's front wall, with its doorway. A line that crosses the
    // wall's plane outside the opening goes through stone.
    if (m_hasDoorway && (a.z - m_gateZ) * (b.z - m_gateZ) < 0.0f)
    {
        const float t = (m_gateZ - a.z) / (b.z - a.z);
        const float x = a.x + t * (b.x - a.x);
        if (std::fabs(x) > m_doorHalfWidth)
        {
            return true;
        }
    }

    return false;
}

float Gaze::visibilityOf(const glm::vec3& eye, const glm::vec3& targetFeet) const
{
    const glm::vec3 chest = targetFeet + glm::vec3(0.0f, Tuning::kGazeTargetHeight, 0.0f);

    glm::vec2 along(chest.x - eye.x, chest.z - eye.z);
    const float length = std::sqrt(along.x * along.x + along.y * along.y);
    if (length < 1e-4f)
    {
        return 1.0f;
    }
    along /= length;

    // Perpendicular to the line of sight, in the floor plane.
    const glm::vec2 side(-along.y, along.x);

    constexpr float kShoulder = 0.40f;
    const float offsets[3] = { -kShoulder, 0.0f, kShoulder };

    int seen = 0;
    for (float offset : offsets)
    {
        const glm::vec3 p = chest + glm::vec3(side.x * offset, 0.0f, side.y * offset);
        if (!lineBlocked(eye, p))
        {
            ++seen;
        }
    }

    return static_cast<float>(seen) / 3.0f;
}

// ------------------------------------------------------------------- the cycle

void Gaze::enter(Phase next)
{
    m_phase = next;
    m_t = 0.0f;
}

void Gaze::trackYaw(float desired, float deltaTime)
{
    const float maxTurn = Tuning::kGazeTrackRate * deltaTime;
    const float delta = Easing::shortestAngleDelta(m_yaw, desired);
    m_yaw = normaliseAngle(m_yaw + std::clamp(delta, -maxTurn, maxTurn));
}

void Gaze::update(const glm::vec3& eye, const glm::vec3& targetFeet, float deltaTime)
{
    const glm::vec3 chest = targetFeet + glm::vec3(0.0f, Tuning::kGazeTargetHeight, 0.0f);

    const float dx = chest.x - eye.x;
    const float dz = chest.z - eye.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    const float targetYaw = (distance > 1e-3f)
                          ? normaliseAngle(glm::degrees(std::atan2(dx, dz)))
                          : m_yaw;

    m_t += deltaTime;
    m_exposedNow = false;

    // --- phase logic --------------------------------------------------------
    if (!m_active)
    {
        // Abandon any attack in progress and just watch him.
        m_phase = Phase::Dormant;
        trackYaw(targetYaw, deltaTime);
        m_eyeLevel  = std::max(0.3f, m_eyeLevel - deltaTime * 1.5f);
        m_beamWidth = std::max(0.0f, m_beamWidth - deltaTime * 3.0f);
    }
    else
    {
        if (m_phase == Phase::Dormant)
        {
            enter(Phase::Cooldown);
            m_cooldownLength = Tuning::kGazeFirstDelay;
            m_yaw = targetYaw;
        }

        const float arc = Tuning::kGazeSweepArc;

        switch (m_phase)
        {
            case Phase::Cooldown:
            {
                trackYaw(targetYaw, deltaTime);
                m_eyeLevel  = std::max(0.3f, m_eyeLevel - deltaTime * 1.2f);
                m_beamWidth = std::max(0.0f, m_beamWidth - deltaTime * 2.5f);

                if (m_t >= m_cooldownLength && distance <= Tuning::kGazeRange)
                {
                    // Alternate the direction of the sweep, so it is never
                    // the same threat twice in a row.
                    m_side = (m_attacks % 2 == 0) ? 1 : -1;
                    ++m_attacks;
                    enter(Phase::Telegraph);
                }
                break;
            }

            case Phase::Telegraph:
            {
                const float p = Easing::clamp01(m_t / Tuning::kGazeTelegraph);

                // For the first part she simply follows him. Then the beam
                // winds back toward one wall, which is the visible "here it
                // comes" - and the start edge of the sweep.
                const float windBack = Easing::smoothstep01((p - 0.55f) / 0.45f);
                trackYaw(targetYaw - static_cast<float>(m_side) * arc * windBack,
                         deltaTime);

                m_eyeLevel  = 0.3f + 0.7f * Easing::smoothstep01(p);
                m_beamWidth = 0.10f + 0.16f * p;

                if (m_t >= Tuning::kGazeTelegraph)
                {
                    enter(Phase::Lock);
                }
                break;
            }

            case Phase::Lock:
            {
                // The aim does not move at all: that stillness is the cue.
                const float p = Easing::clamp01(m_t / Tuning::kGazeLock);
                m_eyeLevel  = 1.0f;
                m_beamWidth = Easing::mix(0.26f, 1.0f, Easing::smoothstep01(p));

                if (m_t >= Tuning::kGazeLock)
                {
                    m_sweepFrom = m_yaw;
                    enter(Phase::Sweep);
                }
                break;
            }

            case Phase::Sweep:
            {
                const float u = Easing::clamp01(m_t / Tuning::kGazeSweep);
                m_yaw = normaliseAngle(m_sweepFrom
                                       + static_cast<float>(m_side) * 2.0f * arc * u);
                m_eyeLevel  = 1.0f;
                m_beamWidth = 1.0f;

                if (m_t >= Tuning::kGazeSweep)
                {
                    enter(Phase::Cooldown);
                    m_cooldownLength = Tuning::kGazeCooldown;
                }
                break;
            }

            case Phase::Dormant:
                break;
        }
    }

    // --- aim point ------------------------------------------------------------
    // At HIS range along the beam, so it always reaches him however far he has
    // run, and at chest height.
    const float range = std::max(distance, 2.0f);
    const glm::vec3 direction = yawToDirection(m_yaw);
    m_aimPoint = glm::vec3(eye.x + direction.x * range, chest.y, eye.z + direction.z * range);

    // --- how far the beam visibly reaches ---------------------------------------
    {
        const glm::vec3 tip = m_aimPoint + glm::normalize(m_aimPoint - eye) * 0.6f;
        float reach = 1.0f;
        for (int i = 0; i < m_coverCount; ++i)
        {
            reach = std::min(reach, coverHit(m_cover[i], eye, tip));
        }
        m_beamLength = glm::length(tip - eye) * std::min(reach, 1.0f);
    }

    // --- exposure ---------------------------------------------------------------
    m_visibility = visibilityOf(eye, targetFeet);

    bool inCone = false;
    if (beamLive() && distance <= Tuning::kGazeRange)
    {
        // His body has width, so a beam passing just beside him still grazes
        // him: widen the cone by the angle he subtends.
        const float subtended = glm::degrees(std::atan2(Tuning::kPlayerRadius,
                                                        std::max(distance, 0.5f)));
        const float off = std::fabs(Easing::shortestAngleDelta(m_yaw, targetYaw));
        inCone = off < Tuning::kGazeConeHalf + subtended;
    }

    if (inCone && m_visibility > 0.0f)
    {
        // Partial cover gives partial exposure - a shoulder poking out is
        // hurt more slowly than a whole body.
        m_exposure += (m_visibility / Tuning::kGazeFillTime) * deltaTime;
        m_exposedNow = true;
    }
    else
    {
        m_exposure -= deltaTime / Tuning::kGazeDecayTime;
    }

    m_exposure = Easing::clamp01(m_exposure);
}
