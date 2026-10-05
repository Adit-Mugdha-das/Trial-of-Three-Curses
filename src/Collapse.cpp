#include "Collapse.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"
#include "EscapeTuning.h"

namespace
{
    constexpr float kPi = 3.14159265f;

    // How fast a section is already moving down when it breaks off its hinge.
    constexpr float kBreakSpeed = 2.0f;

    // The bounce when it hits the floor.
    constexpr float kSettleTime   = 0.25f;
    constexpr float kSettleHeight = 0.10f;
}

// ------------------------------------------------------------------- layout --

std::vector<Collapse::Spec> Collapse::standardLayout()
{
    std::vector<Spec> layout;

    // --- the chamber: three blocks of the cornice on the front wall ---------
    // The cornice is a ledge running round the top of the chamber walls. The
    // three that fall are on the FRONT wall, either side of the doorway,
    // because that is what the chase camera is looking at while he runs for
    // the door. The side walls are out of frame for the whole run.
    //
    // Each hangs from its top edge against the wall, so its inner edge drops
    // first, and it lands leaning back against the wall it fell from - well
    // clear of the doorway, which spans |x| < 3.5.
    struct ChamberPiece { const char* name; float x; float restX; float restZ;
                          float tilt; float yaw; float startAt; };
    const ChamberPiece chamber[3] = {
        { "cornice, far left",      -10.89f, -10.60f, 9.95f, 16.0f, -8.0f, 0.00f },
        { "cornice, left of door",   -7.26f,  -7.00f, 9.90f, 18.0f,  6.0f, 0.12f },
        { "cornice, right of door",   7.26f,   7.40f, 9.95f, 15.0f, -5.0f, 0.24f },
    };

    for (const ChamberPiece& piece : chamber)
    {
        Spec spec;
        spec.name        = piece.name;
        spec.home        = { piece.x, 7.6f, 10.2f };
        spec.size        = { 3.58f, 0.7f, 2.0f };
        spec.hingeAlongZ = false;                        // tips about X
        spec.hinge       = { piece.x, 7.95f, 11.2f };    // top edge, at the wall
        spec.rest        = { piece.restX, piece.restZ };
        spec.restTilt    = piece.tilt;
        spec.restYaw     = piece.yaw;
        spec.startAt     = piece.startAt;
        spec.crackTime   = Tuning::kChamberCrackTime;
        spec.shakeTime   = Tuning::kChamberShakeTime;
        layout.push_back(spec);
    }

    // --- the corridor: three ceiling beams that break in two ----------------
    // One half falls and lands across its side of the corridor, leaning on
    // the wall; the other half stays up, so every fall leaves an open lane.
    //
    // Each falls on the LEFT, because the pillar just before each of these
    // beams (27, 41, 55) stands on the right - and that is where he hides
    // from Medusa's gaze. A block landing on the far side of his hiding
    // place would trap him in it; on this side it leaves his way on open.
    // (The first layout alternated sides and the cover-using bot went from
    // escaping every run to escaping none.)
    //
    // Corridor torch i hangs at z = gateZ + 7 + 12i, on the left when i is
    // even. The blast of dust from the beam at 30.5 puts out torch 1 across
    // the way; the beam at 44.5 lands right beside torch 2 and smashes it;
    // the last only makes torch 3 gutter.
    struct Beam { const char* name; float z; float side; int torch; bool snuffs; };
    const Beam beams[3] = {
        { "beam at 30.5, left half", 30.5f, -1.0f, Collapse::kChamberTorches + 1, true  },
        { "beam at 44.5, left half", 44.5f, -1.0f, Collapse::kChamberTorches + 2, true  },
        { "beam at 58.5, left half", 58.5f, -1.0f, Collapse::kChamberTorches + 3, false },
    };

    for (const Beam& beam : beams)
    {
        Spec spec;
        spec.name        = beam.name;
        spec.home        = { beam.side * 2.0f, 6.7f, beam.z };
        spec.size        = { 3.98f, 0.6f, 1.0f };
        spec.hingeAlongZ = true;                                  // tips about Z
        spec.hinge       = { beam.side * 4.0f, 7.0f, beam.z };    // at the wall
        spec.rest        = { beam.side * 1.95f, beam.z };
        spec.restTilt    = 14.0f;
        spec.restYaw     = 0.0f;
        spec.startAt     = -1.0f;
        spec.triggerZ    = beam.z - Tuning::kSectionTriggerAhead;
        spec.crackTime   = Tuning::kCorridorCrackTime;
        spec.shakeTime   = Tuning::kCorridorShakeTime;
        spec.torch       = beam.torch;
        spec.snuffs      = beam.snuffs;

        spec.hasCompanion  = true;
        spec.companionHome = { -beam.side * 2.0f, 6.7f, beam.z };
        spec.companionSize = spec.size;

        layout.push_back(spec);
    }

    return layout;
}

// ------------------------------------------------------------------ helpers --

float Collapse::randomRange(float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    return dist(m_rng);
}

const char* Collapse::stageName(Stage stage)
{
    switch (stage)
    {
        case Stage::Intact:    return "intact";
        case Stage::Cracking:  return "cracking";
        case Stage::Shaking:   return "shaking";
        case Stage::Detaching: return "detaching";
        case Stage::Falling:   return "falling";
        case Stage::Down:      return "down";
    }
    return "?";
}

glm::vec3 Collapse::aboutHinge(const Spec& spec, const glm::vec3& p, float degrees) const
{
    const float a = glm::radians(degrees);
    const float c = std::cos(a);
    const float s = std::sin(a);
    const glm::vec3 r = p - spec.hinge;

    // The same senses as the scene graph's Rz and Rx, so the box turns
    // exactly the way its centre swings.
    if (spec.hingeAlongZ)
    {
        return spec.hinge + glm::vec3(r.x * c - r.y * s, r.x * s + r.y * c, r.z);
    }
    return spec.hinge + glm::vec3(r.x, r.y * c - r.z * s, r.y * s + r.z * c);
}

float Collapse::restHeight(const Spec& spec, float tiltDegrees)
{
    const float a = glm::radians(tiltDegrees);
    const glm::vec3 h = spec.size * 0.5f;
    const float across = spec.hingeAlongZ ? h.x : h.z;
    return across * std::fabs(std::sin(a)) + h.y * std::fabs(std::cos(a));
}

glm::vec3 Collapse::eulerFor(const Spec& spec, float tilt, float yaw) const
{
    return spec.hingeAlongZ ? glm::vec3(0.0f, yaw, tilt)
                            : glm::vec3(tilt, yaw, 0.0f);
}

// ----------------------------------------------------------------- set-up ----

int Collapse::add(const Spec& spec)
{
    Section section;
    section.spec = spec;

    // Whichever way round lowers its centre is the way it tips. Worked out
    // rather than written into each spec, where it would be easy to get
    // backwards and have a block swing up into the ceiling.
    const glm::vec3 tipped = aboutHinge(spec, spec.home, 1.0f);
    section.sign = (tipped.y < spec.home.y) ? 1.0f : -1.0f;

    m_sections.push_back(section);
    const int index = static_cast<int>(m_sections.size()) - 1;
    pose(m_sections.back(), index);
    return index;
}

void Collapse::reset()
{
    for (int i = 0; i < count(); ++i)
    {
        Section& s = m_sections[i];
        s.stage = Stage::Intact;
        s.t = 0.0f;
        s.dustTimer = 0.0f;
        pose(s, i);
    }

    m_events.clear();
    m_begun = false;
    m_triggering = false;
    m_clock = 0.0f;
    m_shake = 0.0f;
    m_tremor = 0.0f;
    for (float& out : m_torchOut) { out = -1.0f; }
}

void Collapse::begin()
{
    m_begun = true;
    m_clock = 0.0f;
}

std::vector<Collapse::Circle> Collapse::footprint(int i) const
{
    const Spec& spec = m_sections[i].spec;
    const float a = glm::radians(spec.restTilt);
    const glm::vec3 h = spec.size * 0.5f;

    // Its outline on the floor once it is lying at its resting tilt.
    float ex = h.x;
    float ez = h.z;
    if (spec.hingeAlongZ) { ex = h.x * std::cos(a) + h.y * std::sin(a); }
    else                  { ez = h.z * std::cos(a) + h.y * std::sin(a); }

    // A row of circles down its long axis, overlapping so there is no gap to
    // squeeze through between them.
    const bool  alongX = ex >= ez;
    const float lengthHalf = alongX ? ex : ez;
    const float radius = std::min(ex, ez) + 0.05f;

    int n = 1;
    if (lengthHalf > radius)
    {
        n = 1 + static_cast<int>(std::ceil(2.0f * (lengthHalf - radius) / (1.2f * radius)));
    }

    const float yaw = glm::radians(spec.restYaw);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);

    std::vector<Circle> circles;
    for (int k = 0; k < n; ++k)
    {
        const float u = (n == 1) ? 0.0f
                      : -1.0f + 2.0f * static_cast<float>(k) / static_cast<float>(n - 1);
        const float along = u * (lengthHalf - radius);

        float ox = alongX ? along : 0.0f;
        float oz = alongX ? 0.0f  : along;

        // Ry, as the scene graph applies it.
        const float rx = ox * cy + oz * sy;
        const float rz = -ox * sy + oz * cy;

        circles.push_back({ spec.rest.x + rx, spec.rest.y + rz, radius });
    }
    return circles;
}

bool Collapse::popEvent(Event& out)
{
    if (m_events.empty())
    {
        return false;
    }
    out = m_events.front();
    m_events.erase(m_events.begin());
    return true;
}

// ------------------------------------------------------------------- update --

void Collapse::enter(Section& s, Stage next)
{
    s.stage = next;
    s.t = 0.0f;
    s.dustTimer = 0.0f;
}

void Collapse::update(float deltaTime, const glm::vec3& player)
{
    m_shake = std::max(0.0f, m_shake - deltaTime * 2.4f);
    if (m_begun)
    {
        m_clock += deltaTime;
    }

    bool chamberRumbling = false;

    for (int i = 0; i < count(); ++i)
    {
        Section& s = m_sections[i];
        const Spec& spec = s.spec;
        s.t += deltaTime;

        const glm::vec3 underside = spec.home - glm::vec3(0.0f, spec.size.y * 0.5f + 0.05f, 0.0f);
        const glm::vec3 spread(spec.size.x * 0.4f, 0.05f, spec.size.z * 0.4f);

        const float dx = spec.rest.x - player.x;
        const float dz = spec.rest.y - player.z;
        const float nearness = Easing::clamp01(1.0f - std::sqrt(dx * dx + dz * dz) / 18.0f);

        switch (s.stage)
        {
            case Stage::Intact:
            {
                const bool timed     = spec.startAt >= 0.0f;
                const bool timeUp    = timed && m_begun && m_clock >= spec.startAt;
                const bool passedBy  = !timed && m_begun && m_triggering
                                     && player.z >= spec.triggerZ;
                if (timeUp || passedBy)
                {
                    enter(s, Stage::Cracking);

                    Event e;
                    e.type = Event::Type::Crack;
                    e.section = i;
                    e.at = underside;
                    e.extent = spread;
                    e.count = 16;
                    m_events.push_back(e);
                }
                break;
            }

            case Stage::Cracking:
            case Stage::Shaking:
            {
                // Dust trickles out of the cracks, faster once it is moving.
                const bool shaking = (s.stage == Stage::Shaking);
                s.dustTimer -= deltaTime;
                if (s.dustTimer <= 0.0f)
                {
                    s.dustTimer = shaking ? 0.06f : 0.14f;

                    Event e;
                    e.type = Event::Type::Dust;
                    e.section = i;
                    e.at = underside;
                    e.extent = spread;
                    e.count = shaking ? 8 : 5;
                    m_events.push_back(e);
                }

                if (shaking)
                {
                    // A low rumble through the camera, felt more the closer
                    // he is.
                    m_shake = std::max(m_shake, 0.18f * nearness);
                }

                const float length = shaking ? spec.shakeTime : spec.crackTime;
                if (s.t >= length)
                {
                    if (!shaking)
                    {
                        enter(s, Stage::Shaking);
                    }
                    else
                    {
                        enter(s, Stage::Detaching);

                        // Chunks break off the free edge as it lets go.
                        Event e;
                        e.type = Event::Type::Detach;
                        e.section = i;
                        e.at = underside;
                        e.extent = spread;
                        e.count = 3;
                        m_events.push_back(e);
                    }
                }
                break;
            }

            case Stage::Detaching:
            {
                if (s.t >= Tuning::kSectionDetachTime)
                {
                    s.detachEnd = aboutHinge(spec, spec.home,
                                             s.sign * Tuning::kSectionDetachAngle);

                    // How long the drop takes, from the height it breaks
                    // free at to the height it rests at.
                    const float drop = std::max(0.1f, s.detachEnd.y
                                                      - restHeight(spec, spec.restTilt));
                    const float g = Tuning::kSectionGravity;
                    s.fallTime = (-kBreakSpeed
                                  + std::sqrt(kBreakSpeed * kBreakSpeed + 2.0f * g * drop)) / g;

                    enter(s, Stage::Falling);
                }
                break;
            }

            case Stage::Falling:
            {
                if (s.t >= s.fallTime)
                {
                    enter(s, Stage::Down);

                    // Did it come down on him?
                    bool hit = false;
                    for (const Circle& c : footprint(i))
                    {
                        const float cx = player.x - c.x;
                        const float cz = player.z - c.z;
                        const float reach = c.radius + Tuning::kPlayerRadius;
                        if (cx * cx + cz * cz < reach * reach) { hit = true; }
                    }

                    Event e;
                    e.type = Event::Type::Land;
                    e.section = i;
                    e.at = { spec.rest.x, 0.3f, spec.rest.y };
                    e.extent = { spec.size.x * 0.5f, 0.1f, spec.size.z * 0.5f };
                    e.count = 3;
                    e.hitPlayer = hit;
                    m_events.push_back(e);

                    m_shake = std::max(m_shake, 0.25f + 0.75f * nearness);

                    if (spec.snuffs && spec.torch >= 0 && spec.torch < kMaxTorches
                        && m_torchOut[spec.torch] < 0.0f)
                    {
                        m_torchOut[spec.torch] = 0.0f;

                        Event snuff;
                        snuff.type = Event::Type::Snuff;
                        snuff.section = i;
                        snuff.torch = spec.torch;
                        m_events.push_back(snuff);
                    }
                }
                break;
            }

            case Stage::Down:
                break;
        }

        // The chamber trembles as a whole while any of its own sections is
        // on the move, and for a moment after the last one lands.
        if (spec.startAt >= 0.0f)
        {
            const bool moving = s.stage == Stage::Cracking || s.stage == Stage::Shaking
                             || s.stage == Stage::Detaching || s.stage == Stage::Falling;
            const bool justLanded = s.stage == Stage::Down && s.t < 1.0f;
            if (moving || justLanded) { chamberRumbling = true; }
        }

        pose(s, i);
    }

    // Torches that have gone out keep counting, for the dying sputter.
    for (float& out : m_torchOut)
    {
        if (out >= 0.0f) { out += deltaTime; }
    }

    const float target = chamberRumbling ? 1.0f : 0.0f;
    m_tremor += (target - m_tremor) * std::min(1.0f, deltaTime * 4.0f);
}

// -------------------------------------------------------------------- poses --

void Collapse::pose(Section& s, int index)
{
    const Spec& spec = s.spec;

    // Each section shudders out of step with the others.
    const float phase = 1.7f * static_cast<float>(index) + 0.4f;
    auto jitter = [&](float amount)
    {
        const float t = s.t;
        return glm::vec3(std::sin(t * 53.0f + phase),
                         0.5f * std::sin(t * 61.0f + phase * 2.0f),
                         std::sin(t * 47.0f + phase * 3.0f)) * amount;
    };

    s.companionPosition = spec.companionHome;
    s.companionRotation = glm::vec3(0.0f);

    switch (s.stage)
    {
        case Stage::Intact:
            s.position = spec.home;
            s.rotation = glm::vec3(0.0f);
            s.crack = 0.0f;
            break;

        case Stage::Cracking:
        {
            s.crack = Easing::clamp01(s.t / std::max(0.01f, spec.crackTime));
            s.position = spec.home + jitter(0.008f * s.crack);
            s.rotation = glm::vec3(0.0f);
            s.companionPosition = spec.companionHome + jitter(0.004f * s.crack);
            break;
        }

        case Stage::Shaking:
        {
            s.crack = 1.0f;
            const float u = Easing::clamp01(s.t / std::max(0.01f, spec.shakeTime));
            const float amount = 0.04f + 0.05f * u;
            s.position = spec.home + jitter(amount);

            const float wobble = s.sign * 1.8f * (0.4f + 0.6f * u)
                               * std::sin(s.t * 41.0f + phase);
            s.rotation = eulerFor(spec, wobble, 0.6f * std::sin(s.t * 37.0f + phase));

            s.companionPosition = spec.companionHome + jitter(amount * 0.4f);
            break;
        }

        case Stage::Detaching:
        {
            s.crack = 1.0f;
            const float u = Easing::clamp01(s.t / Tuning::kSectionDetachTime);

            // Accelerating, as anything pivoting under its own weight does.
            const float angle = s.sign * Tuning::kSectionDetachAngle * u * u;
            s.position = aboutHinge(spec, spec.home, angle);
            s.rotation = eulerFor(spec, angle, 0.0f);

            s.companionPosition = spec.companionHome + jitter(0.03f * (1.0f - u));
            break;
        }

        case Stage::Falling:
        {
            s.crack = 1.0f;
            const float t = s.t;
            const float u = Easing::clamp01(t / std::max(0.01f, s.fallTime));

            const float g = Tuning::kSectionGravity;
            float y = s.detachEnd.y - kBreakSpeed * t - 0.5f * g * t * t;

            const float x = Easing::mix(s.detachEnd.x, spec.rest.x, u);
            const float z = Easing::mix(s.detachEnd.z, spec.rest.y, u);

            // It keeps turning after it breaks free, then slaps down onto its
            // resting angle - one end hits the floor first.
            const float tilt = Tuning::kSectionDetachAngle * (1.0f - u)
                             + spec.restTilt * u
                             + 26.0f * std::sin(kPi * u) * (1.0f - 0.3f * u);
            const float angle = s.sign * tilt;

            // Never through the floor, whatever angle it is at on the way.
            y = std::max(y, restHeight(spec, angle));

            s.position = { x, y, z };
            s.rotation = eulerFor(spec, angle, spec.restYaw * Easing::smoothstep01(u));
            break;
        }

        case Stage::Down:
        {
            s.crack = 1.0f;
            const float angle = s.sign * spec.restTilt;
            float y = restHeight(spec, angle);

            // One small bounce, so it lands with weight rather than stopping
            // dead.
            if (s.t < kSettleTime)
            {
                y += kSettleHeight * std::sin(kPi * s.t / kSettleTime);
            }

            s.position = { spec.rest.x, y, spec.rest.y };
            s.rotation = eulerFor(spec, angle, spec.restYaw);
            break;
        }
    }
}

// ------------------------------------------------------------------ torches --

float Collapse::torchLevel(int torch, float time) const
{
    if (torch < 0 || torch >= kMaxTorches)
    {
        return 1.0f;
    }

    const float id = static_cast<float>(torch);

    float alive = 1.0f;
    if (m_torchOut[torch] >= 0.0f)
    {
        // Half a second of sputtering, then nothing.
        constexpr float kDying = 0.5f;
        const float f = m_torchOut[torch] / kDying;
        if (f >= 1.0f)
        {
            return 0.0f;
        }
        alive = (1.0f - f) * (0.55f + 0.45f * std::sin(time * 47.0f + id));
    }

    float amount = 0.0f;

    if (torch < kChamberTorches)
    {
        amount = std::max(amount, 0.75f * m_tremor);
    }

    for (const Section& s : m_sections)
    {
        if (s.spec.torch != torch) { continue; }

        switch (s.stage)
        {
            case Stage::Cracking:
                amount = std::max(amount, 0.35f);
                break;
            case Stage::Shaking:
            case Stage::Detaching:
            case Stage::Falling:
                amount = std::max(amount, 0.85f);
                break;
            case Stage::Down:
                if (!s.spec.snuffs && s.t < 2.0f)
                {
                    amount = std::max(amount, 0.85f * (1.0f - s.t / 2.0f));
                }
                break;
            case Stage::Intact:
                break;
        }
    }

    // Two sines at unrelated rates multiplied: irregular dips, never a pulse.
    const float flicker = 0.5f + 0.5f * std::sin(time * 29.0f + id * 1.7f)
                                      * std::sin(time * 11.3f + id * 4.1f);

    return Easing::clamp01(alive * (1.0f - amount * flicker));
}
