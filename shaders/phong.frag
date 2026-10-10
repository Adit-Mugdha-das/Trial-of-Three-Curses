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

// Gouraud mode: the lighting was already worked out per vertex (phong.vert)
// and arrives here blended across the triangle.
uniform int  uGouraud;
in vec3 vGouraudDiffuse;
in vec3 vGouraudSpecular;

// The sky dome over the clouds: violet-blue overhead, rose at the horizon,
// gold toward the sun. Already in display colours - no lighting, no tone map.
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyGlow;
uniform vec3 uSkySunDir;

// --- ray tracing ----------------------------------------------------------------
// The garden pool reflects by real ray tracing: every water pixel fires a
// reflection ray into the garden. Each shape is sent up exactly as it is
// drawn - a unit cube, sphere, cylinder, cone or ring - with the inverse of
// its world matrix, so the ray is intersected in the shape's own space and
// every rotation and squash comes for free. The nearest hit is lit, given a
// highlight and tested with its own shadow ray. The sun glints in the water.
#define RT_MAX      56
#define RT_BOX      0
#define RT_SPHERE   1
#define RT_CYLINDER 2
#define RT_CONE     3
#define RT_TORUS    4
uniform int   uRtEnabled;
uniform float uRtTime;
uniform int   uRtCount;
uniform mat4  uRtInverse[RT_MAX];   // world -> the shape's unit space
uniform vec4  uRtKd[RT_MAX];        // its colour; the shape type in w
uniform vec4  uRtGlow[RT_MAX];      // its glow; a ring's tube radius in w
uniform vec3  uRtLightPos;
uniform vec3  uRtLightColor;
uniform vec3  uRtSunPos;            // the sun's disc, for its glint in the water
uniform vec3  uRtSky;               // the sky's display colour, for rays that hit nothing

// Every test takes the ray o + t*d already in the shape's unit space. The map
// is linear, so t there is the same t as in the world. Each returns the
// surface normal in that unit space.

// The unit cube, -0.5..0.5 (the slab method).
bool rtUnitBox(vec3 o, vec3 d, out float t, out vec3 n)
{
    vec3 safe = mix(d, vec3(1e-6), lessThan(abs(d), vec3(1e-6)));
    vec3 inv = 1.0 / safe;
    vec3 t0 = (vec3(-0.5) - o) * inv;
    vec3 t1 = (vec3( 0.5) - o) * inv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tn = max(max(tmin.x, tmin.y), tmin.z);
    float tf = min(min(tmax.x, tmax.y), tmax.z);
    t = tn; n = vec3(0.0);
    if (tf < 0.0 || tn > tf || tn < 0.001) { return false; }
    if (tn == tmin.x)      { n = vec3(-sign(d.x), 0.0, 0.0); }
    else if (tn == tmin.y) { n = vec3(0.0, -sign(d.y), 0.0); }
    else                   { n = vec3(0.0, 0.0, -sign(d.z)); }
    return true;
}

// The unit sphere, radius 0.5.
bool rtUnitSphere(vec3 o, vec3 d, out float t, out vec3 n)
{
    float a = dot(d, d);
    float b = dot(o, d);
    float c = dot(o, o) - 0.25;
    float disc = b * b - a * c;
    t = 0.0; n = vec3(0.0);
    if (disc < 0.0) { return false; }
    t = (-b - sqrt(disc)) / a;
    if (t < 0.001) { return false; }
    n = o + d * t;
    return true;
}

// The unit cylinder: upright, radius 0.5, y from -0.5 to 0.5, capped.
bool rtUnitCylinder(vec3 o, vec3 d, out float t, out vec3 n)
{
    t = 1e9; n = vec3(0.0);
    float a = d.x * d.x + d.z * d.z;
    if (a > 1e-9)
    {
        float b = o.x * d.x + o.z * d.z;
        float c = o.x * o.x + o.z * o.z - 0.25;
        float disc = b * b - a * c;
        if (disc >= 0.0)
        {
            float ts = (-b - sqrt(disc)) / a;
            float y = o.y + d.y * ts;
            if (ts > 0.001 && abs(y) <= 0.5) { t = ts; n = vec3(o.x + d.x * ts, 0.0, o.z + d.z * ts); }
        }
    }
    if (abs(d.y) > 1e-9)
    {
        for (int k = 0; k < 2; ++k)
        {
            float cy = (k == 0) ? 0.5 : -0.5;
            float tc = (cy - o.y) / d.y;
            vec2 q = o.xz + d.xz * tc;
            if (tc > 0.001 && tc < t && dot(q, q) <= 0.25) { t = tc; n = vec3(0.0, cy, 0.0); }
        }
    }
    return t < 1e8;
}

// The unit cone: apex at y = 0.5, a base of radius 0.5 at y = -0.5.
bool rtUnitCone(vec3 o, vec3 d, out float t, out vec3 n)
{
    t = 1e9; n = vec3(0.0);
    // Its radius at height y is 0.25 - 0.5y; along the ray that is k0 + k1 t.
    float k0 = 0.25 - 0.5 * o.y;
    float k1 = -0.5 * d.y;
    float a = d.x * d.x + d.z * d.z - k1 * k1;
    float b = o.x * d.x + o.z * d.z - k0 * k1;
    float c = o.x * o.x + o.z * o.z - k0 * k0;
    if (abs(a) > 1e-9)
    {
        float disc = b * b - a * c;
        if (disc >= 0.0)
        {
            float s = sqrt(disc);
            for (int k = 0; k < 2; ++k)
            {
                float tc = (k == 0) ? (-b - s) / a : (-b + s) / a;
                vec3 p = o + d * tc;
                if (tc > 0.001 && tc < t && p.y >= -0.5 && p.y <= 0.5)
                {
                    t = tc;
                    n = vec3(2.0 * p.x, 0.25 - 0.5 * p.y, 2.0 * p.z);
                }
            }
        }
    }
    if (abs(d.y) > 1e-9)
    {
        float tc = (-0.5 - o.y) / d.y;
        vec2 q = o.xz + d.xz * tc;
        if (tc > 0.001 && tc < t && dot(q, q) <= 0.25) { t = tc; n = vec3(0.0, -1.0, 0.0); }
    }
    return t < 1e8;
}

// The unit ring: a circle of radius 0.5 in XZ, with a tube of radius r.
// No neat formula - a ring is a quartic - so it is sphere-traced: step along
// the ray by the distance to the ring, inside its bounding box.
bool rtUnitTorus(vec3 o, vec3 d, float r, out float t, out vec3 n)
{
    t = 0.0; n = vec3(0.0);
    vec3 ext = vec3(0.5 + r, r, 0.5 + r);
    vec3 safe = mix(d, vec3(1e-6), lessThan(abs(d), vec3(1e-6)));
    vec3 inv = 1.0 / safe;
    vec3 t0 = (-ext - o) * inv;
    vec3 t1 = ( ext - o) * inv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tn = max(max(tmin.x, tmin.y), tmin.z);
    float tf = min(min(tmax.x, tmax.y), tmax.z);
    if (tf < 0.0 || tn > tf) { return false; }
    float len = length(d);
    float tt = max(tn, 0.001);
    for (int i = 0; i < 96; ++i)
    {
        vec3 p = o + d * tt;
        vec2 q = vec2(length(p.xz) - 0.5, p.y);
        float dist = length(q) - r;
        if (dist < 0.0005)
        {
            vec2 core = (length(p.xz) > 1e-6) ? normalize(p.xz) * 0.5 : vec2(0.5, 0.0);
            n = p - vec3(core.x, 0.0, core.y);
            t = tt;
            return true;
        }
        tt += dist / len;
        if (tt > tf) { break; }
    }
    return false;
}

// The nearest thing a ray hits. Shadow rays only need to know whether
// anything is in the way, and skip glowing things (the moon and the sun are
// lights, not blockers).
bool rtHit(vec3 o, vec3 d, float maxT, bool shadowRay,
           out float tBest, out vec3 nBest, out vec3 kd, out vec3 glow)
{
    tBest = maxT; nBest = vec3(0.0, 1.0, 0.0); kd = vec3(0.0); glow = vec3(0.0);
    bool hit = false;
    for (int i = 0; i < RT_MAX; ++i)
    {
        if (i >= uRtCount) { break; }
        if (shadowRay && dot(uRtGlow[i].rgb, vec3(1.0)) > 0.6) { continue; }

        mat4 inv = uRtInverse[i];
        vec3 lo = (inv * vec4(o, 1.0)).xyz;
        vec3 ld = (inv * vec4(d, 0.0)).xyz;
        int type = int(uRtKd[i].w + 0.5);

        float t; vec3 n; bool h;
        if (type == RT_BOX)           { h = rtUnitBox(lo, ld, t, n); }
        else if (type == RT_SPHERE)   { h = rtUnitSphere(lo, ld, t, n); }
        else if (type == RT_CYLINDER) { h = rtUnitCylinder(lo, ld, t, n); }
        else if (type == RT_CONE)     { h = rtUnitCone(lo, ld, t, n); }
        else                          { h = rtUnitTorus(lo, ld, uRtGlow[i].w, t, n); }

        if (h && t < tBest)
        {
            if (shadowRay) { return true; }
            tBest = t;
            hit = true;
            // Normals go back to the world by the inverse transpose - which,
            // given the inverse already, is just its transpose.
            nBest = normalize(transpose(mat3(inv)) * n);
            kd = uRtKd[i].rgb;
            glow = uRtGlow[i].rgb;
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

// What a reflection ray sees: the lit, shadow-tested surface it hits, or the
// sky with the sun's glint in it.
vec3 rtTrace(vec3 o, vec3 d)
{
    float t; vec3 n, kd, glow;
    if (!rtHit(o, d, 1.0e4, false, t, n, kd, glow))
    {
        vec3 sky = rtLinear(uRtSky * mix(1.15, 0.85, clamp(d.y, 0.0, 1.0)));
        float sun = max(dot(d, normalize(uRtSunPos - o)), 0.0);
        return sky + uRtLightColor * (pow(sun, 600.0) * 8.0 + pow(sun, 40.0) * 0.6);
    }
    if (dot(n, d) > 0.0) { n = -n; }
    vec3  p = o + d * t;
    vec3  toLight = uRtLightPos - p;
    float dist = length(toLight);
    vec3  L = toLight / dist;
    float lit = max(dot(n, L), 0.0);
    float ts; vec3 ns, ks, gs;
    if (lit > 0.0 && rtHit(p + n * 0.02, L, dist, true, ts, ns, ks, gs)) { lit = 0.0; }

    // A highlight, so the gold and the stone catch the light in the mirror too.
    vec3  H = normalize(L - d);
    float spec = (lit > 0.0) ? pow(max(dot(n, H), 0.0), 40.0) : 0.0;

    // A little sky light as well, so the shaded side is not black.
    return kd * (uAmbient + vec3(0.18) + uRtLightColor * lit) + uRtLightColor * spec * 0.5 + glow;
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

    // Percentage-closer filtering over a 16-tap Poisson disc, turned by a
    // different angle on every pixel. A square grid of taps shows its own
    // banding in the penumbra; scattered, rotated taps blend into a smooth,
    // soft edge instead.
    const vec2 kPoisson[16] = vec2[16](
        vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
        vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
        vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
        vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
        vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
        vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790));

    float angle = 6.2831853 * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    mat2 spin = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    const float kSpread = 2.4;      // in shadow-map texels

    float shadow = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        float closest = texture(uShadowMap, proj.xy + spin * kPoisson[i] * texel * kSpread).r;
        shadow += (proj.z - bias > closest) ? 1.0 : 0.0;
    }

    return shadow / 16.0;
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

    // Normal mapping needs a normal per pixel; Gouraud only has one per vertex.
    if (uGouraud == 0 && uUseTextures == 1 && uUseNormalMaps == 1 && uMaterial.useNormalMap == 1)
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

    if (uGouraud == 1)
    {
        // Gouraud: the light was computed at the vertices; only the material
        // colours (and their textures) are applied here, per pixel.
        result += kd * vGouraudDiffuse + ks * vGouraudSpecular;
    }
    else
    {
        // Blinn-Phong: the whole lighting equation, for every pixel.
        for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i)
        {
            bool casts = (uShadowsEnabled == 1) && (i == uShadowLightIndex);
            result += shade(uLights[i], N, V, kd, ks, shininess, casts);
        }
    }

    float alpha = uMaterial.opacity;
    if (uMaterial.rayTraced == 1 && uRtEnabled == 1)
    {
        // Gentle moving ripples bend each pixel's mirror a little.
        vec3 P = vWorldPos;
        vec3 Nw = normalize(vec3(
            0.006 * sin(P.x * 5.0 + uRtTime * 1.7) + 0.004 * sin((P.x + P.z) * 7.0 - uRtTime * 2.3),
            1.0,
            0.006 * cos(P.z * 5.5 + uRtTime * 1.4) + 0.004 * cos((P.x - P.z) * 6.0 + uRtTime * 2.0)));
        vec3 Vw = normalize(uViewPos - P);
        vec3 reflection = rtTrace(P + Nw * 0.02, reflect(-Vw, Nw));

        // Shadow ray from the water itself: what stands between it and the light.
        vec3 toLight = uRtLightPos - P;
        float ts; vec3 ns, ks, gs;
        float shade = rtHit(P + vec3(0.0, 0.02, 0.0), normalize(toLight), length(toLight), true, ts, ns, ks, gs)
                    ? 0.5 : 1.0;

        // Fresnel: a mirror at a glancing look, clearer looking straight down.
        // Still water is mostly mirror, with a faint cool tint of its own.
        float fresnel = 0.65 + 0.35 * pow(1.0 - max(dot(Nw, Vw), 0.0), 3.0);
        result = mix(result * shade, reflection * vec3(0.92, 0.97, 1.0) * shade, fresnel);
        alpha = 0.97;
    }

    // Reinhard-style rolloff, so the bright torch cores clip gracefully to
    // white instead of banding.
    result = result / (result + vec3(1.0));

    // Back to display space.
    result = pow(result, vec3(1.0 / 1.6));

    FragColor = vec4(result, alpha);
}
