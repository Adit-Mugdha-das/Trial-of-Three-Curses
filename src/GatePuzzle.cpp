#include "GatePuzzle.h"

#include <algorithm>
#include <cmath>

#include "Easing.h"

namespace
{
    int wrapStep(int s)
    {
        const int n = GatePuzzle::kSteps;
        return ((s % n) + n) % n;
    }
}

void GatePuzzle::setLayout(const glm::vec3& centre, const float bandRadius[kRings],
                           float socketRadius)
{
    m_centre = centre;
    for (int k = 0; k < kRings; ++k) { m_band[k] = bandRadius[k]; }
    m_socketRadius = socketRadius;
}

glm::vec3 GatePuzzle::onCircle(float radius, float degrees) const
{
    // The same sense as the scene's rotation about Z: step 0 is straight up,
    // and positive angles go clockwise as you face the gate (towards -X).
    const float a = glm::radians(degrees);
    return m_centre + glm::vec3(-std::sin(a) * radius, std::cos(a) * radius, 0.0f);
}

glm::vec3 GatePuzzle::notchPosition(int ring, int stepIndex) const
{
    return onCircle(m_band[ring], kStepDegrees * static_cast<float>(stepIndex));
}

int GatePuzzle::shortestTurn(int from, int to)
{
    const int d = wrapStep(to - from);
    return (d <= kSteps / 2) ? d : d - kSteps;
}

void GatePuzzle::reset(unsigned seed, const glm::vec3& releaseFrom)
{
    m_rng.seed(seed);
    std::uniform_int_distribution<int> pick(1, kSteps - 1);

    // Never 0, so every ring has to be turned; and never all three the same,
    // or the constellation is just a straight line.
    for (int k = 0; k < kRings; ++k) { m_target[k] = pick(m_rng); }
    if (m_target[0] == m_target[1] && m_target[1] == m_target[2])
    {
        m_target[1] = 1 + (m_target[1] + 1) % (kSteps - 1);
    }

    for (int k = 0; k < kRings; ++k)
    {
        m_step[k] = 0;
        m_angle[k] = 0.0f;
    }
    m_selected = 0;
    m_lockTime = 0.0f;

    m_show = Show::Idle;
    m_showTime = 0.0f;
    m_shows = 0;

    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    for (int i = 0; i < kFlies; ++i)
    {
        m_flyPos[i]  = releaseFrom + glm::vec3(spread(m_rng), spread(m_rng), spread(m_rng)) * 0.3f;
        m_flyVel[i]  = glm::vec3(spread(m_rng), spread(m_rng), spread(m_rng)) * 3.0f;
        m_flyGlow[i] = 0.5f;
        m_flyPhase[i] = 6.2831853f * static_cast<float>(i) / static_cast<float>(kFlies);
    }
}

// --------------------------------------------------------------------- input --

void GatePuzzle::select(int delta)
{
    if (solved()) { return; }
    m_selected = std::clamp(m_selected + delta, 0, kRings - 1);
}

void GatePuzzle::rotate(int delta)
{
    if (solved()) { return; }
    m_step[m_selected] += delta;
}

bool GatePuzzle::requestPattern()
{
    if (m_show != Show::Idle || solved())
    {
        return false;
    }
    m_show = Show::Gathering;
    m_showTime = 0.0f;
    ++m_shows;
    return true;
}

// ------------------------------------------------------------------- queries --

bool GatePuzzle::ringMatches(int ring) const
{
    return wrapStep(m_step[ring]) == m_target[ring];
}

bool GatePuzzle::matched() const
{
    for (int k = 0; k < kRings; ++k)
    {
        if (!ringMatches(k)) { return false; }
        const float rest = kStepDegrees * static_cast<float>(m_step[k]);
        if (std::fabs(m_angle[k] - rest) > 1.0f) { return false; }   // still turning
    }
    return true;
}

float GatePuzzle::lockLevel() const
{
    return Easing::clamp01(m_lockTime / kLockTime);
}

float GatePuzzle::showLevel() const
{
    switch (m_show)
    {
        case Show::Gathering:  return Easing::clamp01(m_showTime / kGatherTime);
        case Show::Holding:    return 1.0f;
        case Show::Dispersing: return 1.0f - Easing::clamp01(m_showTime / kDisperseTime);
        case Show::Idle:       break;
    }
    return 0.0f;
}

// --------------------------------------------------------------- the swarm ----

glm::vec3 GatePuzzle::patternPlace(int fly) const
{
    // Flies 0-8: three to a star, one star per ring at its answer.
    if (fly < 9)
    {
        const int ring = fly / 3;
        const float a = glm::radians(120.0f * static_cast<float>(fly % 3));
        const glm::vec3 star = notchPosition(ring, m_target[ring]);
        return star + glm::vec3(std::cos(a), std::sin(a), 0.0f) * 0.13f;
    }

    // Flies 9-16: four along each line joining the stars, outer to middle
    // then middle to inner - the constellation's drawing.
    const int line = (fly - 9) / 4;              // 0: outer-middle, 1: middle-inner
    const float t = 0.2f * static_cast<float>((fly - 9) % 4 + 1);
    const glm::vec3 a = notchPosition(line,     m_target[line]);
    const glm::vec3 b = notchPosition(line + 1, m_target[line + 1]);
    return glm::mix(a, b, t);
}

glm::vec3 GatePuzzle::restingPlace(int fly, float time) const
{
    // A slow little loop round one of the six sockets.
    const glm::vec3 socket = onCircle(m_socketRadius, kStepDegrees * static_cast<float>(fly % kSteps));
    const float p = m_flyPhase[fly];
    return socket + glm::vec3(std::cos(time * 1.3f + p) * 0.28f,
                              std::sin(time * 1.7f + p * 1.3f) * 0.22f,
                              std::sin(time * 0.9f + p) * 0.15f);
}

// -------------------------------------------------------------------- update --

void GatePuzzle::update(float deltaTime, float time)
{
    // --- the rings: each eases onto its step, so a press is a smooth turn ---
    for (int k = 0; k < kRings; ++k)
    {
        const float goal = kStepDegrees * static_cast<float>(m_step[k]);
        m_angle[k] += (goal - m_angle[k]) * std::min(1.0f, deltaTime * 10.0f);
        if (std::fabs(goal - m_angle[k]) < 0.05f) { m_angle[k] = goal; }
    }

    // --- the lock: all three on their marks, held a moment -------------------
    if (matched()) { m_lockTime += deltaTime; }
    else           { m_lockTime = 0.0f; }

    // --- the showing ------------------------------------------------------------
    m_showTime += deltaTime;
    if (m_show == Show::Gathering && m_showTime >= kGatherTime)
    {
        m_show = Show::Holding;  m_showTime = 0.0f;
    }
    else if (m_show == Show::Holding && m_showTime >= kHoldTime)
    {
        m_show = Show::Dispersing;  m_showTime = 0.0f;
    }
    else if (m_show == Show::Dispersing && m_showTime >= kDisperseTime)
    {
        m_show = Show::Idle;  m_showTime = 0.0f;
    }

    // Once it is solved they gather into the constellation for good - the
    // answer made real.
    const float formed = solved() ? 1.0f : Easing::smoothstep01(showLevel());

    // --- each firefly: a stiff spring to where it should be ------------------
    for (int i = 0; i < kFlies; ++i)
    {
        const glm::vec3 want = glm::mix(restingPlace(i, time), patternPlace(i), formed);

        const glm::vec3 accel = (want - m_flyPos[i]) * 22.0f - m_flyVel[i] * 7.0f;
        m_flyVel[i] += accel * deltaTime;
        m_flyPos[i] += m_flyVel[i] * deltaTime;

        // Dim and lazy at rest; bright in the pattern, the stars brightest.
        const float flicker = 0.5f + 0.5f * std::sin(time * 2.3f + m_flyPhase[i] * 3.0f);
        const float resting = 0.25f + 0.25f * flicker;
        const float shown   = (i < 9) ? 1.0f : 0.75f;
        m_flyGlow[i] = Easing::mix(resting, shown, formed);
    }
}
