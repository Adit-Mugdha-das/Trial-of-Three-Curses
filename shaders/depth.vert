#version 330 core

// Depth-only pass, rendered from the light. Position is all that matters -
// normals, UVs and materials are irrelevant to how far away a surface is.

layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
