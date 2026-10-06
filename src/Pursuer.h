#ifndef PURSUER_H
#define PURSUER_H

#include <glm/glm.hpp>

#include "WorldShape.h"

class SceneNode;

// Medusa, once she starts moving.
//
// The chase is deliberately the simplest thing that works: head straight for
// where the traveller is, right now, at a speed that climbs the longer it
// goes on. No pathfinding, no prediction. The corridor is a straight line, so
// anything cleverer would look identical and be harder to tune.
//
// The tension does not come from her being clever. It comes from her ending
// up faster than the player while the falling stones slow him down.
class Pursuer
{
public:
    // `blessedVerdict` applies the handicap: a traveller whose heart was true
    // gets a slower pursuer and a head start.
    void reset(const glm::vec3& home, float homeHeading, bool blessedVerdict);

    void setWorld(const WorldShape& world) { m_world = world; }
    void setGateOpen(bool open) { m_world.gateOpen = open; }
    void setObstacleActive(int index, bool active)
    {
        m_world.setObstacleActive(index, active);
    }

    void setActive(bool active) { m_active = active; }

    // Keeps her outside a circle - the sanctuary. True while she is pressed
    // against it.
    bool holdOutside(const glm::vec3& centre, float radius);
    bool active() const { return m_active; }

    void update(const glm::vec3& target, float deltaTime);
    void applyTo(SceneNode& node) const;

    const glm::vec3& position() const { return m_position; }
    float heading() const { return m_heading; }
    float speed() const { return m_speed; }

    // 0 at her opening speed, 1 once fully wound up. Drives how frantic the
    // snakes look.
    float speedFraction() const;

    // True while she is still uncoiling and has not started moving.
    bool windingUp() const;

    // 0..1 through that wind-up, for the snakes to build on.
    float windUp() const;

    // Flat XZ distance; height is never relevant on one floor.
    float distanceTo(const glm::vec3& target) const;
    bool hasCaught(const glm::vec3& target) const;

private:
    glm::vec3 m_position{ 6.0f, 0.0f, -1.5f };
    glm::vec3 m_velocity{ 0.0f, 0.0f, 0.0f };

    float m_heading   = -35.0f;
    float m_speed     = 0.0f;
    float m_chaseTime = 0.0f;
    bool  m_active    = false;

    // Copied at reset() so the verdict handicap is baked in for the run.
    float m_speedStart = 0.0f;
    float m_speedEnd   = 0.0f;

    WorldShape m_world;
};

#endif // PURSUER_H
