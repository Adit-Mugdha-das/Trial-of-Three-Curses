#include "Material.h"

#include "Shader.h"
#include "Texture.h"

void Material::upload(const Shader& shader) const
{
    shader.setVec3("uMaterial.ka", ka);
    shader.setVec3("uMaterial.kd", kd);
    shader.setVec3("uMaterial.ks", ks);
    shader.setVec3("uMaterial.emissive", emissive);
    shader.setFloat("uMaterial.shininess", shininess);
    shader.setFloat("uVertexShininess", shininess);     // the same value, for Gouraud's vertex path
    shader.setVec2("uMaterial.uvScale", uvScale);
    shader.setFloat("uMaterial.opacity", opacity);

    shader.setInt("uMaterial.living", living ? 1 : 0);
    shader.setInt("uMaterial.sky", sky ? 1 : 0);
    shader.setInt("uMaterial.rayTraced", rayTraced ? 1 : 0);
    if (living)
    {
        shader.setVec3("uMaterial.deadKa", deadKa);
        shader.setVec3("uMaterial.deadKd", deadKd);
        shader.setVec3("uMaterial.deadKs", deadKs);
        shader.setVec3("uMaterial.deadEmissive", deadEmissive);
    }

    // Fixed texture units: 0 diffuse, 1 specular, 2 shadow map (bound once
    // per frame by main), 3 normal.
    shader.setInt("uMaterial.diffuseMap", 0);
    shader.setInt("uMaterial.specularMap", 1);
    shader.setInt("uMaterial.normalMap", 3);

    shader.setInt("uMaterial.useDiffuseMap", diffuseMap != nullptr ? 1 : 0);
    shader.setInt("uMaterial.useSpecularMap", specularMap != nullptr ? 1 : 0);
    shader.setInt("uMaterial.useNormalMap", normalMap != nullptr ? 1 : 0);

    (diffuseMap  != nullptr ? *diffuseMap  : Texture::white()).bind(0);
    (specularMap != nullptr ? *specularMap : Texture::white()).bind(1);
    (normalMap   != nullptr ? *normalMap   : Texture::white()).bind(3);
}

namespace Materials
{

// Highly specular, tight highlight: reads as polished metal.
const Material gold {
    { 0.24f, 0.20f, 0.07f },
    { 0.75f, 0.61f, 0.23f },
    { 0.63f, 0.56f, 0.37f },
    { 0.0f, 0.0f, 0.0f },
    51.2f
};

const Material brass {
    { 0.33f, 0.22f, 0.03f },
    { 0.78f, 0.57f, 0.11f },
    { 0.99f, 0.94f, 0.81f },
    { 0.0f, 0.0f, 0.0f },
    27.9f
};

// Near-zero ks and a very low exponent: no highlight at all, just form.
const Material stone {
    { 0.10f, 0.10f, 0.10f },
    { 0.42f, 0.42f, 0.44f },
    { 0.06f, 0.06f, 0.06f },
    { 0.0f, 0.0f, 0.0f },
    4.0f
};

const Material darkStone {
    { 0.06f, 0.06f, 0.07f },
    { 0.26f, 0.26f, 0.29f },
    { 0.05f, 0.05f, 0.05f },
    { 0.0f, 0.0f, 0.0f },
    4.0f
};

const Material sandstone {
    { 0.12f, 0.10f, 0.08f },
    { 0.55f, 0.45f, 0.33f },
    { 0.04f, 0.04f, 0.04f },
    { 0.0f, 0.0f, 0.0f },
    2.0f
};

const Material floorStone {
    { 0.10f, 0.08f, 0.06f },
    { 0.40f, 0.33f, 0.25f },
    { 0.05f, 0.05f, 0.05f },
    { 0.0f, 0.0f, 0.0f },
    3.0f
};

const Material snakeScale {
    { 0.05f, 0.10f, 0.05f },
    { 0.20f, 0.45f, 0.22f },
    { 0.35f, 0.45f, 0.35f },
    { 0.0f, 0.0f, 0.0f },
    24.0f
};

const Material flesh {
    { 0.15f, 0.12f, 0.11f },
    { 0.66f, 0.52f, 0.45f },
    { 0.30f, 0.30f, 0.30f },
    { 0.0f, 0.0f, 0.0f },
    22.0f
};

// The Phase 9 destination: rougher, duller, colder than flesh.
const Material petrified {
    { 0.10f, 0.10f, 0.10f },
    { 0.44f, 0.44f, 0.46f },
    { 0.05f, 0.05f, 0.05f },
    { 0.0f, 0.0f, 0.0f },
    3.0f
};

const Material cloth {
    { 0.09f, 0.06f, 0.11f },
    { 0.42f, 0.30f, 0.52f },
    { 0.08f, 0.08f, 0.10f },
    { 0.0f, 0.0f, 0.0f },
    8.0f
};

const Material feather {
    { 0.18f, 0.18f, 0.16f },
    { 0.90f, 0.90f, 0.84f },
    { 0.22f, 0.22f, 0.22f },
    { 0.0f, 0.0f, 0.0f },
    14.0f
};

const Material heart {
    { 0.22f, 0.04f, 0.05f },
    { 0.72f, 0.13f, 0.17f },
    { 0.55f, 0.40f, 0.40f },
    { 0.28f, 0.02f, 0.04f },
    30.0f
};

const Material djinnEnergy {
    { 0.05f, 0.18f, 0.20f },
    { 0.15f, 0.55f, 0.62f },
    { 0.40f, 0.60f, 0.65f },
    { 0.18f, 0.70f, 0.80f },
    40.0f
};

const Material flame {
    { 0.30f, 0.16f, 0.05f },
    { 0.90f, 0.50f, 0.15f },
    { 0.20f, 0.15f, 0.10f },
    { 0.95f, 0.55f, 0.16f },
    8.0f
};

const Material eye {
    { 0.20f, 0.25f, 0.10f },
    { 0.60f, 0.75f, 0.30f },
    { 0.60f, 0.60f, 0.40f },
    { 0.45f, 0.70f, 0.22f },
    36.0f
};

// Gold, but lit from within: the one thing in the chamber worth the risk.
const Material treasure {
    { 0.38f, 0.30f, 0.10f },
    { 0.95f, 0.78f, 0.28f },
    { 1.00f, 0.92f, 0.62f },
    { 0.55f, 0.40f, 0.10f },
    78.0f
};

Material lerp(const Material& a, const Material& b, float t)
{
    Material m;
    m.ka        = glm::mix(a.ka, b.ka, t);
    m.kd        = glm::mix(a.kd, b.kd, t);
    m.ks        = glm::mix(a.ks, b.ks, t);
    m.emissive  = glm::mix(a.emissive, b.emissive, t);
    m.shininess = glm::mix(a.shininess, b.shininess, t);
    return m;
}

} // namespace Materials
