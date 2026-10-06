#include "DjinnForm.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"

namespace
{
    constexpr float kTwoPi = 6.2831853f;
    constexpr float kArmLength = 1.9f;
    constexpr float kRiseTime  = 1.5f;   // bottom to top as he forms
    constexpr float kSinkTime  = 1.2f;   // and back down
}

float DjinnForm::random(float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    return dist(m_rng);
}

void DjinnForm::init(unsigned seed)
{
    m_rng.seed(seed);
    m_parts.clear();

    auto add = [this](Part part, int n, int side = 0, bool pointing = false)
    {
        for (int i = 0; i < n; ++i)
        {
            Particle p;
            p.part = part;
            p.side = side;
            p.pointing = pointing;
            p.u = random(0.0f, 1.0f);
            p.v = random(0.0f, 1.0f);
            p.jitter = { random(-1.0f, 1.0f), random(-1.0f, 1.0f), random(-1.0f, 1.0f) };
            p.phase = random(0.0f, kTwoPi);
            p.tint = random(0.0f, 1.0f);
            p.sizeJitter = random(0.75f, 1.25f);
            m_parts.push_back(p);
        }
    };

    add(Part::Tail,   80);
    add(Part::Torso, 140);
    add(Part::Head,   80);
    add(Part::Turban, 25);
    add(Part::Beard,  18);
    add(Part::Eye,     6, -1);
    add(Part::Eye,     6,  1);
    // Which arm points is decided by where the target is; the flag says
    // "this is the arm on side `side`", and m_pointSide picks one.
    add(Part::Arm,    40, -1);
    add(Part::Arm,    40,  1);
    add(Part::Hand,   10, -1);
    add(Part::Hand,   10,  1);

    hideNow();
}

// -------------------------------------------------------------- the figure --

glm::vec3 DjinnForm::shoulder(int side) const
{
    return { 1.05f * static_cast<float>(side), 3.45f, 0.05f };
}

glm::vec3 DjinnForm::armDirection(bool pointing) const
{
    // Raised and spread, palms to the sky: the pose of a Djinn granting
    // a wish. The pointing arm swings from that onto its target.
    const float s = static_cast<float>(pointing ? m_pointSide : -m_pointSide);
    const glm::vec3 rest = glm::normalize(glm::vec3(s, 0.55f, 0.3f));
    if (!pointing) { return rest; }

    const glm::vec3 from = toWorld(shoulder(m_pointSide));
    glm::vec3 aim = toLocalDirection(m_pointAt - from);
    if (glm::length(aim) < 1e-3f) { return rest; }
    aim = glm::normalize(aim);

    const glm::vec3 d = glm::mix(rest, aim, Easing::smoothstep01(m_point));
    return glm::length(d) > 1e-3f ? glm::normalize(d) : rest;
}

glm::vec3 DjinnForm::local(const Particle& p) const
{
    const float t = m_time;
    switch (p.part)
    {
        case Part::Tail:
        {
            // A spiral of smoke out of the lamp, widening as it rises and
            // turning all the time.
            const float u = p.u;
            const float a = u * 2.0f * kTwoPi + p.phase * 0.3f + t * 1.6f;
            const float r = 0.10f + 0.55f * std::pow(u, 1.5f);
            return glm::vec3(std::cos(a) * r, 2.0f * u, std::sin(a) * r) + p.jitter * 0.07f;
        }
        case Part::Torso:
        {
            // Broad at the chest, narrowing into the tail; breathing.
            const float u = p.u;
            const float breathe = 1.0f + 0.03f * std::sin(t * 1.4f);
            // Broad shoulders: thinner than this and he read as a stick figure.
            const float rx = (0.60f + 0.65f * std::sin(3.14159f * u * 0.85f)) * breathe;
            const float rz = rx * 0.65f;
            const float a = p.v * kTwoPi;
            const float fill = 0.75f + 0.25f * p.jitter.x * p.jitter.x;
            return { std::cos(a) * rx * fill, 2.0f + 1.6f * u, std::sin(a) * rz * fill };
        }
        case Part::Head:
        {
            const float a = p.v * kTwoPi;
            const float c = 2.0f * p.u - 1.0f;              // even over the sphere
            const float s = std::sqrt(std::max(0.0f, 1.0f - c * c));
            const float r = 0.68f * (0.88f + 0.12f * std::fabs(p.jitter.y));
            return glm::vec3(0.0f, 4.25f, 0.0f) + glm::vec3(std::cos(a) * s, c, std::sin(a) * s) * r;
        }
        case Part::Turban:
        {
            const float u = p.u;
            const float r = 0.58f * (1.0f - u) + 0.04f;
            const float a = p.v * kTwoPi + t * 0.8f;
            return { std::cos(a) * r, 4.85f + 0.6f * u, std::sin(a) * r };
        }
        case Part::Beard:
        {
            const float u = p.u;
            const float r = 0.22f * (1.0f - u) + 0.02f;
            const float a = p.v * kTwoPi;
            return { std::cos(a) * r, 3.70f - 0.6f * u, 0.48f + std::sin(a) * r * 0.5f };
        }
        case Part::Eye:
        {
            return glm::vec3(0.24f * static_cast<float>(p.side), 4.34f, 0.62f) + p.jitter * 0.03f;
        }
        case Part::Arm:
        case Part::Hand:
        {
            const bool pointing = (p.side == m_pointSide);
            const glm::vec3 d = armDirection(pointing);
            const glm::vec3 s = shoulder(p.side);
            if (p.part == Part::Hand)
            {
                return s + d * (kArmLength + 0.12f) + p.jitter * 0.11f;
            }
            const float along = p.u * kArmLength;
            const float thick = 0.24f * (1.0f - 0.45f * p.u);   // tapering to the wrist
            return s + d * along + p.jitter * thick;
        }
    }
    return glm::vec3(0.0f);
}

glm::vec3 DjinnForm::toWorld(const glm::vec3& l) const
{
    // Ry(yaw), as the scene graph applies it: local +Z ends up facing yaw.
    const float a = glm::radians(m_yaw);
    const float c = std::cos(a), s = std::sin(a);
    const glm::vec3 v = l * kScale;
    return m_base + glm::vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

glm::vec3 DjinnForm::toLocalDirection(const glm::vec3& w) const
{
    const float a = glm::radians(-m_yaw);
    const float c = std::cos(a), s = std::sin(a);
    return { w.x * c + w.z * s, w.y, -w.x * s + w.z * c };
}

glm::vec3 DjinnForm::targetOf(int i) const
{
    const Particle& p = m_parts[i];
    // A shimmer, so the smoke never sits still.
    const glm::vec3 drift(std::sin(m_time * 2.7f + p.phase),
                          std::sin(m_time * 2.1f + p.phase * 1.7f),
                          std::sin(m_time * 2.4f + p.phase * 2.3f));
    const float amount = (p.part == Part::Eye) ? 0.0f : 0.04f;
    return toWorld(local(p) + drift * amount);
}

glm::vec3 DjinnForm::handPosition() const
{
    return toWorld(shoulder(m_pointSide) + armDirection(true) * (kArmLength + 0.12f));
}

glm::vec3 DjinnForm::chestPosition() const
{
    return toWorld({ 0.0f, 3.2f, 0.0f });
}

glm::vec3 DjinnForm::eyePosition(int eye) const
{
    return toWorld({ 0.24f * (eye == 0 ? -1.0f : 1.0f), 4.34f, 0.62f });
}

// -------------------------------------------------------------- controls ----

void DjinnForm::faceTowards(const glm::vec3& w, float deltaTime)
{
    const glm::vec3 d = w - m_base;
    if (d.x * d.x + d.z * d.z < 1e-4f) { return; }
    const float want = glm::degrees(std::atan2(d.x, d.z));
    if (!m_yawSet) { m_yaw = want; m_yawSet = true; return; }
    m_yaw += Easing::shortestAngleDelta(m_yaw, want) * std::min(1.0f, deltaTime * 1.5f);
}

void DjinnForm::setPointing(const glm::vec3& worldPoint, float amount)
{
    m_pointAt = worldPoint;
    m_point = Easing::clamp01(amount);

    // Point with whichever hand is on the target's side.
    const glm::vec3 l = toLocalDirection(worldPoint - m_base);
    m_pointSide = (l.x >= 0.0f) ? 1 : -1;
}

void DjinnForm::show(bool on)
{
    if (on == m_shown) { return; }
    m_shown = on;
    m_clock = 0.0f;

    for (Particle& p : m_parts)
    {
        const float height = Easing::clamp01(local(p).y / kHeight);
        if (on)
        {
            // Lowest first, so he builds up out of the lamp.
            p.releaseAt = kRiseTime * height + random(0.0f, 0.15f);
        }
        else
        {
            // Into the lamp, head last.
            p.returnAt = kSinkTime * height + random(0.0f, 0.15f);
        }
    }
}

void DjinnForm::hideNow()
{
    m_shown = false;
    m_clock = 100.0f;
    m_presence = 0.0f;
    m_yawSet = false;
    for (Particle& p : m_parts)
    {
        p.presence = 0.0f;
        p.pos = m_base;
        p.vel = glm::vec3(0.0f);
    }
}

// ---------------------------------------------------------------- update ----

void DjinnForm::update(float deltaTime, float time)
{
    m_time = time;
    m_clock += deltaTime;

    float total = 0.0f;
    for (int i = 0; i < count(); ++i)
    {
        Particle& p = m_parts[i];

        glm::vec3 goal;
        if (m_shown)
        {
            if (m_clock < p.releaseAt)
            {
                // Still in the lamp.
                p.pos = m_base + p.jitter * 0.05f;
                p.vel = glm::vec3(0.0f, 2.5f, 0.0f);
                p.presence = 0.0f;
                continue;
            }
            goal = targetOf(i);
            p.presence = std::min(1.0f, p.presence + deltaTime * 3.0f);
        }
        else
        {
            if (p.presence <= 0.0f) { p.pos = m_base; continue; }
            goal = (m_clock < p.returnAt) ? targetOf(i) : m_base;

            // Fade as it reaches the lamp's mouth.
            if (m_clock >= p.returnAt)
            {
                const float near = glm::length(p.pos - m_base);
                p.presence = std::min(p.presence, Easing::clamp01(near / 0.9f));
                p.presence = std::max(0.0f, p.presence - deltaTime * 0.6f);
            }
        }

        // A spring to its place, with a swirl round the lamp's axis that is
        // strong while it is on its way and gone once it has arrived.
        const glm::vec3 toGoal = goal - p.pos;
        glm::vec3 accel = toGoal * 16.0f - p.vel * 6.5f;
        const glm::vec3 radial(p.pos.x - m_base.x, 0.0f, p.pos.z - m_base.z);
        const glm::vec3 swirl(-radial.z, 0.0f, radial.x);
        accel += swirl * std::min(2.5f, glm::length(toGoal) * 1.5f);

        p.vel += accel * deltaTime;
        p.pos += p.vel * deltaTime;
        total += p.presence;
    }

    m_presence = count() > 0 ? total / static_cast<float>(count()) : 0.0f;
}

// ---------------------------------------------------------------- looks -----

glm::vec4 DjinnForm::color(int i) const
{
    const Particle& p = m_parts[i];
    if (p.part == Part::Eye)
    {
        return { 1.0f, 0.92f, 0.55f, p.presence };
    }

    // The Djinn's cyan, deepening to blue, with a slow pulse. Strong enough
    // to read against the lit sandstone: at a quarter of this he vanished.
    const glm::vec3 a(0.22f, 0.72f, 1.0f), b(0.08f, 0.32f, 0.92f);
    const float pulse = 0.85f + 0.15f * std::sin(m_time * 2.0f + p.phase);
    const glm::vec3 c = glm::mix(a, b, p.tint) * pulse;

    float alpha = 0.55f;
    if (p.part == Part::Tail)  { alpha = 0.42f; }
    if (p.part == Part::Head)  { alpha = 0.65f; }
    if (p.part == Part::Hand)  { alpha = 0.75f; }
    return { c.r, c.g, c.b, alpha * p.presence };
}

float DjinnForm::size(int i) const
{
    const Particle& p = m_parts[i];
    switch (p.part)
    {
        case Part::Eye:  return 0.20f;
        case Part::Tail: return (0.34f + 0.26f * p.u) * p.sizeJitter;
        case Part::Hand: return 0.30f * p.sizeJitter;
        case Part::Arm:  return 0.32f * p.sizeJitter;
        default:         return 0.42f * p.sizeJitter;
    }
}
