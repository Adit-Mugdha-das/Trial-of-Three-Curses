#ifndef GATEPUZZLE_H
#define GATEPUZZLE_H

#include <random>

#include <glm/glm.hpp>

// The constellation gate's puzzle, and the fireflies that give its answer.
//
// Three concentric rings, each turning in steps of 60 degrees. Each has a
// gold notch; the gate opens when every notch points where the fireflies
// said. The fireflies normally hover round the six sockets on the gate. Asked
// to show the pattern, they fly into a small constellation - one star on each
// ring where its notch must point, joined by lines of fireflies - hold it for
// a few seconds, and scatter back. Asking again costs time, and the
// sanctuary is running out.
//
// No GL here: the scene turns the rings, main draws the fireflies.
class GatePuzzle
{
public:
    static constexpr int   kRings = 3;
    static constexpr int   kSteps = 6;
    static constexpr float kStepDegrees = 60.0f;
    static constexpr int   kFlies = 17;     // 3 per star, 4 along each line

    enum class Show { Idle, Gathering, Holding, Dispersing };

    // Where the rings are: their shared centre (at the depth the fireflies
    // hover), the radius of each ring's visible band (0 = outer), and the
    // radius the six sockets sit on.
    void setLayout(const glm::vec3& centre, const float bandRadius[kRings],
                   float socketRadius);

    // A new pattern, every ring back to its start, and the swarm released
    // from `releaseFrom` (the charm) to find the sockets.
    void reset(unsigned seed, const glm::vec3& releaseFrom);

    void update(float deltaTime, float time);

    // --- input ---------------------------------------------------------------
    void select(int delta);     // -1 outward, +1 inward
    void rotate(int delta);     // +1 clockwise as you face the gate
    bool requestPattern();      // false if the fireflies are busy

    // --- state ---------------------------------------------------------------
    int   selected() const { return m_selected; }
    int   step(int ring) const { return m_step[ring]; }
    int   target(int ring) const { return m_target[ring]; }
    bool  ringMatches(int ring) const;

    // Degrees about Z for the scene. Positive turns clockwise as seen from
    // in front, which is the way +1 rotates it.
    float ringAngle(int ring) const { return m_angle[ring]; }

    bool  matched() const;                 // every notch on its mark, at rest
    bool  solved() const { return m_lockTime >= kLockTime; }
    float lockLevel() const;               // 0..1 as the rings lock

    Show  show() const { return m_show; }
    float showLevel() const;               // 0 scattered, 1 constellation formed
    int   patternShows() const { return m_shows; }

    glm::vec3 flyPosition(int i) const { return m_flyPos[i]; }
    float     flyGlow(int i) const { return m_flyGlow[i]; }

    // Where a ring's notch is when the ring stands at `stepIndex`.
    glm::vec3 notchPosition(int ring, int stepIndex) const;

    // The fewest presses from one step to another: -3..3.
    static int shortestTurn(int from, int to);

    static constexpr float kGatherTime   = 1.0f;
    static constexpr float kHoldTime     = 4.0f;
    static constexpr float kDisperseTime = 1.0f;
    static constexpr float kLockTime     = 0.8f;

private:
    glm::vec3 m_centre{ 0.0f };
    float m_band[kRings] = { 2.0f, 1.4f, 0.8f };
    float m_socketRadius = 2.9f;

    int   m_target[kRings] = { 0, 0, 0 };
    int   m_step[kRings]   = { 0, 0, 0 };   // not wrapped, so turning stays continuous
    float m_angle[kRings]  = { 0.0f, 0.0f, 0.0f };
    int   m_selected = 0;
    float m_lockTime = 0.0f;

    Show  m_show = Show::Idle;
    float m_showTime = 0.0f;
    int   m_shows = 0;

    glm::vec3 m_flyPos[kFlies];
    glm::vec3 m_flyVel[kFlies];
    float     m_flyGlow[kFlies];
    float     m_flyPhase[kFlies];

    std::mt19937 m_rng{ 1u };

    glm::vec3 onCircle(float radius, float degrees) const;
    glm::vec3 patternPlace(int fly) const;
    glm::vec3 restingPlace(int fly, float time) const;
};

#endif // GATEPUZZLE_H
