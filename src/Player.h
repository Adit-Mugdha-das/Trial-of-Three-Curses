#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>

#include "WorldShape.h"

class SceneNode;

// The player-controlled traveller.
//
// Owns position, velocity and facing; knows nothing about input devices or
// about the scene graph beyond being able to write itself into a node. main
// turns keys into a desired direction, this turns that into motion, and
// applyTo() pushes the result onto the traveller node.
class Player
{
public:
    void reset(const glm::vec3& position, float headingDegrees);

    // desiredDirection is an unnormalised world-space direction from input;
    // a zero vector means "stop". Y is ignored - the game is flat.
    void update(const glm::vec3& desiredDirection, float deltaTime);

    // Writes position and facing onto the traveller node. Call before the
    // scene's updateWorld(), or the change lands a frame late.
    void applyTo(SceneNode& node) const;

    void setBounds(const glm::vec2& minXZ, const glm::vec2& maxXZ);

    // The world is not a rectangle once the corridor exists: a wide room, a
    // narrow doorway, then a narrow corridor. These describe that shape.
    void setCorridor(float gateZ, float doorHalfWidth,
                     float corridorHalfWidth, float exitZ);

    // Until the slab is up, the doorway is solid.
    void setGateOpen(bool open) { m_world.gateOpen = open; }

    // The wall at the corridor's far end (the constellation gate's face).
    void setEndWall(float z) { m_world.endZ = z; }

    // Once the gate is down he may walk on into the garden.
    void setGardenOpen(bool open) { m_world.gardenOpen = open; }

    // Keeps him inside a circle - the sanctuary's dome.
    void keepWithin(const glm::vec3& centre, float radius);

    // A standing prop he cannot walk through, as a circle on the floor.
    int addObstacle(float x, float z, float radius, bool active = true,
                    bool rubble = false)
    {
        return m_world.addObstacle(x, z, radius, active, rubble);
    }
    void setObstacleActive(int index, bool active)
    {
        m_world.setObstacleActive(index, active);
    }

    const WorldShape& world() const { return m_world; }

    // Clipped by a falling stone. This caps his speed for a while; it is NOT
    // done by scaling the input direction, because update() normalises that
    // and the magnitude would be thrown away.
    // First person aims with the mouse, so the heading is an input rather
    // than a consequence of which way he happens to be sliding.
    void setHeading(float degrees) { m_heading = degrees; }
    void setAutoFace(bool value) { m_autoFace = value; }

    // Shoved clear of a landed stone. Cancels only the velocity heading
    // into it, so he slides around the obstacle instead of sticking to it.
    void pushOutOf(const glm::vec3& centre, float combinedRadius);

    // Stone is heavy. 0..1, from the gaze's exposure; slows him by up to
    // Tuning::kExposureSlowdown. Like a stun this changes his speed CAP, not
    // his input - update() normalises the input and would discard the scale.
    void setExposureSlow(float exposure01) { m_exposureSlow = exposure01; }

    void stun(float duration);
    bool stunned() const { return m_stunRemaining > 0.0f; }
    float stunRemaining() const { return m_stunRemaining; }

    const glm::vec3& position() const { return m_position; }
    float heading() const { return m_heading; }

    // Current planar speed, and that as a fraction of the maximum. Step B's
    // walk cycle is driven by the second one, so the legs move at the speed
    // he is actually travelling rather than the speed he is trying to.
    float speed() const;
    float speedFraction() const;

private:
    glm::vec3 m_position{ 0.0f, 0.0f, 5.5f };
    glm::vec3 m_velocity{ 0.0f, 0.0f, 0.0f };

    // Degrees. Forward is (sin(yaw), 0, cos(yaw)), so 180 faces -Z, which is
    // how the traveller is built: looking at the scale.
    float m_heading = 180.0f;

    float m_stunRemaining = 0.0f;
    float m_exposureSlow  = 0.0f;
    bool  m_autoFace = true;

    // Shared with Medusa - see WorldShape.h.
    WorldShape m_world;
};

#endif // PLAYER_H
