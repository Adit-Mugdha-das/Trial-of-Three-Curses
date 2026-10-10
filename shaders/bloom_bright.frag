#version 330 core

// Bloom, step one: keep only the bright parts of the picture. A soft knee
// rather than a hard cut, so a glow fades in instead of switching on.

in vec2 vUV;

uniform sampler2D uScene;

out vec4 FragColor;

void main()
{
    vec3 color = texture(uScene, vUV).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    float keep = smoothstep(0.86, 1.0, brightness);
    FragColor = vec4(color * keep, 1.0);
}
