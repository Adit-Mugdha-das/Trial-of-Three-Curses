#ifndef DJINNFORM_H
#define DJINNFORM_H

#include <random>
#include <vector>

#include <glm/glm.hpp>

// The Djinn, made of smoke: a few hundred particles, each with its own place
// in a figure - a swirling tail out of the lamp, chest, head, turban, beard,
// two arms and two glowing eyes.
//
// Shown, they pour out of the lamp and fly to their places, lowest first, so
// he builds up out of the smoke. Hidden, they stream back down into the lamp,
// head last. One arm can swing round to point at something.
//
// No GL here: main draws the particles through ParticleSystem::addGlow.
class DjinnForm
{
public:
    void init(unsigned seed = 77u);

    // Where he rises from (the lamp's mouth), and which way he faces.
    void setBase(const glm::vec3& lampMouth) { m_base = lampMouth; }
    void faceTowards(const glm::vec3& worldPoint, float deltaTime);

    // 0 arms raised, 1 one arm pointing straight at `worldPoint`.
    void setPointing(const glm::vec3& worldPoint, float amount);

    void show(bool on);         // start rising / start returning to the lamp
    void hideNow();             // gone at once (a new run)

    void update(float deltaTime, float time);

    bool  visible() const { return m_presence > 0.01f; }
    float presence() const { return m_presence; }       // 0..1, overall
    bool  shown() const { return m_shown; }

    int       count() const { return static_cast<int>(m_parts.size()); }
    glm::vec3 position(int i) const { return m_parts[i].pos; }
    glm::vec4 color(int i) const;   // rgb and alpha
    float     size(int i) const;

    glm::vec3 handPosition() const;     // the pointing hand, in the world
    glm::vec3 chestPosition() const;
    glm::vec3 eyePosition(int eye) const;

    // Height of the figure above the lamp's mouth (before scaling), and the
    // scale it is drawn at: at full size his head was cut off by the frame.
    static constexpr float kHeight = 5.3f;
    static constexpr float kScale  = 0.8f;

    enum class Part { Tail, Torso, Head, Turban, Beard, Eye, Arm, Hand };

    // Where particle i belongs, in the world, right now.
    glm::vec3 targetOf(int i) const;
    Part      partOf(int i) const { return m_parts[i].part; }

private:
    struct Particle
    {
        Part  part = Part::Torso;
        int   side = 0;              // arms and eyes: -1 / +1
        bool  pointing = false;      // on the arm that points
        float u = 0.0f, v = 0.0f;    // where on its part
        glm::vec3 jitter{ 0.0f };
        float phase = 0.0f;
        float tint = 0.0f;
        float sizeJitter = 1.0f;

        float releaseAt = 0.0f;      // when it leaves the lamp
        float returnAt  = 0.0f;      // when it heads back in
        float presence  = 0.0f;
        glm::vec3 pos{ 0.0f };
        glm::vec3 vel{ 0.0f };
    };

    std::vector<Particle> m_parts;
    std::mt19937 m_rng{ 77u };

    glm::vec3 m_base{ 0.0f };
    float m_yaw = 0.0f;              // degrees; 0 faces +Z
    bool  m_yawSet = false;
    glm::vec3 m_pointAt{ 0.0f };
    float m_point = 0.0f;
    int   m_pointSide = 1;

    bool  m_shown = false;
    float m_clock = 0.0f;            // since the last show / hide
    float m_time = 0.0f;
    float m_presence = 0.0f;

    float random(float low, float high);
    glm::vec3 local(const Particle& p) const;
    glm::vec3 toWorld(const glm::vec3& localPoint) const;
    glm::vec3 toLocalDirection(const glm::vec3& worldDirection) const;
    glm::vec3 armDirection(bool pointing) const;
    glm::vec3 shoulder(int side) const;
};

#endif // DJINNFORM_H
