#include "Texture.h"

#include <iostream>

// The one and only place stb_image's implementation is compiled.
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

Texture::~Texture()
{
    release();
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_id = 0;
    other.m_width = 0;
    other.m_height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        release();

        m_id     = other.m_id;
        m_width  = other.m_width;
        m_height = other.m_height;

        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Texture::release()
{
    if (m_id != 0)
    {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_width = 0;
    m_height = 0;
}

void Texture::createFromPixels(int width, int height, int channels,
                               const unsigned char* pixels)
{
    release();

    m_width  = width;
    m_height = height;

    GLenum format = GL_RGB;
    if (channels == 1) { format = GL_RED; }
    else if (channels == 4) { format = GL_RGBA; }

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    // Rows are tightly packed and may not be 4-byte aligned (a 3-channel
    // texture of odd width is the classic case), so relax the default.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format),
                 width, height, 0, format, GL_UNSIGNED_BYTE, pixels);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Texture::loadFromFile(const std::string& path)
{
    // OpenGL's first texel row is the bottom one; image files store the top
    // row first. Without this flip every texture is upside down.
    stbi_set_flip_vertically_on_load(true);

    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (pixels == nullptr)
    {
        std::cout << "[Texture] Failed to load '" << path << "': "
                  << stbi_failure_reason() << '\n';
        return false;
    }

    createFromPixels(width, height, channels, pixels);
    stbi_image_free(pixels);

    return true;
}

void Texture::bind(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

const Texture& Texture::white()
{
    // Function-local static: constructed on first call, which is guaranteed
    // to be after the GL context exists.
    static Texture fallback;

    if (!fallback.valid())
    {
        const unsigned char pixel[3] = { 255, 255, 255 };
        fallback.createFromPixels(1, 1, 3, pixel);
    }

    return fallback;
}
