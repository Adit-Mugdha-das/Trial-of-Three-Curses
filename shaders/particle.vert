#version 330 core

// Instanced billboards. Attribute 0 advances per vertex; 1, 2 and 3 advance
// per instance (set with glVertexAttribDivisor), so one four-vertex quad is
// reused for every particle in a single draw call.

layout (location = 0) in vec2 aCorner;   // per vertex:  -0.5 .. 0.5
layout (location = 1) in vec3 aCenter;   // per instance
layout (location = 2) in float aSize;    // per instance
layout (location = 3) in vec4 aColor;    // per instance

uniform mat4 uView;
uniform mat4 uProjection;

// The camera's right and up axes in world space. Expanding the quad along
// these is what keeps every particle facing the viewer from any angle.
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vUV;
out vec4 vColor;

void main()
{
    vec3 world = aCenter
               + (uCameraRight * aCorner.x + uCameraUp * aCorner.y) * aSize;

    gl_Position = uProjection * uView * vec4(world, 1.0);

    vUV    = aCorner + 0.5;
    vColor = aColor;
}
