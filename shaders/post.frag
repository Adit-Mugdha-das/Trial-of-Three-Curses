#version 330 core

// Operates on the finished image. During the curse this drains the colour
// out of the chamber and closes the corners in, so the whole frame turns to
// stone alongside the traveller - something no per-object shader can do,
// because it needs the composed picture.

in vec2 vUV;

uniform sampler2D uScene;

uniform float uDesaturate;   // 0 = full colour, 1 = fully grey
uniform float uVignette;     // 0 = none, 1 = heavy corner darkening
uniform vec3  uTint;
uniform float uTintAmount;

out vec4 FragColor;

void main()
{
    vec3 color = texture(uScene, vUV).rgb;

    // Rec. 709 luma weights, not a flat average: the eye is far more
    // sensitive to green than to blue, and averaging the channels makes
    // reds and blues collapse to the same muddy grey.
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));

    color = mix(color, vec3(luma), uDesaturate);

    // Tint against luma rather than the original colour, so the tint reads
    // as a light the scene is lit by rather than a filter laid over it.
    color = mix(color, luma * uTint, uTintAmount);

    // 0 at the centre, 1 at the corners.
    float radius = length(vUV - vec2(0.5)) * 1.41421356;
    float falloff = smoothstep(0.35, 1.0, radius);

    color *= 1.0 - uVignette * falloff;

    FragColor = vec4(color, 1.0);
}
