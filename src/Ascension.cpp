#include "Ascension.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Easing.h"

namespace
{
    float smooth(float t, float from, float to)
    {
        return Easing::smoothstep01((t - from) / (to - from));
    }

    glm::vec3 bezier(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& d, float u)
    {
        const float v = 1.0f - u;
        return a * (v * v * v) + b * (3.0f * v * v * u) + c * (3.0f * v * u * u) + d * (u * u * u);
    }

    float fract(float x) { return x - std::floor(x); }

    // A constellation: its stars in its own little 2D frame, the lines that
    // join them, where it sits in the sky and when it appears.
    struct Figure
    {
        glm::vec2 offset;
        float scale;
        float appearAt;
        std::vector<glm::vec2> points;
        std::vector<std::pair<int, int>> lines;
    };

    void ring(Figure& f, const glm::vec2& centre, float r, int n, float start = 0.0f)
    {
        const int first = static_cast<int>(f.points.size());
        for (int i = 0; i < n; ++i)
        {
            const float a = start + 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
            f.points.push_back(centre + glm::vec2(std::cos(a), std::sin(a)) * r);
        }
        for (int i = 0; i < n; ++i) { f.lines.push_back({ first + i, first + (i + 1) % n }); }
    }

    void chain(Figure& f, std::initializer_list<glm::vec2> pts)
    {
        int prev = -1;
        for (const glm::vec2& p : pts)
        {
            f.points.push_back(p);
            const int me = static_cast<int>(f.points.size()) - 1;
            if (prev >= 0) { f.lines.push_back({ prev, me }); }
            prev = me;
        }
    }
}

// ------------------------------------------------------------------- set-up --

void Ascension::reset()
{
    m_active = false;
    m_t = 0.0f;
}

void Ascension::begin(const glm::vec3& feet)
{
    m_active = true;
    m_t = 0.0f;
    m_feet = feet;

    // The sky the stars are drawn on: high up ahead of the boat.
    m_skyCentre = glm::vec3(0.0f, 158.0f, feet.z + 150.0f);
    buildConstellations();
}

void Ascension::update(float deltaTime)
{
    if (m_active) { m_t += deltaTime; }
}

// ------------------------------------------------------------------ him ------

float Ascension::bodyGlow() const
{
    return m_active ? smooth(m_t, 0.3f, 2.8f) : 0.0f;
}

float Ascension::bodyFade() const
{
    return m_active ? smooth(m_t, 6.0f, 8.5f) : 0.0f;
}

float Ascension::soulOrb() const
{
    return m_active ? smooth(m_t, 5.0f, 7.0f) : 0.0f;
}

glm::vec3 Ascension::boatAt(float t) const
{
    // Sailing slowly toward the sun, rocking a little.
    const float z = m_feet.z + 70.0f + 25.0f * Easing::clamp01((t - 14.0f) / 20.0f);
    return { 0.0f, 96.0f + 0.25f * std::sin(t * 1.3f), z };
}

glm::vec3 Ascension::seat(float t) const
{
    return boatAt(t) + glm::vec3(0.0f, 2.2f, -1.5f);
}

glm::vec3 Ascension::soulPosition() const
{
    const float t = m_t;
    const glm::vec3 f = m_feet;

    if (t < 5.0f)
    {
        // A slow lift off the ground.
        return f + glm::vec3(0.0f, 2.5f * smooth(t, 2.0f, 5.0f), 0.0f);
    }
    if (t < 11.0f)
    {
        return f + glm::vec3(0.0f, 2.5f + 57.5f * smooth(t, 5.0f, 11.0f), 0.0f);
    }
    if (t < 14.5f)
    {
        // Up through the clouds (78-86) at about 13 s.
        return f + glm::vec3(0.0f, 60.0f + 35.0f * Easing::clamp01((t - 11.0f) / 3.5f), 0.0f);
    }
    if (t < 24.0f)
    {
        // Along the river of souls to the boat.
        const float u = smooth(t, 14.5f, 24.0f);
        return bezier(f + glm::vec3(0.0f, 95.0f, 0.0f),
                      glm::vec3(-18.0f, 95.0f, f.z + 25.0f),
                      glm::vec3(14.0f, 98.0f, f.z + 55.0f),
                      seat(24.0f), u);
    }
    if (t < 34.0f)
    {
        return seat(t);
    }
    // Out of the boat and up into the sky, to the heart of his constellation.
    return glm::mix(seat(34.0f), m_heart, smooth(t, 34.0f, 39.0f));
}

// ----------------------------------------------------------------- world -----

float Ascension::clouds() const
{
    return m_active ? smooth(m_t, 11.0f, 12.5f) : 0.0f;
}

float Ascension::skyGold() const
{
    return m_active ? smooth(m_t, 12.0f, 15.0f) * (1.0f - smooth(m_t, 26.0f, 29.0f)) : 0.0f;
}

float Ascension::skyNight() const
{
    return m_active ? smooth(m_t, 26.0f, 29.5f) : 0.0f;
}

float Ascension::boat() const
{
    return m_active ? smooth(m_t, 12.5f, 14.5f) : 0.0f;
}

glm::vec3 Ascension::boatPosition() const
{
    return boatAt(m_t);
}

float Ascension::oarAngle(int oar) const
{
    // Every oar on a side pulls together; the two sides together too.
    return 28.0f * std::sin(m_t * 2.2f + 0.15f * static_cast<float>(oar % 4));
}

glm::vec3 Ascension::skySunPosition() const
{
    // It sets into the clouds as night falls.
    return { 0.0f, 104.0f - 30.0f * skyNight(), m_feet.z + 250.0f };
}

float Ascension::skySun() const
{
    return m_active ? smooth(m_t, 11.5f, 14.0f) * (1.0f - skyNight()) : 0.0f;
}

// ----------------------------------------------------------- the river -------

glm::vec3 Ascension::riverPosition(int i) const
{
    const float seed = static_cast<float>(i) * 0.61803f;
    const float u = fract(seed + m_t * 0.06f);
    const glm::vec3 f = m_feet;
    const glm::vec3 p = bezier(glm::vec3(-70.0f, 90.0f, f.z - 20.0f),
                               glm::vec3(-25.0f, 93.0f, f.z + 15.0f),
                               glm::vec3(10.0f, 97.0f, f.z + 50.0f),
                               seat(m_t), u);

    // A wide, loose stream rather than a single file.
    const float a = seed * 37.0f + m_t * 1.3f;
    const float width = 2.4f * (1.0f - 0.7f * u);
    return p + glm::vec3(std::cos(a) * width, std::sin(a * 1.7f) * 1.2f, std::sin(a) * width);
}

float Ascension::riverGlow(int i) const
{
    if (!m_active) { return 0.0f; }
    const float seed = static_cast<float>(i) * 0.61803f;
    const float u = fract(seed + m_t * 0.06f);

    // In at the far end, out as they reach the boat.
    const float ends = smooth(u, 0.0f, 0.1f) * (1.0f - smooth(u, 0.9f, 1.0f));
    const float flash = 0.65f + 0.35f * std::sin(m_t * 3.0f + seed * 11.0f);
    return ends * flash * smooth(m_t, 12.5f, 14.5f) * (1.0f - smooth(m_t, 28.0f, 31.0f));
}

// ------------------------------------------------------ the constellations ---

void Ascension::buildConstellations()
{
    m_stars.clear();

    std::vector<Figure> figures;

    // The scale of judgment.
    {
        Figure f{ { -45.0f, 14.0f }, 2.2f, 27.6f, {}, {} };
        chain(f, { { -5.0f, 0.0f }, { -4.0f, 3.0f }, { -3.0f, 0.0f } });
        chain(f, { { 3.0f, 0.0f }, { 4.0f, 3.0f }, { 5.0f, 0.0f } });
        f.lines.push_back({ 0, 2 });
        f.lines.push_back({ 3, 5 });
        chain(f, { { -4.0f, 3.0f }, { 0.0f, 4.0f }, { 4.0f, 3.0f } });
        chain(f, { { 0.0f, 4.0f }, { 0.0f, -3.0f } });
        chain(f, { { -2.0f, -3.0f }, { 2.0f, -3.0f } });
        figures.push_back(f);
    }
    // The Djinn's lamp, with its smoke rising.
    {
        Figure f{ { 45.0f, 16.0f }, 2.2f, 29.2f, {}, {} };
        chain(f, { { -6.0f, 1.2f }, { -3.0f, 0.5f }, { -1.5f, 1.5f }, { 1.5f, 1.5f }, { 3.0f, 0.3f },
                   { 1.5f, -1.2f }, { -1.5f, -1.2f }, { -3.0f, 0.5f } });
        chain(f, { { 3.0f, 0.3f }, { 4.5f, 1.0f }, { 4.2f, -0.8f }, { 1.5f, -1.2f } });
        chain(f, { { -1.5f, 1.5f }, { 0.0f, 2.8f }, { 1.5f, 1.5f } });
        chain(f, { { 0.0f, 2.8f }, { 0.5f, 4.0f }, { -0.5f, 5.2f }, { 0.8f, 6.4f } });
        figures.push_back(f);
    }
    // The three rings of the gate.
    {
        Figure f{ { -48.0f, -18.0f }, 2.0f, 30.8f, {}, {} };
        ring(f, { 0.0f, 0.0f }, 1.5f, 6);
        ring(f, { 0.0f, 0.0f }, 3.0f, 10);
        ring(f, { 0.0f, 0.0f }, 4.5f, 14);
        figures.push_back(f);
    }
    // Medusa's eye - closed now - and her snakes, still.
    {
        Figure f{ { 46.0f, -16.0f }, 2.1f, 32.4f, {}, {} };
        for (int lid = 0; lid < 2; ++lid)
        {
            const float h = (lid == 0) ? 1.0f : -0.6f;
            int prev = -1;
            for (int k = 0; k <= 8; ++k)
            {
                const float x = -4.0f + static_cast<float>(k);
                f.points.push_back({ x, h * (1.0f - (x / 4.0f) * (x / 4.0f)) });
                const int me = static_cast<int>(f.points.size()) - 1;
                if (prev >= 0) { f.lines.push_back({ prev, me }); }
                prev = me;
            }
        }
        chain(f, { { -2.5f, 1.2f }, { -3.2f, 2.4f }, { -2.4f, 3.4f }, { -3.0f, 4.4f } });
        chain(f, { {  0.0f, 1.2f }, {  0.6f, 2.4f }, { -0.3f, 3.5f }, {  0.4f, 4.6f } });
        chain(f, { {  2.5f, 1.2f }, {  3.2f, 2.4f }, {  2.4f, 3.4f }, {  3.0f, 4.4f } });
        figures.push_back(f);
    }
    // The traveller himself, arms raised. His heart star is added apart.
    {
        Figure f{ { 0.0f, -2.0f }, 1.6f, 35.5f, {}, {} };
        ring(f, { 0.0f, 6.2f }, 1.1f, 7, 1.5708f);
        chain(f, { { 0.0f, 5.1f }, { 0.0f, 0.5f } });
        chain(f, { { -3.8f, 7.8f }, { -3.0f, 5.8f }, { -1.6f, 4.4f }, { 0.0f, 5.1f },
                   { 1.6f, 4.4f }, { 3.0f, 5.8f }, { 3.8f, 7.8f } });
        chain(f, { { -1.8f, -4.8f }, { -1.0f, -2.0f }, { 0.0f, 0.5f }, { 1.0f, -2.0f }, { 1.8f, -4.8f } });
        figures.push_back(f);
    }

    // --- onto the sky: a plane facing the camera that will look at it ---------
    const glm::vec3 eye = boatAt(30.0f) + glm::vec3(-6.0f, 4.0f, -16.0f);
    const glm::vec3 forward = glm::normalize(m_skyCentre - eye);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up = glm::cross(right, forward);

    auto onSky = [&](const glm::vec2& p)
    {
        return m_skyCentre + right * p.x + up * p.y;
    };

    for (std::size_t fi = 0; fi < figures.size(); ++fi)
    {
        const Figure& f = figures[fi];
        auto place = [&](const glm::vec2& p) { return f.offset + p * f.scale; };

        // The stars, one after another.
        for (std::size_t i = 0; i < f.points.size(); ++i)
        {
            Star s;
            s.position = onSky(place(f.points[i]));
            s.appearAt = f.appearAt + 0.05f * static_cast<float>(i);
            s.size = 1.8f;
            s.vertex = true;
            s.twinkle = static_cast<float>(i) * 1.7f + static_cast<float>(fi);
            s.figure = static_cast<int>(fi);
            m_stars.push_back(s);
        }
        // Then the lines between them, drawn as if by a finger.
        for (std::size_t l = 0; l < f.lines.size(); ++l)
        {
            const glm::vec2 a = place(f.points[f.lines[l].first]);
            const glm::vec2 b = place(f.points[f.lines[l].second]);
            const int dots = std::max(1, static_cast<int>(glm::length(b - a) / 1.4f));
            for (int d = 1; d < dots; ++d)
            {
                const float u = static_cast<float>(d) / static_cast<float>(dots);
                Star s;
                s.position = onSky(glm::mix(a, b, u));
                s.appearAt = f.appearAt + 0.4f + 0.06f * static_cast<float>(l) + 0.25f * u;
                s.size = 0.75f;
                s.vertex = false;
                s.twinkle = static_cast<float>(d) * 0.9f;
                s.figure = static_cast<int>(fi);
                m_stars.push_back(s);
            }
        }
    }

    // An ordinary night sky behind the figures, coming out as night falls.
    unsigned seed = 12345u;
    auto next = [&seed]()
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f;
    };
    for (int i = 0; i < 220; ++i)
    {
        Star s;
        s.position = onSky({ -80.0f + 160.0f * next(), -34.0f + 68.0f * next() });
        s.appearAt = 26.5f + 2.5f * next();
        s.size = 0.35f + 0.55f * next() * next();
        s.vertex = false;
        s.twinkle = 6.28f * next();
        s.figure = -1;
        m_stars.push_back(s);
    }

    // His heart: the star he becomes.
    const Figure& him = figures.back();
    m_heart = onSky(him.offset + glm::vec2(0.0f, 3.2f) * him.scale);
}

float Ascension::starGlow(int i) const
{
    const Star& s = m_stars[i];
    if (m_t < s.appearAt) { return 0.0f; }
    const float on = smooth(m_t, s.appearAt, s.appearAt + 0.5f);
    const float twinkle = 0.8f + 0.2f * std::sin(m_t * 3.0f + s.twinkle);

    // His own constellation flares when his star arrives.
    const float flare = (s.figure == 4) ? 1.0f + 0.6f * heartGlow() : 1.0f;
    return on * twinkle * flare;
}

float Ascension::starSize(int i) const
{
    return m_stars[i].size;
}

float Ascension::heartGlow() const
{
    return m_active ? smooth(m_t, 38.0f, 39.5f) : 0.0f;
}

// ---------------------------------------------------------------- camera -----

Ascension::Shot Ascension::shotAt(int which, float t) const
{
    const glm::vec3 s = soulPosition();
    const glm::vec3 f = m_feet;
    switch (which)
    {
        case 0:   // close on him as the light takes him
        {
            const float lift = s.y - f.y;
            return { f + glm::vec3(-3.2f, 2.4f + lift, -4.4f), f + glm::vec3(0.0f, 1.5f + lift, 0.0f) };
        }
        case 1:   // following him up
            return { s + glm::vec3(-5.0f, 1.0f, -7.0f), s + glm::vec3(0.0f, 1.0f, 0.0f) };
        case 2:   // the whole temple below, him rising toward us
            return { glm::vec3(-30.0f, 64.0f, f.z - 36.0f), glm::vec3(0.0f, 0.0f, f.z - 36.0f) };
        case 3:   // from just below as he breaks through the clouds
            return { glm::vec3(f.x - 14.0f, std::max(90.0f, s.y + 4.0f), f.z - 16.0f),
                     s + glm::vec3(0.0f, 1.0f, 0.0f) };
        case 4:   // beside the river of souls, the boat ahead
            return { s + glm::vec3(-16.0f, 5.0f, -12.0f), glm::mix(s, boatAt(t), 0.5f) };
        default:  // from the boat, up at the stars
            return { boatAt(t) + glm::vec3(-6.0f, 4.0f, -16.0f), m_skyCentre };
    }
}

Ascension::Shot Ascension::camera() const
{
    // Each shot takes over from the last with a smooth blend.
    const float starts[6] = { 0.0f, 5.0f, 7.5f, 11.5f, 14.5f, 26.0f };
    const float blends[6] = { 0.0f, 1.5f, 2.0f, 1.5f, 1.8f, 2.5f };

    int shot = 0;
    for (int k = 5; k >= 0; --k)
    {
        if (m_t >= starts[k]) { shot = k; break; }
    }

    Shot now = shotAt(shot, m_t);
    if (shot > 0)
    {
        const float b = smooth(m_t, starts[shot], starts[shot] + blends[shot]);
        if (b < 1.0f)
        {
            const Shot before = shotAt(shot - 1, m_t);
            now.eye  = glm::mix(before.eye,  now.eye,  b);
            now.look = glm::mix(before.look, now.look, b);
        }
    }
    return now;
}
