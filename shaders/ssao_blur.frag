#version 330 core

// Smooths the SSAO grain: averages the 4x4 tile the sample pattern was turned
// across, so the rotations cancel into a soft shade. Neighbours at a very
// different depth are left out, so the shade does not bleed across a
// silhouette onto whatever lies behind it.

in vec2 vUV;

uniform sampler2D uAO;
uniform sampler2D uDepth;
uniform float uNear;
uniform float uFar;

out vec4 FragColor;

float linearDepth(vec2 uv)
{
    float z = texture(uDepth, uv).r * 2.0 - 1.0;
    return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear));
}

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uAO, 0));
    float centre = linearDepth(vUV);

    float sum = 0.0;
    float weight = 0.0;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 uv = vUV + vec2(float(x), float(y)) * texel;
            float dz = abs(linearDepth(uv) - centre);
            float w = 1.0 / (1.0 + dz * 20.0 / (1.0 + 0.1 * centre));
            sum += texture(uAO, uv).r * w;
            weight += w;
        }
    }

    FragColor = vec4(vec3(sum / weight), 1.0);
}
