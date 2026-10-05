#ifndef LIGHT_H
#define LIGHT_H

#include <vector>

#include <glm/glm.hpp>

class Shader;

// Must match the MAX_LIGHTS in phong.frag.
constexpr int kMaxLights = 12;

enum class LightType
{
    Directional = 0,   // infinitely far, no attenuation (the dim blue fill)
    Point       = 1,   // torches, the Djinn energy column
    Spot        = 2    // Medusa's eyes
};

struct Light
{
    LightType type = LightType::Point;

    glm::vec3 position { 0.0f, 0.0f, 0.0f };
    glm::vec3 direction{ 0.0f, -1.0f, 0.0f };   // directional and spot only
    glm::vec3 color    { 1.0f, 1.0f, 1.0f };

    float intensity = 1.0f;

    // Attenuation: 1 / (constant + linear*d + quadratic*d*d).
    // Larger linear/quadratic makes the falloff tighter and more dramatic,
    // which is what makes the Djinn light read as a local glow.
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;

    // Spot cone half-angles in degrees. Converted to cosines on upload,
    // because the shader compares against dot products.
    float innerAngle = 12.0f;
    float outerAngle = 18.0f;
};

// Uploads up to kMaxLights and sets uLightCount.
void uploadLights(const Shader& shader, const std::vector<Light>& lights);

#endif // LIGHT_H
