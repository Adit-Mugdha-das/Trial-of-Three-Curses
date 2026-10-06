#version 330 core

// Per-fragment Blinn-Phong over a mixed rig of directional, point and spot
// lights. Phase 5 adds texture maps; Phase 9 adds the petrification blend.

#define MAX_LIGHTS 12

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2

struct Light
{
    int   type;
    vec3  position;
    vec3  direction;
    vec3  color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    float cutOff;        // cosine of the inner cone angle
    float outerCutOff;   // cosine of the outer cone angle
};

struct Material
{
    vec3  ka;
    vec3  kd;
    vec3  ks;
    vec3  emissive;
    float shininess;

    // Diffuse map multiplies kd, specular map multiplies ks. The specular
    // map is what lets shininess vary across one surface.
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    int   useDiffuseMap;
    int   useSpecularMap;
    int   useNormalMap;
    vec2  uvScale;
    float opacity;

    // The garden's plants: what they look like dead.
    int   living;
    vec3  deadKa;
    vec3  deadKd;
    vec3  deadKs;
    vec3  deadEmissive;
};

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in vec3 vBitangent;

uniform Light uLights[MAX_LIGHTS];
uniform int   uLightCount;

uniform Material uMaterial;

// The wave of life spreading from the garden's altar. Radius <= 0: not begun.
uniform vec3  uLifeCentre;
uniform float uLifeRadius;

uniform vec3 uAmbient;     // global ambient, modulated by the material's ka
uniform vec3 uViewPos;
uniform int  uDebugNormals;
uniform int  uUseTextures;    // global off switch, for the T-less comparison
uniform int  uUseNormalMaps;  // separate switch, to isolate the relief

out vec4 FragColor;

// --- petrification -------------------------------------------------------
// The curse spreads UPWARD from the traveller's feet. Doing it per fragment
// against world height, rather than per node, means a single limb is part
// flesh and part stone while the front passes through it.
uniform int   uPetrifyEnabled;   // per node: only the traveller opts in
uniform float uPetrify;          // 0..1, how far the curse has climbed
uniform float uPetrifyBaseY;     // world Y of the traveller's feet
uniform float uPetrifyHeight;    // world height of the traveller

uniform vec3  uStoneKa;
uniform vec3  uStoneKd;
uniform vec3  uStoneKs;
uniform float uStoneShininess;

// --- shadow mapping ------------------------------------------------------
uniform sampler2D uShadowMap;
uniform mat4  uLightSpaceMatrix;
uniform int   uShadowLightIndex;   // which light in uLights casts; -1 = none
uniform int   uShadowsEnabled;

// Returns 1 where fully shadowed, 0 where fully lit.
float shadowFactor(vec3 N, vec3 L)
{
    vec4 lightClip = uLightSpaceMatrix * vec4(vWorldPos, 1.0);

    // Perspective divide by hand: this is not gl_Position, so nothing does it
    // for us. Then map the [-1,1] clip range onto the [0,1] texture range.
    vec3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5 + 0.5;

    // Past the light's far plane there is no information; treat as lit.
    if (proj.z > 1.0)
    {
        return 0.0;
    }

    // Slope-scaled bias. A surface edge-on to the light spans many depth
    // values within one texel, so it needs far more bias than one facing the
    // light square on. A single constant bias either leaves acne on the
    // slopes or makes flat surfaces float free of their own shadow.
    float bias = max(0.0040 * (1.0 - dot(N, L)), 0.0009);

    // 3x3 percentage-closer filtering: nine taps averaged, which trades the
    // hard stair-stepped edge of a single tap for a soft one.
    float shadow = 0.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closest = texture(uShadowMap, proj.xy + vec2(x, y) * texel).r;
            shadow += (proj.z - bias > closest) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

vec3 shade(Light light, vec3 N, vec3 V, vec3 kd, vec3 ks, float shininess,
           bool castsShadow)
{
    vec3  L = vec3(0.0);
    float attenuation = 1.0;

    if (light.type == LIGHT_DIRECTIONAL)
    {
        // No position and no falloff - the light is infinitely far away.
        L = normalize(-light.direction);
    }
    else
    {
        vec3  toLight = light.position - vWorldPos;
        float dist    = length(toLight);
        L = normalize(toLight);

        attenuation = 1.0 / (light.constant +
                             light.linear * dist +
                             light.quadratic * dist * dist);

        if (light.type == LIGHT_SPOT)
        {
            // Soft-edged cone: full brightness inside cutOff, fading to zero
            // at outerCutOff.
            float theta   = dot(L, normalize(-light.direction));
            float epsilon = max(light.cutOff - light.outerCutOff, 1e-4);
            attenuation  *= clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
        }
    }

    float NdotL = max(dot(N, L), 0.0);

    // Blinn-Phong: the half-vector between light and view, rather than
    // Phong's reflected vector. Cheaper, and the highlight holds its shape at
    // grazing angles instead of cutting off.
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(shininess, 1.0));

    vec3 diffuse  = kd * NdotL * light.color;

    // A surface facing away from the light must not show a highlight, or
    // shapes glow along their dark edge.
    vec3 specular = (NdotL > 0.0)
                  ? ks * spec * light.color
                  : vec3(0.0);

    // Only the direct terms are shadowed. Ambient is handled once in main()
    // and deliberately left alone - killing it too would make shadowed areas
    // pure black instead of dimly lit.
    float lit = 1.0;
    if (castsShadow)
    {
        lit = 1.0 - shadowFactor(N, L);
    }

    return (diffuse + specular) * lit * light.intensity * attenuation;
}

void main()
{
    vec3 N = normalize(vNormal);

    // Tiling happens here rather than in the mesh UVs, so one unit cube can
    // be a 6x6-tiled wall and an untiled prop depending only on its material.
    // Needed before the normal map is sampled.
    vec2 uv = vUV * uMaterial.uvScale;

    if (uUseTextures == 1 && uUseNormalMaps == 1 && uMaterial.useNormalMap == 1)
    {
        // Undo the [0,1] storage range back to a signed direction.
        vec3 sampled = texture(uMaterial.normalMap, uv).rgb * 2.0 - 1.0;

        // Tangent space to world space. The map's values are relative to the
        // surface, which is what lets one map work on any geometry at any
        // orientation - the whole point of tangent-space normal mapping.
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), N);
        N = normalize(TBN * sampled);
    }

    // Deliberately after the perturbation, so the debug view shows the
    // normals actually used for lighting rather than the geometric ones.
    if (uDebugNormals == 1)
    {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }

    vec3 V = normalize(uViewPos - vWorldPos);

    vec3 kd = uMaterial.kd;
    vec3 ks = uMaterial.ks;
    vec3 ka = uMaterial.ka;
    vec3 lifeGlow = vec3(0.0);
    float life = 1.0;

    if (uMaterial.living == 1)
    {
        // Each pixel decides for itself whether the wave has reached it, so
        // the colour spreads across one big lawn as a smooth circle.
        life = 0.0;
        if (uLifeRadius > 0.0)
        {
            float d = length(vWorldPos.xz - uLifeCentre.xz);
            life = clamp((uLifeRadius - d) / 1.2, 0.0, 1.0);
        }
        ka = mix(uMaterial.deadKa, ka, life);
        kd = mix(uMaterial.deadKd, kd, life);
        ks = mix(uMaterial.deadKs, ks, life);

        // The wave's leading edge glows gold as it passes.
        float edge = life * (1.0 - life) * 4.0;
        lifeGlow = vec3(1.0, 0.82, 0.40) * 1.1 * edge;
    }

    if (uUseTextures == 1)
    {
        if (uMaterial.useDiffuseMap == 1)
        {
            vec3 texel = texture(uMaterial.diffuseMap, uv).rgb;
            kd *= texel;

            // Ambient tracks the diffuse map too, otherwise unlit areas show
            // a flat silhouette with none of the surface detail.
            ka *= texel;
        }

        if (uMaterial.useSpecularMap == 1)
        {
            ks *= texture(uMaterial.specularMap, uv).rgb;
        }
    }

    float shininess = uMaterial.shininess;
    vec3  emissive  = uMaterial.emissive;
    if (uMaterial.living == 1)
    {
        emissive = mix(uMaterial.deadEmissive, emissive, life) + lifeGlow;
    }

    if (uPetrifyEnabled == 1 && uPetrify > 0.0)
    {
        // Where the curse has reached, in world Y.
        float front = uPetrifyBaseY + uPetrify * uPetrifyHeight;

        // A narrow transition band rather than a hard line, so the boundary
        // reads as spreading rather than as a cut.
        float stone = clamp((front - vWorldPos.y + 0.15) / 0.30, 0.0, 1.0);

        // Every coefficient crosses over, not just the colour: this is the
        // moderately-shiny-to-rough-stone transition the brief asks for.
        ka        = mix(ka, uStoneKa, stone);
        kd        = mix(kd, uStoneKd, stone);
        ks        = mix(ks, uStoneKs, stone);
        shininess = mix(shininess, uStoneShininess, stone);

        // A faint green glow riding the leading edge, so the front itself is
        // visible as it climbs. Peaks mid-transition and vanishes either side.
        float edge = stone * (1.0 - stone) * 4.0;
        emissive += vec3(0.10, 0.32, 0.12) * edge;
    }

    // Ambient is applied once, not per light, so adding lights does not wash
    // the ambient term out.
    vec3 result = emissive + uAmbient * ka;

    for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i)
    {
        bool casts = (uShadowsEnabled == 1) && (i == uShadowLightIndex);
        result += shade(uLights[i], N, V, kd, ks, shininess, casts);
    }

    // Reinhard-style rolloff, so the bright torch cores clip gracefully to
    // white instead of banding.
    result = result / (result + vec3(1.0));

    // Back to display space.
    result = pow(result, vec3(1.0 / 1.6));

    FragColor = vec4(result, uMaterial.opacity);
}
