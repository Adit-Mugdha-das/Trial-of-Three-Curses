#include "WorldShape.h"

#include <algorithm>
#include <cmath>

void WorldShape::addObstacle(float x, float z, float radius)
{
    if (obstacleCount >= kMaxObstacles)
    {
        return;
    }

    obstacles[obstacleCount].x      = x;
    obstacles[obstacleCount].z      = z;
    obstacles[obstacleCount].radius = radius;
    ++obstacleCount;
}

void WorldShape::collide(glm::vec3& position, glm::vec3& velocity,
                         float radius) const
{
    auto clampX = [&position, &velocity](float low, float high)
    {
        if (position.x < low)
        {
            position.x = low;
            velocity.x = std::max(0.0f, velocity.x);
        }
        if (position.x > high)
        {
            position.x = high;
            velocity.x = std::min(0.0f, velocity.x);
        }
    };

    auto clampZ = [&position, &velocity](float low, float high)
    {
        if (position.z < low)
        {
            position.z = low;
            velocity.z = std::max(0.0f, velocity.z);
        }
        if (position.z > high)
        {
            position.z = high;
            velocity.z = std::min(0.0f, velocity.z);
        }
    };

    // --- standing props ------------------------------------------------------
    // Resolved before the walls, so that being shoved off a pedestal can
    // never end up pushing anyone through a wall: the wall pass below runs
    // afterwards and wins.
    for (int i = 0; i < obstacleCount; ++i)
    {
        const Obstacle& prop = obstacles[i];

        const float dx = position.x - prop.x;
        const float dz = position.z - prop.z;
        const float reach = prop.radius + radius;

        const float distanceSquared = dx * dx + dz * dz;
        if (distanceSquared >= reach * reach)
        {
            continue;
        }

        // Dead centre gives no direction to push along, so pick one rather
        // than dividing by zero.
        glm::vec2 away(1.0f, 0.0f);
        if (distanceSquared > 1e-6f)
        {
            const float distance = std::sqrt(distanceSquared);
            away = glm::vec2(dx / distance, dz / distance);
        }

        position.x = prop.x + away.x * reach;
        position.z = prop.z + away.y * reach;

        // Cancel only the part of the motion heading into it, so he slides
        // around the pedestal instead of sticking to it.
        const float into = velocity.x * away.x + velocity.z * away.y;
        if (into < 0.0f)
        {
            velocity.x -= away.x * into;
            velocity.z -= away.y * into;
        }
    }

    if (position.z <= gateZ)
    {
        // --- inside the chamber ---
        clampX(roomMinX + radius, roomMaxX - radius);
        clampZ(roomMinZ + radius, exitZ + 4.0f);

        // The front wall is solid except for the doorway. Off to one side, or
        // with the slab still down, the wall stops you.
        const bool linedUpWithDoor = std::fabs(position.x) < (doorHalfWidth - radius);

        if (!(gateOpen && linedUpWithDoor) && position.z > gateZ - radius)
        {
            position.z = gateZ - radius;
            velocity.z = std::min(0.0f, velocity.z);
        }
    }
    else
    {
        // --- inside the corridor ---
        clampX(-corridorHalfWidth + radius, corridorHalfWidth - radius);
        clampZ(gateZ, exitZ + 4.0f);
    }
}
