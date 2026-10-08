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

    int   sky;        // the sky dome: a gradient by view direction
    int   rayTraced;  // the garden pool: ray-traced reflections
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

// The sky dome over the clouds: violet-blue overhead, rose at the horizon,
// gold toward the sun. Already in display colours - no lighting, no tone map.
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyGlow;
uniform vec3 uSkySunDir;

// --- ray tracing ----------------------------------------------------------------
// The garden pool reflects by real ray tracing: every water pixel fires a
// reflection ray against the garden itself - its walls, obelisks, statues,
// palms, dunes, the moon and the sun - sent up each frame as axis-aligned
// boxes and ellipsoids. The nearest hit is shaded with its own shadow ray.
// A second shadow ray from the water lets the obelisks' shadows fall on it.
#define RT_MAX_BOXES 48
#define RT_MAX_BLOBS 40
uniform int   uRtEnabled;
uniform float uRtTime;
uniform int   uRtBoxCount;
uniform vec3  uRtBoxMin[RT_MAX_BOXES];
uniform vec3  uRtBoxMax[RT_MAX_BOXES];
uniform vec3  uRtBoxKd[RT_MAX_BOXES];
uniform int   uRtBlobCount;
uniform vec3  uRtBlobCentre[RT_MAX_BLOBS];
uniform vec3  uRtBlobRadii[RT_MAX_BLOBS];
uniform vec3  uRtBlobKd[RT_MAX_BLOBS];
uniform vec3  uRtBlobGlow[RT_MAX_BLOBS];
uniform vec3  uRtLightPos;
uniform vec3  uRtLightColor;
uniform vec3  uRtSky;            // the sky's display colour, for rays that hit nothing

// Ray against an axis-aligned box (the slab method). Rays starting inside are ignored.
bool rtBox(vec3 o, vec3 d, vec3 bmin, vec3 bmax, out float t, out vec3 n)
{
    vec3 safe = mix(d, vec3(1e-6), lessThan(abs(d), vec3(1e-6)));
    vec3 inv = 1.0 / safe;
    vec3 t0 = (bmin - o) * inv;
    vec3 t1 = (bmax - o) * inv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tn = max(max(tmin.x, tmin.y), tmin.z);
    float tf = min(min(tmax.x, tmax.y), tmax.z);
    if (tf < 0.0 || tn > tf || tn < 0.001) { t = 0.0; n = vec3(0.0); return false; }
    t = tn;
    if (tn == tmin.x)      { n = vec3(-sign(d.x), 0.0, 0.0); }
    else if (tn == tmin.y) { n = vec3(0.0, -sign(d.y), 0.0); }
    else                   { n = vec3(0.0, 0.0, -sign(d.z)); }
    return true;
}

// Ray against an axis-aligned ellipsoid: squash space so it is a unit sphere.
bool rtBlob(vec3 o, vec3 d, vec3 c, vec3 r, out float t, out vec3 n)
{
    vec3 oc = (o - c) / r;
    vec3 dd = d / r;
    float a = dot(dd, dd);
    float b = dot(oc, dd);
    float cc = dot(oc, oc) - 1.0;
    float disc = b * b - a * cc;
    t = 0.0; n = vec3(0.0);
    if (disc < 0.0) { return false; }
    float tt = (-b - sqrt(disc)) / a;
    if (tt < 0.001) { return false; }
    t = tt;
    n = normalize((o + d * tt - c) / (r * r));
    return true;
}

// The nearest thing a ray hits. Shadow rays skip glowing things (the moon
// and the sun are lights, not blockers).
bool rtHit(vec3 o, vec3 d, float maxT, bool shadowRay,
           out float tBest, out vec3 nBest, out vec3 kd, out vec3 glow)
{
    tBest = maxT; nBest = vec3(0.0, 1.0, 0.0); kd = vec3(0.0); glow = vec3(0.0);
    bool hit = false;
    float t; vec3 n;
    for (int i = 0; i < RT_MAX_BOXES; ++i)
    {
        if (i >= uRtBoxCount) { break; }
        if (rtBox(o, d, uRtBoxMin[i], uRtBoxMax[i], t, n) && t < tBest)
        {
            tBest = t; nBest = n; kd = uRtBoxKd[i]; glow = vec3(0.0); hit = true;
            if (shadowRay) { return true; }
        }
    }
    for (int i = 0; i < RT_MAX_BLOBS; ++i)
    {
        if (i >= uRtBlobCount) { break; }
        if (shadowRay && dot(uRtBlobGlow[i], vec3(1.0)) > 0.6) { continue; }
        if (rtBlob(o, d, uRtBlobCentre[i], uRtBlobRadii[i], t, n) && t < tBest)
        {
            tBest = t; nBest = n; kd = uRtBlobKd[i]; glow = uRtBlobGlow[i]; hit = true;
            if (shadowRay) { return true; }
        }
    }
    return hit;
}

// From the display colour back to the linear light the tone map expects.
vec3 rtLinear(vec3 c)
{
    vec3 x = pow(clamp(c, 0.0, 0.95), vec3(1.6));
    return x / (vec3(1.0) - x);
}

// What a reflection ray sees: the lit, shadow-tested surface it hits, or the sky.
vec3 rtTrace(vec3 o, vec3 d)
{
    float t; vec3 n, kd, glow;
    if (!rtHit(o, d, 1.0e4, false, t, n, kd, glow))
    {
        return rtLinear(uRtSky * mix(1.15, 0.85, clamp(d.y, 0.0, 1.0)));
    }
    vec3  p = o + d * t;
    vec3  toLight = uRtLightPos - p;
    float dist = length(toLight);
    vec3  L = toLight / dist;
    float lit = max(dot(n, L), 0.0);
    float ts; vec3 ns, ks, gs;
    if (lit > 0.0 && rtHit(p + n * 0.02, L, dist, true, ts, ns, ks, gs)) { lit = 0.0; }
    // A little sky light as well, so the shaded side is not black.
    return kd * (uAmbient + vec3(0.18) + uRtLightColor * lit) + glow;
}
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
uniform vec3  uStoneEmissive;    // glow inside the converted part (gold, when he ascends)
uniform vec3  uPetrifyEdge;      // glow riding the front itself

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
    if (uMaterial.sky == 1)
    {
        vec3  dir = normalize(vWorldPos - uViewPos);
        float toSun = max(dot(dir, uSkySunDir), 0.0);
        vec3  horizon = mix(uSkyHorizon, uSkyGlow, pow(toSun, 3.0));
        vec3  col = mix(horizon, uSkyZenith, smoothstep(0.0, 0.6, dir.y));
        col += uSkyGlow * pow(toSun, 24.0) * 0.35;
        FragColor = vec4(col, 1.0);
        return;
    }

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
        emissive += uPetrifyEdge * edge + uStoneEmissive * stone;
    }

    // Ambient is applied once, not per light, so adding lights does not wash
    // the ambient term out.
    vec3 result = emissive + uAmbient * ka;

    for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i)
    {
        bool casts = (uShadowsEnabled == 1) && (i == uShadowLightIndex);
        result += shade(uLights[i], N, V, kd, ks, shininess, casts);
    }

    float alpha = uMaterial.opacity;
    if (uMaterial.rayTraced == 1 && uRtEnabled == 1)
    {
        // Gentle moving ripples bend each pixel's mirror a little.
        vec3 P = vWorldPos;
        vec3 Nw = normalize(vec3(
            0.012 * sin(P.x * 5.0 + uRtTime * 1.7) + 0.008 * sin((P.x + P.z) * 7.0 - uRtTime * 2.3),
            1.0,
            0.012 * cos(P.z * 5.5 + uRtTime * 1.4) + 0.008 * cos((P.x - P.z) * 6.0 + uRtTime * 2.0)));
        vec3 Vw = normalize(uViewPos - P);
        vec3 reflection = rtTrace(P + Nw * 0.02, reflect(-Vw, Nw));

        // Shadow ray from the water itself: what stands between it and the light.
        vec3 toLight = uRtLightPos - P;
        float ts; vec3 ns, ks, gs;
        float shade = rtHit(P + vec3(0.0, 0.02, 0.0), normalize(toLight), length(toLight), true, ts, ns, ks, gs)
                    ? 0.5 : 1.0;

        // Fresnel: a mirror at a glancing look, clearer looking straight down.
        float fresnel = 0.45 + 0.55 * pow(1.0 - max(dot(Nw, Vw), 0.0), 3.0);
        result = mix(result * shade, reflection * shade, fresnel);
        alpha = 0.97;
    }

    // Reinhard-style rolloff, so the bright torch cores clip gracefully to
    // white instead of banding.
    result = result / (result + vec3(1.0));

    // Back to display space.
    result = pow(result, vec3(1.0 / 1.6));

    FragColor = vec4(result, alpha);
}
