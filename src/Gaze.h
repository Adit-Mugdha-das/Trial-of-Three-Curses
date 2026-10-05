#ifndef GAZE_H
#define GAZE_H

#include <glm/glm.hpp>

// Medusa's gaze as a mechanic, not a decoration.
//
// An attack cycles through
//
//   Cooldown   she tracks him, eyes smouldering
//   Telegraph  eyes brighten and a thin tracer follows him, then winds back
//              toward one wall: the warning
//   Lock       the aim freezes and the beam goes solid
//   Sweep      the beam crosses the corridor at a constant angular speed
//
// and only Lock and Sweep can hurt. Exposure fills while he is inside the
// cone AND the eyes can actually see him; it drains whenever either stops
// being true. Full exposure is stone.
//
// "Can actually see him" is the point of the whole feature. Being inside the
// spotlight cone is not enough - a tall pillar between the eyes and his chest
// breaks the line, so lighting, geometry and his own movement are all part of
// the one rule. Low rubble does NOT count as cover.
//
// No GL in here, so the whole thing can be driven by a test.
class Gaze
{
public:
    enum class Phase { Dormant, Cooldown, Telegraph, Lock, Sweep };

    // A tall vertical cylinder. `height` is how far up it blocks the line.
    void addCover(float x, float z, float radius, float height);
    void clearCover();

    // The chamber's front wall: blocks any line crossing z = gateZ outside
    // the doorway, so she cannot gaze through solid stone.
    void setDoorway(float gateZ, float doorHalfWidth);

    void reset();

    // Attacks only begin while this is true. An attack already under way is
    // abandoned when it goes false.
    void setActive(bool active) { m_active = active; }
    bool active() const { return m_active; }

    // `eye` is her eyes in world space, `targetFeet` is where he stands.
    void update(const glm::vec3& eye, const glm::vec3& targetFeet, float deltaTime);

    // --- the mechanic ------------------------------------------------------
    float exposure() const { return m_exposure; }
    bool  caught() const { return m_exposure >= 1.0f; }
    bool  beamLive() const;          // Lock or Sweep: can petrify
    bool  exposedNow() const { return m_exposedNow; }
    float visibility() const { return m_visibility; }   // 0..1 of him the eyes can see
    Phase phase() const { return m_phase; }
    const char* phaseName() const;
    int   sweepSide() const { return m_side; }          // +1 = toward +x
    float phaseTime() const { return m_t; }             // seconds in this phase

    // 0..1: an attack is under way and he should be looking for cover.
    float warning() const;

    // --- for the scene -------------------------------------------------------
    glm::vec3 aimPoint() const { return m_aimPoint; }
    float eyeLevel() const { return m_eyeLevel; }       // 0..1 brightness
    float beamWidth() const { return m_beamWidth; }     // 0..1 of the full cone
    float beamLength() const { return m_beamLength; }   // stops at a pillar

    // How much of him the eyes can see from `eye`: 0, 1/3, 2/3 or 1. Three
    // sight lines (centre and both shoulders) rather than one, so a pillar
    // edge gives a partial result instead of flickering on and off.
    float visibilityOf(const glm::vec3& eye, const glm::vec3& targetFeet) const;

    // Is the straight line a -> b broken by a pillar or by the front wall?
    bool lineBlocked(const glm::vec3& a, const glm::vec3& b) const;

private:
    struct Cover
    {
        float x = 0.0f, z = 0.0f, radius = 0.0f, height = 0.0f;
    };

    static constexpr int kMaxCover = 16;
    Cover m_cover[kMaxCover];
    int   m_coverCount = 0;

    float m_gateZ = 0.0f;
    float m_doorHalfWidth = 0.0f;
    bool  m_hasDoorway = false;

    bool  m_active = false;
    Phase m_phase  = Phase::Dormant;
    float m_t      = 0.0f;           // time in the current phase
    float m_cooldownLength = 0.0f;
    int   m_side   = 1;
    int   m_attacks = 0;

    float m_yaw       = 0.0f;        // where the beam points, degrees
    float m_sweepFrom = 0.0f;

    float m_exposure   = 0.0f;
    bool  m_exposedNow = false;
    float m_visibility = 1.0f;

    glm::vec3 m_aimPoint{ 0.0f };
    float m_eyeLevel   = 0.3f;
    float m_beamWidth  = 0.0f;
    float m_beamLength = 0.0f;

    void enter(Phase next);
    void trackYaw(float desired, float deltaTime);

    // Where along a -> b (0..1) the first cover is hit, or 2 for none.
    static float coverHit(const Cover& c, const glm::vec3& a, const glm::vec3& b);
};

#endif // GAZE_H
