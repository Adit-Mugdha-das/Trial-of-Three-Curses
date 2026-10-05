#include "Light.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <glm/gtc/constants.hpp>

#include "Shader.h"

void uploadLights(const Shader& shader, const std::vector<Light>& lights)
{
    const int requested = static_cast<int>(lights.size());
    const int count = std::min(requested, kMaxLights);

    // Overflow silently drops whichever lights happen to be last in the list,
    // which shows up as a light mysteriously not working rather than as an
    // error. Say so, once.
    if (requested > kMaxLights)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            std::cout << "[Light] " << requested << " lights submitted but only "
                      << kMaxLights << " fit; the rest are dropped.\n";
        }
    }

    shader.setInt("uLightCount", count);

    for (int i = 0; i < count; ++i)
    {
        const Light& light = lights[i];
        const std::string base = "uLights[" + std::to_string(i) + "].";

        shader.setInt  (base + "type",      static_cast<int>(light.type));
        shader.setVec3 (base + "position",  light.position);
        shader.setVec3 (base + "direction", glm::normalize(light.direction));
        shader.setVec3 (base + "color",     light.color);
        shader.setFloat(base + "intensity", light.intensity);
        shader.setFloat(base + "constant",  light.constant);
        shader.setFloat(base + "linear",    light.linear);
        shader.setFloat(base + "quadratic", light.quadratic);

        // The shader compares cos(angle) against a dot product, so the
        // conversion happens once here rather than per fragment.
        shader.setFloat(base + "cutOff",
                        std::cos(glm::radians(light.innerAngle)));
        shader.setFloat(base + "outerCutOff",
                        std::cos(glm::radians(light.outerAngle)));
    }
}
