#version 330 core

// Bloom, step two: a Gaussian blur along one direction. Run across, then
// down - a 2D blur for the cost of two thin ones.

in vec2 vUV;

uniform sampler2D uImage;
uniform vec2 uDirection;     // (spread, 0) across, (0, spread) down, in texels

out vec4 FragColor;

void main()
{
    const float weight[5] = float[5](0.2270270, 0.1945946, 0.1216216, 0.0540541, 0.0162162);
    vec2 step = uDirection / vec2(textureSize(uImage, 0));

    vec3 sum = texture(uImage, vUV).rgb * weight[0];
    for (int i = 1; i < 5; ++i)
    {
        sum += texture(uImage, vUV + step * float(i)).rgb * weight[i];
        sum += texture(uImage, vUV - step * float(i)).rgb * weight[i];
    }
    FragColor = vec4(sum, 1.0);
}
