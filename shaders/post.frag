#version 330 core

// Operates on the finished image. During the curse this drains the colour
// out of the chamber and closes the corners in, so the whole frame turns to
// stone alongside the traveller - something no per-object shader can do,
// because it needs the composed picture.

in vec2 vUV;

uniform sampler2D uScene;
uniform sampler2D uAO;          // ambient occlusion, already blurred
uniform float uAOStrength;      // 0 = off
uniform sampler2D uBloom;       // the bright parts, blurred
uniform float uBloomStrength;   // 0 = off

uniform float uDesaturate;   // 0 = full colour, 1 = fully grey
uniform float uVignette;     // 0 = none, 1 = heavy corner darkening
uniform vec3  uTint;
uniform float uTintAmount;

out vec4 FragColor;

void main()
{
    vec3 color = texture(uScene, vUV).rgb;

    // Contact shading first: corners, creases and the ground under things.
    if (uAOStrength > 0.0)
    {
        color *= mix(1.0, texture(uAO, vUV).r, uAOStrength);
    }

    // Light spilling from the brightest things. Screen-blended rather than
    // added, so a glow over a bright wall cannot burn it out to white.
    if (uBloomStrength > 0.0)
    {
        vec3 glow = texture(uBloom, vUV).rgb * uBloomStrength;
        color = 1.0 - (1.0 - color) * (1.0 - glow);
    }

    // A gentle grade: a touch more contrast through the mid-tones, and a
    // little more colour - the flat look of raw shading, lifted.
    vec3 curved = color * color * (3.0 - 2.0 * color);
    color = mix(color, curved, 0.22);
    float grey = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(grey), color, 1.12);
    color = clamp(color, 0.0, 1.0);

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
