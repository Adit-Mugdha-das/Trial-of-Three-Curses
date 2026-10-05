#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "Mesh.h"

// Procedural geometry for the whole scene. Every generator produces correct
// outward normals and UVs, since Phase 4 lighting depends entirely on them.
//
// Convention: all shapes are centred on the origin and Y is up.
namespace Primitives
{
    // Axis-aligned box. 24 vertices so each face gets its own flat normal.
    MeshData cube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

    // UV sphere. Smooth normals (normal == normalised position).
    MeshData sphere(float radius = 0.5f, int stacks = 24, int slices = 48);

    // Open-ended tube plus two caps. Caps use duplicated rim vertices so the
    // seam between side and cap stays sharp.
    MeshData cylinder(float radius = 0.5f, float height = 1.0f, int segments = 48);

    // Apex at +height/2, base disk at -height/2.
    MeshData cone(float radius = 0.5f, float height = 1.0f, int segments = 48);

    // Ring of radius majorRadius, tube of radius minorRadius, lying in XZ.
    MeshData torus(float majorRadius = 0.5f, float minorRadius = 0.15f,
                   int majorSegments = 48, int minorSegments = 24);

    // Flat grid in the XZ plane facing +Y. Subdivided so it can catch
    // per-vertex effects and, later, shadows.
    MeshData plane(float width = 1.0f, float depth = 1.0f, int subdivisions = 1);

    // Filled circle in the XZ plane facing +Y. Used for the scale pans.
    MeshData disk(float radius = 0.5f, int segments = 48);

    // Derives per-vertex tangents from positions and UVs. Every generator
    // above calls this, so all geometry arrives normal-map ready.
    void computeTangents(MeshData& mesh);
}

#endif // PRIMITIVES_H
