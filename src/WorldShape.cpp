#include "WorldShape.h"

#include <algorithm>
#include <cmath>

int WorldShape::addObstacle(float x, float z, float radius, bool active, bool rubble)
{
    if (obstacleCount >= kMaxObstacles)
    {
        return -1;
    }

    obstacles[obstacleCount].x      = x;
    obstacles[obstacleCount].z      = z;
    obstacles[obstacleCount].radius = radius;
    obstacles[obstacleCount].active = active;
    obstacles[obstacleCount].rubble = rubble;
    return obstacleCount++;
}

glm::vec3 WorldShape::avoidRubble(const glm::vec3& position, const glm::vec3& desired,
                                  float radius, float lookAhead) const
{
    auto hitsRubble = [&](const glm::vec3& direction)
    {
        for (int i = 0; i < obstacleCount; ++i)
        {
            const Obstacle& o = obstacles[i];
            if (!o.active || !o.rubble) { continue; }

            // Closest point of the probe to the circle's centre.
            const float wx = o.x - position.x;
            const float wz = o.z - position.z;
            const float along = std::clamp(wx * direction.x + wz * direction.z,
                                           0.0f, lookAhead);
            const float cx = position.x + direction.x * along - o.x;
            const float cz = position.z + direction.z * along - o.z;
            const float reach = o.radius + radius;
            if (cx * cx + cz * cz < reach * reach) { return true; }
        }
        return false;
    };

    // Heading out of the walkable area is no way round either.
    auto leavesWorld = [&](const glm::vec3& direction)
    {
        const glm::vec3 end = position + direction * lookAhead;
        if (end.z > gateZ) { return std::fabs(end.x) > corridorHalfWidth - radius; }
        return end.x < roomMinX + radius || end.x > roomMaxX - radius;
    };

    // Nothing fallen in the way: go exactly where asked. Walls only matter
    // when choosing a way round, so without rubble this changes nothing.
    if (!hitsRubble(desired)) { return desired; }

    // Turn a little at a time, trying both ways at each step, and take the
    // first clear heading: the smallest detour that works.
    for (float degrees = 20.0f; degrees <= 140.0f; degrees += 20.0f)
    {
        for (float sign : { 1.0f, -1.0f })
        {
            const float a = glm::radians(degrees * sign);
            const glm::vec3 turned(desired.x * std::cos(a) + desired.z * std::sin(a),
                                   0.0f,
                                   -desired.x * std::sin(a) + desired.z * std::cos(a));
            if (!hitsRubble(turned) && !leavesWorld(turned)) { return turned; }
        }
    }
    return desired;
}

void WorldShape::setObstacleActive(int index, bool active)
{
    if (index >= 0 && index < obstacleCount)
    {
        obstacles[index].active = active;
    }
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
        if (!prop.active)
        {
            continue;
        }

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
        const float wallHalf = 0.3f;
        if (gardenOpen && position.z > gardenFrontZ - wallHalf)
        {
            // --- inside the garden (or in its doorway) ---
            clampX(-(gardenHalfWidth - wallHalf) + radius, (gardenHalfWidth - wallHalf) - radius);
            clampZ(gateZ, gardenBackZ - wallHalf - radius);

            // Its front wall, either side of the opening the corridor comes
            // through. Out there you are inside it - or behind it.
            const bool besideOpening = std::fabs(position.x) > corridorHalfWidth - radius;
            if (besideOpening && position.z < gardenFrontZ + wallHalf + radius)
            {
                position.z = gardenFrontZ + wallHalf + radius;
                velocity.z = std::max(0.0f, velocity.z);
            }
        }
        else
        {
            // --- inside the corridor ---
            clampX(-corridorHalfWidth + radius, corridorHalfWidth - radius);
            const float far = gardenOpen ? gardenFrontZ : std::min(exitZ + 4.0f, endZ - radius);
            clampZ(gateZ, far);
        }
    }
}
