#include "Player.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"
#include "EscapeTuning.h"
#include "SceneNode.h"

void Player::reset(const glm::vec3& position, float headingDegrees)
{
    m_position = position;
    m_velocity = glm::vec3(0.0f);
    m_heading  = headingDegrees;
    m_stunRemaining = 0.0f;
    m_exposureSlow  = 0.0f;
}

void Player::setBounds(const glm::vec2& minXZ, const glm::vec2& maxXZ)
{
    // The caller passes bounds already inset by the player's radius; the
    // shared shape insets it itself, so undo that here.
    m_world.roomMinX = minXZ.x - Tuning::kPlayerRadius;
    m_world.roomMaxX = maxXZ.x + Tuning::kPlayerRadius;
    m_world.roomMinZ = minXZ.y - Tuning::kPlayerRadius;
}

void Player::setCorridor(float gateZ, float doorHalfWidth,
                         float corridorHalfWidth, float exitZ)
{
    m_world.gateZ             = gateZ;
    m_world.doorHalfWidth     = doorHalfWidth;
    m_world.corridorHalfWidth = corridorHalfWidth;
    m_world.exitZ             = exitZ;
}

float Player::speed() const
{
    return std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
}

float Player::speedFraction() const
{
    return Easing::clamp01(speed() / Tuning::kPlayerSpeed);
}

void Player::keepWithin(const glm::vec3& centre, float radius)
{
    const float dx = m_position.x - centre.x;
    const float dz = m_position.z - centre.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= radius || distance < 1e-6f)
    {
        return;
    }

    const glm::vec3 out(dx / distance, 0.0f, dz / distance);
    m_position.x = centre.x + out.x * radius;
    m_position.z = centre.z + out.z * radius;

    // Cancel only the outward part, so he slides round the inside of it.
    const float outward = m_velocity.x * out.x + m_velocity.z * out.z;
    if (outward > 0.0f)
    {
        m_velocity.x -= out.x * outward;
        m_velocity.z -= out.z * outward;
    }
}

void Player::pushOutOf(const glm::vec3& centre, float combinedRadius)
{
    const float dx = m_position.x - centre.x;
    const float dz = m_position.z - centre.z;
    float distanceSquared = dx * dx + dz * dz;

    if (distanceSquared >= combinedRadius * combinedRadius)
    {
        return;
    }

    // Dead centre: no direction to push along, so pick one rather than
    // dividing by zero.
    glm::vec3 away(1.0f, 0.0f, 0.0f);
    if (distanceSquared > 1e-6f)
    {
        const float distance = std::sqrt(distanceSquared);
        away = glm::vec3(dx / distance, 0.0f, dz / distance);
    }

    m_position.x = centre.x + away.x * combinedRadius;
    m_position.z = centre.z + away.z * combinedRadius;

    // Remove only the component heading into the stone.
    const float into = m_velocity.x * away.x + m_velocity.z * away.z;
    if (into < 0.0f)
    {
        m_velocity.x -= away.x * into;
        m_velocity.z -= away.z * into;
    }

    // The walls still win: being shoved by a stone must not push him
    // through the corridor side.
    m_world.collide(m_position, m_velocity, Tuning::kPlayerRadius);
}

void Player::stun(float duration)
{
    // Take the longer of the two rather than adding: a flurry of hits should
    // not stack into an unrecoverable crawl.
    m_stunRemaining = std::max(m_stunRemaining, duration);
}

void Player::update(const glm::vec3& desiredDirection, float deltaTime)
{
    m_stunRemaining = std::max(0.0f, m_stunRemaining - deltaTime);

    // --- desired velocity --------------------------------------------------
    glm::vec3 direction(desiredDirection.x, 0.0f, desiredDirection.z);

    const float lengthSquared = direction.x * direction.x + direction.z * direction.z;

    glm::vec3 desiredVelocity(0.0f);
    if (lengthSquared > 1e-6f)
    {
        // Normalising is what stops diagonal movement being 1.41x faster than
        // movement along an axis - the classic bug in eight-way controls.
        direction /= std::sqrt(lengthSquared);

        // The cap, not the input, is what a stun changes.
        float topSpeed = stunned()
                       ? Tuning::kPlayerSpeed * Tuning::kStunSpeedFactor
                       : Tuning::kPlayerSpeed;

        topSpeed *= 1.0f - Tuning::kExposureSlowdown * m_exposureSlow;

        desiredVelocity = direction * topSpeed;
    }

    // --- accelerate toward it ----------------------------------------------
    // Moving toward the target rather than assigning it means starts and stops
    // have weight. Snapping the velocity makes a character feel weightless.
    glm::vec3 change = desiredVelocity - m_velocity;
    const float changeLength = std::sqrt(change.x * change.x + change.z * change.z);
    const float maxChange = Tuning::kPlayerAccel * deltaTime;

    if (changeLength > maxChange && changeLength > 1e-6f)
    {
        change *= (maxChange / changeLength);
    }

    m_velocity += change;
    m_velocity.y = 0.0f;

    // --- move ---------------------------------------------------------------
    m_position += m_velocity * deltaTime;

    // --- walls ---------------------------------------------------------------
    m_world.collide(m_position, m_velocity, Tuning::kPlayerRadius);

    // --- facing ---------------------------------------------------------------
    // Turn toward the direction of travel, not the direction of input: if a
    // wall is refusing the input, he should face where he is actually sliding.
    if (m_autoFace && speed() > 0.15f)
    {
        const float targetHeading =
            glm::degrees(std::atan2(m_velocity.x, m_velocity.z));

        const float maxTurn = Tuning::kPlayerTurnRate * deltaTime;
        const float delta = Easing::shortestAngleDelta(m_heading, targetHeading);

        m_heading += std::clamp(delta, -maxTurn, maxTurn);
        m_heading = std::fmod(m_heading + 360.0f, 360.0f);
    }
}

void Player::applyTo(SceneNode& node) const
{
    node.position = m_position;
    node.rotation.y = m_heading;
}
