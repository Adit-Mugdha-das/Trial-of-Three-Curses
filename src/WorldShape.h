#ifndef WORLDSHAPE_H
#define WORLDSHAPE_H

#include <glm/glm.hpp>

// The walkable region: a wide chamber, a narrow doorway in its front wall,
// then a narrow corridor running to the exit.
//
// Both the player and Medusa are confined by exactly the same geometry, so it
// lives here rather than in either of them. Two copies of this logic that
// drifted apart would let one of them walk through a wall the other cannot.
struct WorldShape
{
    // Chamber, already inset by nothing - the caller's radius is taken off
    // inside collide().
    float roomMinX = -12.7f;
    float roomMaxX =  12.7f;
    float roomMinZ = -10.7f;

    float gateZ             = 11.5f;
    float doorHalfWidth     = 3.5f;
    float corridorHalfWidth = 4.0f;
    float exitZ             = 70.0f;

    // The doorway is solid stone until the slab is raised.
    bool gateOpen = false;

    // Standing props - the scale, the lamp - as circles on the floor. A fixed
    // array rather than a vector so the whole struct stays trivially
    // copyable and allocation-free; both the player and Medusa hold one.
    // Fallen ceiling sections add their own circles, switched on only once
    // they are down - hence the room for more than the standing props need.
    static constexpr int kMaxObstacles = 48;

    struct Obstacle
    {
        float x = 0.0f;
        float z = 0.0f;
        float radius = 0.0f;
        bool  active = true;
        bool  rubble = false;   // a fallen ceiling section: steered round
    };

    Obstacle obstacles[kMaxObstacles];
    int obstacleCount = 0;

    // Returns the obstacle's index, or -1 if the table is full.
    int  addObstacle(float x, float z, float radius, bool active = true,
                     bool rubble = false);
    void setObstacleActive(int index, bool active);

    // Pushes `position` back out of any wall it has entered and cancels only
    // the part of `velocity` heading into that wall, so sliding along a
    // surface keeps the other axis.
    void collide(glm::vec3& position, glm::vec3& velocity, float radius) const;

    // A direction as close as possible to `desired` that does not walk into
    // fallen rubble within `lookAhead`. Sliding alone is not enough against a
    // long block met head-on: the push-out cancels all of the motion and
    // whoever hit it simply stands there. Only rubble is considered - the
    // pillars and props behave exactly as they always have.
    glm::vec3 avoidRubble(const glm::vec3& position, const glm::vec3& desired,
                          float radius, float lookAhead) const;
};

#endif // WORLDSHAPE_H
