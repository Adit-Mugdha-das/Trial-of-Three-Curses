#include "Primitives.h"

#include <cmath>

namespace
{
    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;

    void addQuad(MeshData& m, unsigned int a, unsigned int b,
                 unsigned int c, unsigned int d)
    {
        // Counter-clockwise when viewed from outside.
        m.indices.push_back(a);
        m.indices.push_back(b);
        m.indices.push_back(c);

        m.indices.push_back(a);
        m.indices.push_back(c);
        m.indices.push_back(d);
    }
}

namespace Primitives
{

void computeTangents(MeshData& mesh)
{
    std::vector<glm::vec3> accumulated(mesh.vertices.size(), glm::vec3(0.0f));

    // For each triangle, solve for the direction in which U increases:
    //   edge1 = T*du1.x + B*du1.y
    //   edge2 = T*du2.x + B*du2.y
    // Inverting that 2x2 UV matrix isolates T.
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const unsigned int i0 = mesh.indices[i + 0];
        const unsigned int i1 = mesh.indices[i + 1];
        const unsigned int i2 = mesh.indices[i + 2];

        const glm::vec3& p0 = mesh.vertices[i0].position;
        const glm::vec3& p1 = mesh.vertices[i1].position;
        const glm::vec3& p2 = mesh.vertices[i2].position;

        const glm::vec2& w0 = mesh.vertices[i0].uv;
        const glm::vec2& w1 = mesh.vertices[i1].uv;
        const glm::vec2& w2 = mesh.vertices[i2].uv;

        const glm::vec3 edge1 = p1 - p0;
        const glm::vec3 edge2 = p2 - p0;

        const glm::vec2 duv1 = w1 - w0;
        const glm::vec2 duv2 = w2 - w0;

        const float determinant = duv1.x * duv2.y - duv2.x * duv1.y;

        // A degenerate UV triangle (a pole, or a collapsed seam) has no
        // meaningful tangent; skip it rather than dividing by ~zero.
        if (std::fabs(determinant) < 1e-9f)
        {
            continue;
        }

        const float r = 1.0f / determinant;
        const glm::vec3 tangent = (edge1 * duv2.y - edge2 * duv1.y) * r;

        // Summing over every triangle sharing a vertex averages the tangent
        // across them, which keeps the frame smooth over curved surfaces.
        accumulated[i0] += tangent;
        accumulated[i1] += tangent;
        accumulated[i2] += tangent;
    }

    for (std::size_t v = 0; v < mesh.vertices.size(); ++v)
    {
        const glm::vec3 n = mesh.vertices[v].normal;
        glm::vec3 t = accumulated[v];

        if (glm::dot(t, t) < 1e-12f)
        {
            // No usable tangent: build any vector perpendicular to the
            // normal. Choosing the axis the normal is least aligned with
            // avoids a near-zero cross product.
            const glm::vec3 axis = (std::fabs(n.y) < 0.9f)
                                 ? glm::vec3(0.0f, 1.0f, 0.0f)
                                 : glm::vec3(1.0f, 0.0f, 0.0f);
            t = glm::cross(axis, n);
        }

        // Gram-Schmidt: remove any component along the normal so the frame
        // is orthogonal. Averaging above can easily tilt it off the surface.
        t = t - n * glm::dot(n, t);

        const float lengthSquared = glm::dot(t, t);
        mesh.vertices[v].tangent = (lengthSquared > 1e-12f)
                                 ? t * (1.0f / std::sqrt(lengthSquared))
                                 : glm::vec3(1.0f, 0.0f, 0.0f);
    }
}

MeshData cube(float width, float height, float depth)
{
    MeshData m;

    const float x = width  * 0.5f;
    const float y = height * 0.5f;
    const float z = depth  * 0.5f;

    // Six faces, each with four vertices sharing one normal. Listed
    // counter-clockwise as seen from outside the cube.
    const glm::vec3 faceNormals[6] = {
        {  0.0f,  0.0f,  1.0f },   // front  (+Z)
        {  0.0f,  0.0f, -1.0f },   // back   (-Z)
        {  1.0f,  0.0f,  0.0f },   // right  (+X)
        { -1.0f,  0.0f,  0.0f },   // left   (-X)
        {  0.0f,  1.0f,  0.0f },   // top    (+Y)
        {  0.0f, -1.0f,  0.0f }    // bottom (-Y)
    };

    const glm::vec3 facePositions[6][4] = {
        { { -x, -y,  z }, {  x, -y,  z }, {  x,  y,  z }, { -x,  y,  z } },
        { {  x, -y, -z }, { -x, -y, -z }, { -x,  y, -z }, {  x,  y, -z } },
        { {  x, -y,  z }, {  x, -y, -z }, {  x,  y, -z }, {  x,  y,  z } },
        { { -x, -y, -z }, { -x, -y,  z }, { -x,  y,  z }, { -x,  y, -z } },
        { { -x,  y,  z }, {  x,  y,  z }, {  x,  y, -z }, { -x,  y, -z } },
        { { -x, -y, -z }, {  x, -y, -z }, {  x, -y,  z }, { -x, -y,  z } }
    };

    const glm::vec2 faceUVs[4] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    for (int f = 0; f < 6; ++f)
    {
        const unsigned int base = static_cast<unsigned int>(m.vertices.size());

        for (int v = 0; v < 4; ++v)
        {
            m.vertices.push_back({ facePositions[f][v], faceNormals[f], faceUVs[v] });
        }

        addQuad(m, base, base + 1, base + 2, base + 3);
    }

    computeTangents(m);
    return m;
}

MeshData sphere(float radius, int stacks, int slices)
{
    MeshData m;

    if (stacks < 2) { stacks = 2; }
    if (slices < 3) { slices = 3; }

    // phi runs 0 (north pole) -> PI (south pole), theta runs around Y.
    // The seam column is duplicated so UVs do not wrap backwards.
    for (int i = 0; i <= stacks; ++i)
    {
        const float v   = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = v * kPi;

        for (int j = 0; j <= slices; ++j)
        {
            const float u     = static_cast<float>(j) / static_cast<float>(slices);
            const float theta = u * kTwoPi;

            // For a sphere centred on the origin the normal is just the
            // normalised position, so build the unit vector first.
            const glm::vec3 n(
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            );

            m.vertices.push_back({ n * radius, n, { u, 1.0f - v } });
        }
    }

    const int stride = slices + 1;
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            const unsigned int topLeft     = static_cast<unsigned int>(i * stride + j);
            const unsigned int topRight    = topLeft + 1;
            const unsigned int bottomLeft  = static_cast<unsigned int>((i + 1) * stride + j);
            const unsigned int bottomRight = bottomLeft + 1;

            addQuad(m, topLeft, bottomLeft, bottomRight, topRight);
        }
    }

    computeTangents(m);
    return m;
}

MeshData cylinder(float radius, float height, int segments)
{
    MeshData m;

    if (segments < 3) { segments = 3; }

    const float halfHeight = height * 0.5f;

    // --- side wall -------------------------------------------------------
    const unsigned int sideBase = 0;
    for (int j = 0; j <= segments; ++j)
    {
        const float u     = static_cast<float>(j) / static_cast<float>(segments);
        const float theta = u * kTwoPi;
        const float c     = std::cos(theta);
        const float s     = std::sin(theta);

        // Side normal points straight out from the axis; no Y component.
        const glm::vec3 n(c, 0.0f, s);

        m.vertices.push_back({ { radius * c, -halfHeight, radius * s }, n, { u, 0.0f } });
        m.vertices.push_back({ { radius * c,  halfHeight, radius * s }, n, { u, 1.0f } });
    }

    for (int j = 0; j < segments; ++j)
    {
        const unsigned int bottomLeft  = sideBase + static_cast<unsigned int>(j * 2);
        const unsigned int topLeft     = bottomLeft + 1;
        const unsigned int bottomRight = bottomLeft + 2;
        const unsigned int topRight    = bottomLeft + 3;

        addQuad(m, bottomLeft, bottomRight, topRight, topLeft);
    }

    // --- caps ------------------------------------------------------------
    // Rim vertices are duplicated here with vertical normals, which is what
    // keeps the edge between wall and cap looking sharp instead of smeared.
    for (int cap = 0; cap < 2; ++cap)
    {
        const bool  isTop = (cap == 0);
        const float y     = isTop ? halfHeight : -halfHeight;
        const glm::vec3 n(0.0f, isTop ? 1.0f : -1.0f, 0.0f);

        const unsigned int centre = static_cast<unsigned int>(m.vertices.size());
        m.vertices.push_back({ { 0.0f, y, 0.0f }, n, { 0.5f, 0.5f } });

        for (int j = 0; j <= segments; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(segments)) * kTwoPi;
            const float c     = std::cos(theta);
            const float s     = std::sin(theta);

            m.vertices.push_back({
                { radius * c, y, radius * s },
                n,
                { 0.5f + 0.5f * c, 0.5f + 0.5f * s }
            });
        }

        for (int j = 0; j < segments; ++j)
        {
            const unsigned int a = centre + 1 + static_cast<unsigned int>(j);
            const unsigned int b = a + 1;

            // Winding flips between the two caps so both face outward.
            if (isTop)
            {
                m.indices.push_back(centre);
                m.indices.push_back(b);
                m.indices.push_back(a);
            }
            else
            {
                m.indices.push_back(centre);
                m.indices.push_back(a);
                m.indices.push_back(b);
            }
        }
    }

    computeTangents(m);
    return m;
}

MeshData cone(float radius, float height, int segments)
{
    MeshData m;

    if (segments < 3) { segments = 3; }

    const float halfHeight = height * 0.5f;

    // --- slanted side ----------------------------------------------------
    // The side normal is NOT (cos, 0, sin): the surface leans inward. Taking
    // the cross product of the two surface tangents gives
    //   n = normalize(height * cos, radius, height * sin)
    // which is what makes the specular highlight sit correctly in Phase 4.
    const float nY = radius;
    for (int j = 0; j <= segments; ++j)
    {
        const float u     = static_cast<float>(j) / static_cast<float>(segments);
        const float theta = u * kTwoPi;
        const float c     = std::cos(theta);
        const float s     = std::sin(theta);

        const glm::vec3 n = glm::normalize(glm::vec3(height * c, nY, height * s));

        // Apex is duplicated per segment so each triangle keeps its own normal.
        m.vertices.push_back({ { 0.0f, halfHeight, 0.0f }, n, { u, 1.0f } });
        m.vertices.push_back({ { radius * c, -halfHeight, radius * s }, n, { u, 0.0f } });
    }

    for (int j = 0; j < segments; ++j)
    {
        const unsigned int apex       = static_cast<unsigned int>(j * 2);
        const unsigned int baseLeft   = apex + 1;
        const unsigned int baseRight  = apex + 3;

        m.indices.push_back(apex);
        m.indices.push_back(baseLeft);
        m.indices.push_back(baseRight);
    }

    // --- base disk -------------------------------------------------------
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    const unsigned int centre = static_cast<unsigned int>(m.vertices.size());
    m.vertices.push_back({ { 0.0f, -halfHeight, 0.0f }, down, { 0.5f, 0.5f } });

    for (int j = 0; j <= segments; ++j)
    {
        const float theta = (static_cast<float>(j) / static_cast<float>(segments)) * kTwoPi;
        const float c     = std::cos(theta);
        const float s     = std::sin(theta);

        m.vertices.push_back({
            { radius * c, -halfHeight, radius * s },
            down,
            { 0.5f + 0.5f * c, 0.5f + 0.5f * s }
        });
    }

    for (int j = 0; j < segments; ++j)
    {
        m.indices.push_back(centre);
        m.indices.push_back(centre + 1 + static_cast<unsigned int>(j));
        m.indices.push_back(centre + 2 + static_cast<unsigned int>(j));
    }

    computeTangents(m);
    return m;
}

MeshData torus(float majorRadius, float minorRadius,
               int majorSegments, int minorSegments)
{
    MeshData m;

    if (majorSegments < 3) { majorSegments = 3; }
    if (minorSegments < 3) { minorSegments = 3; }

    // theta sweeps around the ring, phi around the tube cross-section.
    for (int i = 0; i <= majorSegments; ++i)
    {
        const float u     = static_cast<float>(i) / static_cast<float>(majorSegments);
        const float theta = u * kTwoPi;
        const float ct    = std::cos(theta);
        const float st    = std::sin(theta);

        for (int j = 0; j <= minorSegments; ++j)
        {
            const float v   = static_cast<float>(j) / static_cast<float>(minorSegments);
            const float phi = v * kTwoPi;
            const float cp  = std::cos(phi);
            const float sp  = std::sin(phi);

            const glm::vec3 position(
                (majorRadius + minorRadius * cp) * ct,
                minorRadius * sp,
                (majorRadius + minorRadius * cp) * st
            );

            // Points from the tube's centre circle outward to the surface.
            const glm::vec3 normal(cp * ct, sp, cp * st);

            m.vertices.push_back({ position, normal, { u, v } });
        }
    }

    const int stride = minorSegments + 1;
    for (int i = 0; i < majorSegments; ++i)
    {
        for (int j = 0; j < minorSegments; ++j)
        {
            const unsigned int a = static_cast<unsigned int>(i * stride + j);
            const unsigned int b = a + 1;
            const unsigned int c = static_cast<unsigned int>((i + 1) * stride + j);
            const unsigned int d = c + 1;

            addQuad(m, a, c, d, b);
        }
    }

    computeTangents(m);
    return m;
}

MeshData plane(float width, float depth, int subdivisions)
{
    MeshData m;

    if (subdivisions < 1) { subdivisions = 1; }

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;

    for (int i = 0; i <= subdivisions; ++i)
    {
        const float v = static_cast<float>(i) / static_cast<float>(subdivisions);

        for (int j = 0; j <= subdivisions; ++j)
        {
            const float u = static_cast<float>(j) / static_cast<float>(subdivisions);

            m.vertices.push_back({
                { -halfW + u * width, 0.0f, -halfD + v * depth },
                up,
                { u, v }
            });
        }
    }

    const int stride = subdivisions + 1;
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < subdivisions; ++j)
        {
            const unsigned int a = static_cast<unsigned int>(i * stride + j);
            const unsigned int b = a + 1;
            const unsigned int c = static_cast<unsigned int>((i + 1) * stride + j);
            const unsigned int d = c + 1;

            addQuad(m, a, c, d, b);
        }
    }

    computeTangents(m);
    return m;
}

MeshData disk(float radius, int segments)
{
    MeshData m;

    if (segments < 3) { segments = 3; }

    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    m.vertices.push_back({ { 0.0f, 0.0f, 0.0f }, up, { 0.5f, 0.5f } });

    for (int j = 0; j <= segments; ++j)
    {
        const float theta = (static_cast<float>(j) / static_cast<float>(segments)) * kTwoPi;
        const float c     = std::cos(theta);
        const float s     = std::sin(theta);

        m.vertices.push_back({
            { radius * c, 0.0f, radius * s },
            up,
            { 0.5f + 0.5f * c, 0.5f + 0.5f * s }
        });
    }

    for (int j = 0; j < segments; ++j)
    {
        m.indices.push_back(0);
        m.indices.push_back(static_cast<unsigned int>(j + 2));
        m.indices.push_back(static_cast<unsigned int>(j + 1));
    }

    computeTangents(m);
    return m;
}

} // namespace Primitives
