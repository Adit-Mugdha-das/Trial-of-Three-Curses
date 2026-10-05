#include "Debris.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "EscapeTuning.h"
#include "Player.h"
#include "Primitives.h"

namespace
{
    // It shrinks away over the last of that, rather than fading. Shrinking
    // keeps it in the opaque pass; fading would need depth sorting against
    // the energy rings for no visual gain.
    constexpr float kCrumbleDuration = 0.9f;

    // A stone only counts as hitting him if it is still low enough to be at
    // body height - one sailing past overhead should not.
    constexpr float kBodyHeight = 2.2f;
}

float Debris::randomRange(float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    return dist(m_rng);
}

void Debris::initPool(int maxStones)
{
    m_stones.assign(static_cast<std::size_t>(maxStones), Stone{});
    m_material = Materials::stone;
}

bool Debris::init(int maxStones)
{
    initPool(maxStones);

    // Deliberately coarse: a 10x14 sphere reads as a chipped block of
    // masonry, where a smooth one reads as a ball.
    m_stoneMesh.upload(Primitives::sphere(0.5f, 10, 14));
    m_ringMesh.upload(Primitives::disk(0.5f, 28));

    return true;
}

void Debris::shutdown()
{
    m_stones.clear();
    m_impacts.clear();
}

void Debris::reset()
{
    for (Stone& stone : m_stones)
    {
        stone.active = false;
        stone.landed = false;
        stone.hasHit = false;
    }

    m_impacts.clear();
    m_spawnTimer = 0.0f;
    m_shake = 0.0f;
}

void Debris::setCorridor(float gateZ, float halfWidth, float exitZ, float ceilingY)
{
    m_gateZ     = gateZ;
    m_halfWidth = halfWidth;
    m_exitZ     = exitZ;
    m_ceilingY  = ceilingY;
}

int Debris::activeCount() const
{
    int count = 0;
    for (const Stone& stone : m_stones)
    {
        if (stone.active) { ++count; }
    }
    return count;
}

int Debris::airborneCount() const
{
    int count = 0;
    for (const Stone& stone : m_stones)
    {
        if (stone.active && !stone.landed) { ++count; }
    }
    return count;
}

bool Debris::nearestThreat(const glm::vec3& from, float lookAhead,
                           glm::vec3& outPosition) const
{
    bool found = false;
    float nearest = 0.0f;

    for (const Stone& stone : m_stones)
    {
        if (!stone.active || stone.landed)
        {
            continue;
        }

        const float ahead = stone.position.z - from.z;
        if (ahead < -1.0f || ahead > lookAhead)
        {
            continue;
        }

        // Only a stone roughly in his lane is worth swerving for.
        if (std::fabs(stone.position.x - from.x) > stone.radius + 1.0f)
        {
            continue;
        }

        if (!found || ahead < nearest)
        {
            found = true;
            nearest = ahead;
            outPosition = stone.position;
        }
    }

    return found;
}

void Debris::spawn(Stone& stone, const glm::vec3& playerPosition)
{
    // Ahead of him, never on top of him: a stone spawned at his feet would
    // be unavoidable, and the warning ring would have nothing to warn about.
    const float ahead = randomRange(Tuning::kStoneAheadMin, Tuning::kStoneAheadMax);

    float z = playerPosition.z + ahead;
    z = std::min(z, m_exitZ - 2.0f);

    // Nothing falls in the chamber - the collapse is the corridor's.
    if (z < m_gateZ + 2.0f)
    {
        stone.active = false;
        return;
    }

    stone.radius = randomRange(0.42f, 0.88f);

    const float margin = m_halfWidth - stone.radius - 0.2f;
    stone.position = { randomRange(-margin, margin), m_ceilingY, z };

    stone.velocity = { 0.0f, 0.0f, 0.0f };
    stone.rotation = { randomRange(0.0f, 360.0f), randomRange(0.0f, 360.0f),
                       randomRange(0.0f, 360.0f) };
    stone.spin     = { randomRange(-160.0f, 160.0f), randomRange(-160.0f, 160.0f),
                       randomRange(-160.0f, 160.0f) };

    stone.life   = Tuning::kStoneRestDuration;
    stone.landed = false;
    stone.hasHit = false;
    stone.active = true;
}

void Debris::collapseAround(const glm::vec3& centre, int count, float spread)
{
    for (Stone& stone : m_stones)
    {
        if (count <= 0)
        {
            break;
        }

        if (stone.active)
        {
            continue;
        }

        // spawn() places things relative to the player and ahead of him;
        // here the position is dictated, so it is set up directly.
        stone.radius = randomRange(0.45f, 0.95f);

        const float margin = m_halfWidth - stone.radius - 0.2f;
        const float z = std::min(m_exitZ - 1.0f,
                                 std::max(m_gateZ + 1.0f,
                                          centre.z + randomRange(-spread, spread)));

        stone.position = { randomRange(-margin, margin),
                           m_ceilingY + randomRange(0.0f, 1.5f), z };
        stone.velocity = { 0.0f, randomRange(-2.0f, 0.0f), 0.0f };
        stone.rotation = { randomRange(0.0f, 360.0f), randomRange(0.0f, 360.0f),
                           randomRange(0.0f, 360.0f) };
        stone.spin     = { randomRange(-200.0f, 200.0f), randomRange(-200.0f, 200.0f),
                           randomRange(-200.0f, 200.0f) };

        stone.life   = Tuning::kStoneRestDuration;
        stone.landed = false;
        stone.hasHit = true;      // scenery: this volley never stuns anyone
        stone.active = true;

        --count;
    }
}

bool Debris::update(float deltaTime, const glm::vec3& playerPosition)
{
    m_shake = std::max(0.0f, m_shake - deltaTime * 2.6f);

    bool struckPlayer = false;

    // --- spawning ------------------------------------------------------------
    if (m_active && m_intensity > 0.0f)
    {
        m_spawnTimer -= deltaTime;

        if (m_spawnTimer <= 0.0f)
        {
            m_spawnTimer = Tuning::kStoneInterval / std::max(0.2f, m_intensity);

            for (Stone& stone : m_stones)
            {
                if (!stone.active)
                {
                    spawn(stone, playerPosition);
                    break;
                }
            }
        }
    }

    // --- simulation ----------------------------------------------------------
    for (Stone& stone : m_stones)
    {
        if (!stone.active)
        {
            continue;
        }

        if (!stone.landed)
        {
            stone.velocity.y -= Tuning::kStoneGravity * deltaTime;
            stone.position   += stone.velocity * deltaTime;
            stone.rotation   += stone.spin * deltaTime;

            // --- does it clip him on the way down? ---
            if (!stone.hasHit)
            {
                const float dx = stone.position.x - playerPosition.x;
                const float dz = stone.position.z - playerPosition.z;
                const float reach = stone.radius + Tuning::kPlayerRadius;

                const bool overlapping = (dx * dx + dz * dz) < (reach * reach);
                const bool lowEnough =
                    (stone.position.y - stone.radius) < kBodyHeight;

                if (overlapping && lowEnough)
                {
                    stone.hasHit = true;
                    struckPlayer = true;
                    m_shake = 1.0f;
                }
            }

            // --- landing ---
            if (stone.position.y - stone.radius <= 0.0f)
            {
                stone.position.y = stone.radius;
                stone.velocity   = glm::vec3(0.0f);
                stone.spin      *= 0.15f;
                stone.landed     = true;

                m_impacts.push_back(stone.position);

                // Shake scales with mass and with how close it landed, so a
                // near miss is felt and a distant one is merely heard.
                const float dx = stone.position.x - playerPosition.x;
                const float dz = stone.position.z - playerPosition.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                const float nearness = std::max(0.0f, 1.0f - distance / 14.0f);

                m_shake = std::max(m_shake, 0.75f * nearness * (stone.radius / 0.88f));
            }
        }
        else
        {
            stone.life -= deltaTime;
            stone.rotation += stone.spin * deltaTime;

            if (stone.life <= 0.0f)
            {
                stone.active = false;
            }
        }
    }

    return struckPlayer;
}

void Debris::pushPlayerOut(Player& player) const
{
    for (const Stone& stone : m_stones)
    {
        // Only settled rubble blocks the way; a stone still in the air is
        // handled as a strike, not as a wall.
        if (!stone.active || !stone.landed)
        {
            continue;
        }

        // A crumbling stone stops obstructing as it shrinks, or he would be
        // held by something he can no longer see.
        const float shrink = std::min(1.0f, stone.life / kCrumbleDuration);
        const float reach = stone.radius * shrink + Tuning::kPlayerRadius;

        const glm::vec3 p = player.position();
        const float dx = p.x - stone.position.x;
        const float dz = p.z - stone.position.z;
        const float distanceSquared = dx * dx + dz * dz;

        if (distanceSquared >= reach * reach || distanceSquared < 1e-6f)
        {
            continue;
        }

        player.pushOutOf(stone.position, reach);
    }
}

bool Debris::popImpact(glm::vec3& outPosition)
{
    if (m_impacts.empty())
    {
        return false;
    }

    outPosition = m_impacts.back();
    m_impacts.pop_back();
    return true;
}

void Debris::draw(const Shader& shader) const
{
    shader.setInt("uPetrifyEnabled", 0);
    m_material.upload(shader);

    for (const Stone& stone : m_stones)
    {
        if (!stone.active)
        {
            continue;
        }

        // Shrink rather than fade, so these stay in the opaque pass.
        const float shrink = stone.landed
                           ? std::min(1.0f, stone.life / kCrumbleDuration)
                           : 1.0f;

        if (shrink <= 0.01f)
        {
            continue;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, stone.position);
        model = glm::rotate(model, glm::radians(stone.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(stone.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(stone.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(stone.radius * 2.0f * shrink));

        shader.setMat4("uModel", model);
        shader.setMat3("uNormalMatrix",
                       glm::mat3(glm::transpose(glm::inverse(model))));

        m_stoneMesh.draw();
    }
}

void Debris::drawWarnings(const Shader& shader) const
{
    // A flat disc on the floor under every stone still in the air. This is
    // what turns an unavoidable hazard into a dodgeable one.
    Material warning;
    warning.ka = { 0.0f, 0.0f, 0.0f };
    warning.kd = { 0.0f, 0.0f, 0.0f };
    warning.ks = { 0.0f, 0.0f, 0.0f };

    shader.setInt("uPetrifyEnabled", 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);
    glDisable(GL_CULL_FACE);

    for (const Stone& stone : m_stones)
    {
        if (!stone.active || stone.landed)
        {
            continue;
        }

        // Time left before it lands, from the usual constant-acceleration
        // solution. Driving the warning from time-to-impact rather than from
        // height means a big slow stone and a small fast one both warn for
        // the right length of time.
        const float drop = std::max(0.0f, stone.position.y - stone.radius);
        const float timeToImpact =
            std::sqrt(2.0f * drop / Tuning::kStoneGravity);

        // Urgency: 0 when it is far off, 1 at the moment of impact.
        const float urgency = 1.0f - std::min(1.0f, timeToImpact / 1.1f);

        // Tightens onto the true footprint as it falls, so the ring reads as
        // a prediction converging on an answer.
        const float radius = stone.radius * (2.1f - 1.0f * urgency);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(stone.position.x, 0.03f, stone.position.z));
        model = glm::scale(model, glm::vec3(radius * 2.0f, 1.0f, radius * 2.0f));

        warning.emissive = glm::vec3(0.85f, 0.18f, 0.10f) * (0.35f + 0.65f * urgency);
        warning.opacity  = 0.20f + 0.55f * urgency;
        warning.upload(shader);

        shader.setMat4("uModel", model);
        shader.setMat3("uNormalMatrix", glm::mat3(1.0f));

        m_ringMesh.draw();
    }

    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
