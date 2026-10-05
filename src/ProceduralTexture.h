#ifndef PROCEDURALTEXTURE_H
#define PROCEDURALTEXTURE_H

#include <vector>

// CPU-generated textures, so the project ships with no binary assets.
//
// Every generator is TILEABLE: the noise lattice wraps at the image edge, so
// a wall tiled 6x6 shows no seams. Swap any of these for a real image via
// Texture::loadFromFile without touching anything else.
namespace Procedural
{
    struct Image
    {
        int width    = 0;
        int height   = 0;
        int channels = 3;
        std::vector<unsigned char> pixels;
    };

    // --- diffuse maps ---
    Image sandstone(int size = 256);     // mottled warm blocks, horizontal strata
    Image floorTiles(int size = 256);    // dark flagstones with grout lines
    Image goldBrushed(int size = 256);   // fine directional brushing
    Image brassPatina(int size = 256);   // aged brass with green verdigris
    Image roughStone(int size = 256);    // speckled grey granite
    Image snakeScales(int size = 256);   // staggered rounded scales

    // --- specular maps ---
    // Greyscale; multiplied into the material's ks so shininess varies across
    // a single surface. Verdigris does not shine; bare brass does.
    Image brassSpecular(int size = 256);
    Image stoneSpecular(int size = 256);
    Image snakeSpecular(int size = 256);

    // --- normal maps ---
    // Treats the source image's luminance as a height field and differentiates
    // it into a tangent-space normal map. For these textures the shading IS
    // the relief - the dome across each snake scale, the grain in sandstone -
    // so the diffuse doubles as a height map and the two register exactly.
    // `strength` scales the apparent depth.
    Image normalFromLuminance(const Image& source, float strength = 2.5f);
}

#endif // PROCEDURALTEXTURE_H
