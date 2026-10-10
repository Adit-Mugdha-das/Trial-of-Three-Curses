#version 330 core

// Phase 2: full transform pipeline. The lighting varyings produced here are
// already the ones Phase 4's Blinn-Phong shader will consume, so this file
// should not need to change again.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// Inverse-transpose of the model matrix. Uploaded from the CPU rather than
// inverted per-vertex here, because inverse() in a shader is expensive.
// Without this, non-uniform scaling (the pulsing heart, the growing rings)
// would skew the normals and break lighting.
uniform mat3 uNormalMatrix;

// --- Gouraud shading --------------------------------------------------------
// The comparison mode for Blinn-Phong. The SAME lighting equation, but worked
// out here, once per vertex, instead of once per pixel in the fragment
// shader. The rasteriser then blends those vertex results across each
// triangle. Cheap, but anything smaller than a triangle - a torch's pool of
// light in the middle of a big wall, a highlight on a sphere - is lost or
// smeared. Declarations must match phong.frag exactly: they are one program.
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

uniform Light uLights[MAX_LIGHTS];
uniform int   uLightCount;
uniform vec3  uViewPos;
uniform int   uGouraud;             // 1 = light per vertex, 0 = per pixel (Blinn-Phong)
uniform float uVertexShininess;     // the material's shininess, for the vertex path

uniform sampler2D uShadowMap;
uniform mat4  uLightSpaceMatrix;
uniform int   uShadowLightIndex;
uniform int   uShadowsEnabled;

// The light arriving at this vertex, before the material's colours: the
// fragment shader multiplies these by kd and ks (which may come from a
// texture, so they have to be applied per pixel).
out vec3 vGouraudDiffuse;
out vec3 vGouraudSpecular;

// One shadow-map test for the whole vertex.
float vertexShadow(vec3 P, vec3 N, vec3 L)
{
    vec4 clip = uLightSpaceMatrix * vec4(P, 1.0);
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z > 1.0) { return 0.0; }
    float bias = max(0.0040 * (1.0 - dot(N, L)), 0.0009);
    float closest = textureLod(uShadowMap, p.xy, 0.0).r;
    return (p.z - bias > closest) ? 1.0 : 0.0;
}

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

// The other two axes of the tangent frame, in world space. Passed as separate
// vectors rather than a mat3 varying, which some drivers handle poorly.
out vec3 vTangent;
out vec3 vBitangent;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    vec3 N = normalize(uNormalMatrix * aNormal);
    vec3 T = normalize(uNormalMatrix * aTangent);

    // Re-orthogonalise: interpolating tangents across a triangle can tilt
    // them off the surface, and a skewed frame shears the mapped normals.
    T = normalize(T - N * dot(N, T));

    vWorldPos  = worldPos.xyz;
    vNormal    = N;
    vTangent   = T;
    vBitangent = cross(N, T);
    vUV        = aUV;

    vGouraudDiffuse  = vec3(0.0);
    vGouraudSpecular = vec3(0.0);
    if (uGouraud == 1)
    {
        vec3 P = worldPos.xyz;
        vec3 V = normalize(uViewPos - P);
        for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i)
        {
            Light light = uLights[i];
            vec3  L = vec3(0.0);
            float attenuation = 1.0;
            if (light.type == LIGHT_DIRECTIONAL)
            {
                L = normalize(-light.direction);
            }
            else
            {
                vec3  toLight = light.position - P;
                float dist    = length(toLight);
                L = normalize(toLight);
                attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);
                if (light.type == LIGHT_SPOT)
                {
                    float theta   = dot(L, normalize(-light.direction));
                    float epsilon = max(light.cutOff - light.outerCutOff, 1e-4);
                    attenuation  *= clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
                }
            }

            float NdotL = max(dot(N, L), 0.0);
            vec3  H     = normalize(L + V);
            float spec  = (NdotL > 0.0) ? pow(max(dot(N, H), 0.0), max(uVertexShininess, 1.0)) : 0.0;

            float lit = 1.0;
            if (uShadowsEnabled == 1 && i == uShadowLightIndex)
            {
                lit = 1.0 - vertexShadow(P, N, L);
            }

            float strength = lit * light.intensity * attenuation;
            vGouraudDiffuse  += light.color * NdotL * strength;
            vGouraudSpecular += light.color * spec  * strength;
        }
    }

    gl_Position = uProjection * uView * worldPos;
}
