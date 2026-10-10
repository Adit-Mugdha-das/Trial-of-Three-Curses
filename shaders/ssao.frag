#version 330 core

// Screen-space ambient occlusion, from the depth buffer alone.
//
// Each pixel is turned back into a point in view space. Sixteen sample points
// are scattered in a small hemisphere above its surface; each one is projected
// back onto the screen, and if the scene there is in front of the sample, the
// sample is buried - something is crowding this point. The fraction buried is
// how much sky the point cannot see: corners, creases and the floor beneath
// an object come out dark, open surfaces stay light.

in vec2 vUV;

uniform sampler2D uDepth;
uniform mat4  uProjection;
uniform mat4  uInvProjection;
uniform vec3  uKernel[16];
uniform float uRadius;      // how far round a point to look, in world units

out vec4 FragColor;

vec3 viewPosition(vec2 uv)
{
    float d = texture(uDepth, uv).r;
    vec4 ndc = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 v = uInvProjection * ndc;
    return v.xyz / v.w;
}

void main()
{
    float depth = texture(uDepth, vUV).r;
    if (depth >= 0.99999)
    {
        FragColor = vec4(1.0);      // nothing drawn here: open sky
        return;
    }

    vec3 P = viewPosition(vUV);

    // The surface normal, rebuilt from the neighbours. On each axis the one
    // whose depth is closer is used, so a silhouette edge does not bend it.
    vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
    vec3 right = viewPosition(vUV + vec2(texel.x, 0.0));
    vec3 left  = viewPosition(vUV - vec2(texel.x, 0.0));
    vec3 up    = viewPosition(vUV + vec2(0.0, texel.y));
    vec3 down  = viewPosition(vUV - vec2(0.0, texel.y));
    vec3 dx = (abs(right.z - P.z) < abs(P.z - left.z)) ? right - P : P - left;
    vec3 dy = (abs(up.z - P.z)    < abs(P.z - down.z)) ? up - P    : P - down;
    vec3 N = normalize(cross(dx, dy));

    // Turn the sample pattern differently on each pixel of a 4x4 tile, so
    // sixteen samples act like many more; the blur pass averages exactly
    // that tile back out.
    ivec2 cell = ivec2(gl_FragCoord.xy) & 3;
    float angle = fract(float(cell.x + cell.y * 4) * 0.618034) * 6.2831853;
    vec3 spin = vec3(cos(angle), sin(angle), 0.0);
    vec3 T = normalize(spin - N * dot(spin, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occlusion = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        vec3 s = P + TBN * uKernel[i] * uRadius;

        vec4 clip = uProjection * vec4(s, 1.0);
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { continue; }

        float sceneZ = viewPosition(uv).z;

        // Only nearby geometry counts: a wall far behind the sample is not
        // crowding this point, it is just further along the view ray.
        float near = smoothstep(0.0, 1.0, uRadius / abs(P.z - sceneZ));
        occlusion += ((sceneZ >= s.z + 0.03) ? 1.0 : 0.0) * near;
    }

    float ao = 1.0 - occlusion / 16.0;

    // Fade it out with distance: far away, a few pixels cover whole objects
    // and the estimate turns to noise.
    ao = mix(ao, 1.0, smoothstep(30.0, 60.0, -P.z));

    FragColor = vec4(vec3(pow(ao, 2.2)), 1.0);
}
