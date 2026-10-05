#version 330 core

// Screen-space quad. aPos is a 0..1 unit square; origin and size place it
// directly in normalised device coordinates, so no projection is needed.

layout (location = 0) in vec2 aPos;

uniform vec2 uOrigin;   // bottom-left corner, NDC
uniform vec2 uSize;     // width and height, NDC

void main()
{
    gl_Position = vec4(uOrigin + aPos * uSize, 0.0, 1.0);
}
