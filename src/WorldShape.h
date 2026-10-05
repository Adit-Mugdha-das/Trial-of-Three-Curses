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
    static constexpr int kMaxObstacles = 8;

    struct Obstacle
    {
        float x = 0.0f;
        float z = 0.0f;
        float radius = 0.0f;
    };

    Obstacle obstacles[kMaxObstacles];
    int obstacleCount = 0;

    void addObstacle(float x, float z, float radius);

    // Pushes `position` back out of any wall it has entered and cancels only
    // the part of `velocity` heading into that wall, so sliding along a
    // surface keeps the other axis.
    void collide(glm::vec3& position, glm::vec3& velocity, float radius) const;
};

#endif // WORLDSHAPE_H
