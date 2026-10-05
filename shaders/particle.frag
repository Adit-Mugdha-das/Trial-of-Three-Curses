#version 330 core

in vec2 vUV;
in vec4 vColor;

out vec4 FragColor;

void main()
{
    // Radial falloff turns the square quad into a soft round mote. Squaring
    // it tightens the core and keeps the edges from looking like discs.
    float d = length(vUV - vec2(0.5)) * 2.0;
    float falloff = 1.0 - smoothstep(0.0, 1.0, d);
    falloff *= falloff;

    // Discarding fully transparent fragments early saves blending work on
    // the corners of every quad, which at this particle count is most of them.
    if (falloff < 0.004)
    {
        discard;
    }

    FragColor = vec4(vColor.rgb * falloff, vColor.a * falloff);
}
