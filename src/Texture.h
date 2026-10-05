#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>

#include <glad/glad.h>

// A 2D texture with mipmaps and repeat wrapping.
//
// Two ways in: load a real image file through stb_image, or hand it a pixel
// buffer generated on the CPU (see ProceduralTexture.h). The scene uses the
// procedural path so the project needs no binary assets, but dropping real
// images into assets/textures/ and calling loadFromFile works identically.
class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool loadFromFile(const std::string& path);
    void createFromPixels(int width, int height, int channels,
                          const unsigned char* pixels);

    void bind(unsigned int unit) const;

    GLuint id() const { return m_id; }
    bool valid() const { return m_id != 0; }

    // A 1x1 white texture, created on first use. Bound to any sampler slot a
    // material leaves empty: some drivers misbehave when a sampler points at
    // a unit with nothing bound, even if the shader branches around the
    // sample. White is also the identity for the multiply the shader does.
    static const Texture& white();

private:
    GLuint m_id = 0;
    int m_width  = 0;
    int m_height = 0;

    void release();
};

#endif // TEXTURE_H
