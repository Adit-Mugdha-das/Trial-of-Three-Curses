#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>

class Shader;
class Texture;

// Blinn-Phong surface coefficients.
//
//   ka - ambient reflectance, what the surface shows with no light on it
//   kd - diffuse reflectance, the "colour" in the everyday sense
//   ks - specular reflectance, how bright the highlight is
//   ns - shininess exponent, how *tight* the highlight is
//
// The contrast that sells the lighting is gold (ks 0.63, ns 51) against stone
// (ks 0.06, ns 4) under the same torch.
struct Material
{
    glm::vec3 ka{ 0.10f, 0.10f, 0.10f };
    glm::vec3 kd{ 0.70f, 0.70f, 0.70f };
    glm::vec3 ks{ 0.20f, 0.20f, 0.20f };

    // Added straight to the result, unlit. Flames, the Djinn energy and
    // Medusa's eyes glow rather than being shaded.
    glm::vec3 emissive{ 0.0f, 0.0f, 0.0f };

    float shininess = 16.0f;

    // Optional maps. Not owned - the Scene owns the textures and outlives
    // every material that points at one.
    //
    // The diffuse map multiplies kd, the specular map multiplies ks. That
    // means shininess can vary across a single surface: bare brass shines,
    // the verdigris patch beside it does not.
    const Texture* diffuseMap  = nullptr;
    const Texture* specularMap = nullptr;

    // Perturbs the surface normal per fragment, so flat geometry catches
    // light as though it had relief.
    const Texture* normalMap = nullptr;

    // Texture repeats across the surface. Walls and floor tile; props do not.
    glm::vec2 uvScale{ 1.0f, 1.0f };

    // Below 1 the node is routed into the sorted transparent pass instead of
    // the opaque one. See Scene::draw().
    float opacity = 1.0f;

    bool isTransparent() const { return opacity < 0.999f; }

    void upload(const Shader& shader) const;
};

// The table from PROJECT_PLAN.md section 5.
namespace Materials
{
    extern const Material gold;
    extern const Material brass;
    extern const Material stone;
    extern const Material darkStone;
    extern const Material sandstone;
    extern const Material floorStone;
    extern const Material snakeScale;
    extern const Material flesh;
    extern const Material petrified;
    extern const Material cloth;
    extern const Material feather;
    extern const Material heart;
    extern const Material djinnEnergy;
    extern const Material flame;
    extern const Material eye;
    extern const Material treasure;

    // Used by Phase 9 to melt the traveller from flesh into stone.
    Material lerp(const Material& a, const Material& b, float t);
}

#endif // MATERIAL_H
