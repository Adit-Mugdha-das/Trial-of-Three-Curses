#include "ProceduralTexture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

namespace
{
    // --- tileable value noise ------------------------------------------------

    std::uint32_t hashInt(std::uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    // Lattice coordinates wrap at `period`, which is what makes the result
    // tile seamlessly.
    float lattice(int x, int y, int period, std::uint32_t seed)
    {
        x = ((x % period) + period) % period;
        y = ((y % period) + period) % period;

        const std::uint32_t h = hashInt(static_cast<std::uint32_t>(x) * 374761393u +
                                        static_cast<std::uint32_t>(y) * 668265263u +
                                        seed);
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
    }

    float fade(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

    float mixf(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    // u, v in [0,1). Sampled on a `period` x `period` lattice.
    float valueNoise(float u, float v, int period, std::uint32_t seed)
    {
        const float fx = u * static_cast<float>(period);
        const float fy = v * static_cast<float>(period);

        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));

        const float sx = fade(fx - static_cast<float>(x0));
        const float sy = fade(fy - static_cast<float>(y0));

        const float v00 = lattice(x0,     y0,     period, seed);
        const float v10 = lattice(x0 + 1, y0,     period, seed);
        const float v01 = lattice(x0,     y0 + 1, period, seed);
        const float v11 = lattice(x0 + 1, y0 + 1, period, seed);

        return mixf(mixf(v00, v10, sx), mixf(v01, v11, sx), sy);
    }

    // Summed octaves. Each octave doubles the lattice period, so every one
    // still wraps and the total stays tileable.
    float fbm(float u, float v, int basePeriod, int octaves, std::uint32_t seed)
    {
        float sum    = 0.0f;
        float amp    = 0.5f;
        float total  = 0.0f;
        int   period = basePeriod;

        for (int o = 0; o < octaves; ++o)
        {
            sum   += amp * valueNoise(u, v, period, seed + static_cast<std::uint32_t>(o) * 7919u);
            total += amp;
            amp   *= 0.5f;
            period *= 2;
        }

        return sum / total;
    }

    float clamp01(float x)
    {
        return std::min(1.0f, std::max(0.0f, x));
    }

    unsigned char toByte(float x)
    {
        return static_cast<unsigned char>(clamp01(x) * 255.0f + 0.5f);
    }

    Procedural::Image makeImage(int size, int channels)
    {
        Procedural::Image image;
        image.width    = size;
        image.height   = size;
        image.channels = channels;
        image.pixels.resize(static_cast<std::size_t>(size) * size * channels);
        return image;
    }

    void setRGB(Procedural::Image& image, int x, int y, float r, float g, float b)
    {
        const std::size_t i = (static_cast<std::size_t>(y) * image.width + x) * 3;
        image.pixels[i + 0] = toByte(r);
        image.pixels[i + 1] = toByte(g);
        image.pixels[i + 2] = toByte(b);
    }
}

namespace Procedural
{

Image sandstone(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            // Stretched horizontally so the grain reads as sedimentary strata
            // rather than uniform mush.
            const float grain = fbm(u, v * 3.0f, 8, 5, 11u);
            const float bands = 0.5f + 0.5f * std::sin(v * 34.0f + fbm(u, v, 4, 3, 23u) * 5.0f);

            const float shade = 0.72f + 0.24f * grain + 0.10f * bands;

            setRGB(image, x, y,
                   shade * 0.86f,
                   shade * 0.72f,
                   shade * 0.53f);
        }
    }

    return image;
}

Image floorTiles(int size)
{
    Image image = makeImage(size, 3);

    constexpr int kTiles = 4;
    const float cell = 1.0f / kTiles;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            // Distance to the nearest grout line, in cell-local units.
            const float cu = std::fmod(u, cell) / cell;
            const float cv = std::fmod(v, cell) / cell;
            const float edge = std::min(std::min(cu, 1.0f - cu),
                                        std::min(cv, 1.0f - cv));

            const float grout = clamp01(edge / 0.06f);

            // Per-tile brightness, so neighbouring flagstones differ.
            const int tileX = static_cast<int>(u * kTiles);
            const int tileY = static_cast<int>(v * kTiles);
            const float tileTint =
                0.82f + 0.28f * lattice(tileX, tileY, kTiles, 77u);

            const float grain = fbm(u, v, 16, 4, 41u);
            const float shade = (0.55f + 0.35f * grain) * tileTint;

            // Grout is darker and cooler than the stone it separates.
            const float lit = mixf(0.34f, shade, grout);

            setRGB(image, x, y,
                   lit * 0.78f,
                   lit * 0.68f,
                   lit * 0.55f);
        }
    }

    return image;
}

Image goldBrushed(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            // Very high frequency along u, very low along v: directional
            // brushing, the thing that makes metal read as metal.
            const float brush = fbm(u * 1.0f, v * 0.06f, 64, 3, 5u);
            const float broad = fbm(u, v, 4, 3, 9u);

            const float shade = 0.80f + 0.22f * brush + 0.10f * broad;

            setRGB(image, x, y,
                   shade * 1.00f,
                   shade * 0.84f,
                   shade * 0.42f);
        }
    }

    return image;
}

Image brassPatina(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            const float blotch = fbm(u, v, 6, 5, 61u);

            // Sharp threshold, so verdigris sits in defined patches instead of
            // fading evenly across the whole surface.
            const float patina = clamp01((blotch - 0.52f) / 0.18f);

            const float grain = 0.88f + 0.18f * fbm(u, v, 32, 3, 83u);

            const float baseR = 0.86f, baseG = 0.66f, baseB = 0.24f;
            const float patR  = 0.32f, patG  = 0.62f, patB  = 0.52f;

            setRGB(image, x, y,
                   mixf(baseR, patR, patina) * grain,
                   mixf(baseG, patG, patina) * grain,
                   mixf(baseB, patB, patina) * grain);
        }
    }

    return image;
}

Image brassSpecular(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            // Same noise and threshold as brassPatina, so the two maps line
            // up exactly: where the diffuse turns green, the specular dies.
            const float blotch = fbm(u, v, 6, 5, 61u);
            const float patina = clamp01((blotch - 0.52f) / 0.18f);

            const float shine = mixf(1.0f, 0.12f, patina);

            setRGB(image, x, y, shine, shine, shine);
        }
    }

    return image;
}

Image roughStone(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            const float base    = fbm(u, v, 8, 5, 101u);
            const float speckle = valueNoise(u, v, 128, 137u);

            // Hard speckle threshold gives the mineral flecks in granite.
            const float fleck = (speckle > 0.78f) ? 0.22f : 0.0f;

            const float shade = 0.52f + 0.30f * base + fleck;

            setRGB(image, x, y,
                   shade * 0.94f,
                   shade * 0.94f,
                   shade * 1.00f);
        }
    }

    return image;
}

Image stoneSpecular(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            // Only the mineral flecks catch any light at all.
            const float speckle = valueNoise(u, v, 128, 137u);
            const float shine = (speckle > 0.78f) ? 0.85f : 0.12f;

            setRGB(image, x, y, shine, shine, shine);
        }
    }

    return image;
}

namespace
{
    // Shared by the scale diffuse and specular maps so they register exactly.
    // Returns how deep inside a scale the sample is: 1 at the centre, 0 at
    // the boundary. Rows are offset by half a cell, giving the staggered
    // arrangement real snakeskin has.
    float scaleField(float u, float v, int rows, int cols)
    {
        const float fy  = v * static_cast<float>(rows);
        const int   row = static_cast<int>(std::floor(fy));
        const float ly  = fy - static_cast<float>(row);

        const float offset = (row % 2 == 0) ? 0.0f : 0.5f;
        const float fx = u * static_cast<float>(cols) + offset;
        const float lx = fx - std::floor(fx);

        // Ellipse distance, wider than tall, measured from the cell centre.
        const float dx = (lx - 0.5f) * 2.0f;
        const float dy = (ly - 0.5f) * 2.4f;
        const float d  = std::sqrt(dx * dx + dy * dy);

        return clamp01(1.0f - d);
    }
}

Image snakeScales(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            const float depth = scaleField(u, v, 16, 12);

            // Rounded shading across each scale, dark in the gaps between.
            const float dome  = std::sqrt(depth);
            const float grain = 0.90f + 0.20f * fbm(u, v, 24, 3, 211u);

            const float shade = (0.22f + 0.78f * dome) * grain;

            setRGB(image, x, y,
                   shade * 0.34f,
                   shade * 0.66f,
                   shade * 0.32f);
        }
    }

    return image;
}

Image snakeSpecular(int size)
{
    Image image = makeImage(size, 3);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / size;
            const float v = static_cast<float>(y) / size;

            const float depth = scaleField(u, v, 16, 12);

            // Scale crowns are glossy; the recessed gaps are not.
            const float shine = 0.10f + 0.90f * depth * depth;

            setRGB(image, x, y, shine, shine, shine);
        }
    }

    return image;
}

Image normalFromLuminance(const Image& source, float strength)
{
    // Every generator here produces a square image.
    Image image = makeImage(source.width, 3);

    const int w = source.width;
    const int h = source.height;

    auto heightAt = [&source, w, h](int x, int y)
    {
        // Wrap, so the normal map tiles exactly as the diffuse does. Clamping
        // here would leave a seam of flat normals along every edge.
        x = ((x % w) + w) % w;
        y = ((y % h) + h) % h;

        const std::size_t i = (static_cast<std::size_t>(y) * w + x) * source.channels;

        const float r = source.pixels[i + 0] / 255.0f;
        const float g = source.pixels[i + 1] / 255.0f;
        const float b = source.pixels[i + 2] / 255.0f;

        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            // Sobel: a weighted 3x3 gradient. It samples the diagonals too,
            // so it is far less noisy than a plain two-tap difference, which
            // matters when the height field is fractal noise.
            const float tl = heightAt(x - 1, y - 1);
            const float tc = heightAt(x,     y - 1);
            const float tr = heightAt(x + 1, y - 1);
            const float ml = heightAt(x - 1, y);
            const float mr = heightAt(x + 1, y);
            const float bl = heightAt(x - 1, y + 1);
            const float bc = heightAt(x,     y + 1);
            const float br = heightAt(x + 1, y + 1);

            const float dx = (tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl);
            const float dy = (bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr);

            // A surface sloping up along +X has a normal leaning toward -X,
            // hence the negated gradients. Z is 1: in tangent space, flat
            // points straight out of the surface.
            glm::vec3 n(-dx * strength, -dy * strength, 1.0f);
            n = glm::normalize(n);

            // Normals are signed but the texture is unsigned, so remap
            // [-1,1] into [0,1]. The shader undoes this.
            setRGB(image, x, y,
                   n.x * 0.5f + 0.5f,
                   n.y * 0.5f + 0.5f,
                   n.z * 0.5f + 0.5f);
        }
    }

    return image;
}

} // namespace Procedural
