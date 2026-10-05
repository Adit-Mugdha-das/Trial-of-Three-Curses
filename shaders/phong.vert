#version 330 core

// Phase 2: full transform pipeline. The lighting varyings produced here are
// already the ones Phase 4's Blinn-Phong shader will consume, so this file
// should not need to change again.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// Inverse-transpose of the model matrix. Uploaded from the CPU rather than
// inverted per-vertex here, because inverse() in a shader is expensive.
// Without this, non-uniform scaling (the pulsing heart, the growing rings)
// would skew the normals and break lighting.
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

// The other two axes of the tangent frame, in world space. Passed as separate
// vectors rather than a mat3 varying, which some drivers handle poorly.
out vec3 vTangent;
out vec3 vBitangent;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    vec3 N = normalize(uNormalMatrix * aNormal);
    vec3 T = normalize(uNormalMatrix * aTangent);

    // Re-orthogonalise: interpolating tangents across a triangle can tilt
    // them off the surface, and a skewed frame shears the mapped normals.
    T = normalize(T - N * dot(N, T));

    vWorldPos  = worldPos.xyz;
    vNormal    = N;
    vTangent   = T;
    vBitangent = cross(N, T);
    vUV        = aUV;

    gl_Position = uProjection * uView * worldPos;
}
