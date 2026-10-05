#ifndef COLLAPSE_H
#define COLLAPSE_H

#include <random>
#include <vector>

#include <glm/glm.hpp>

// Sections of ceiling that break away during the escape, so the collapse
// reads as the building itself failing rather than stones appearing from
// nowhere.
//
// Each section runs
//
//     Intact -> Cracking -> Shaking -> Detaching -> Falling -> Down
//
// Cracking   cracks spread across it and dust trickles out
// Shaking    it shudders in place
// Detaching  it tips about the edge it hangs from, like a hinge
// Falling    it breaks free, turns over in the air and lands
// Down       it lies where it fell, and is solid
//
// No GL here, like Gaze: the scene reads the poses, main reads the events and
// turns them into dust, chunks, camera shake and obstacles.
class Collapse
{
public:
    enum class Stage { Intact, Cracking, Shaking, Detaching, Falling, Down };

    // Torch ids: 0 and 1 are the chamber's two, 2 + i is corridor torch i.
    static constexpr int kChamberTorches = 2;
    static constexpr int kMaxTorches     = 8;

    struct Spec
    {
        const char* name = "";

        glm::vec3 home{ 0.0f };      // centre of the box while in the ceiling
        glm::vec3 size{ 1.0f };      // full size of the box

        // The edge it hangs from while it tips: a line through `hinge`,
        // running along Z (so it tips about Z) or along X.
        bool      hingeAlongZ = true;
        glm::vec3 hinge{ 0.0f };

        // Where it comes to rest. The height is worked out, so it sits on
        // the floor whatever angle it ends at.
        glm::vec2 rest{ 0.0f };      // x, z
        float     restTilt = 14.0f;  // degrees about the hinge axis, same way it tipped
        float     restYaw  = 0.0f;

        // When it starts: a fixed time after the collapse begins, or - if
        // startAt is negative - when the traveller passes triggerZ.
        float startAt  = -1.0f;
        float triggerZ = 0.0f;

        float crackTime = 0.5f;
        float shakeTime = 0.4f;

        int  torch  = -1;            // the torch it disturbs
        bool snuffs = false;         // and puts out when it lands

        // The other half of a beam that breaks in two: it cracks and shudders
        // with this one but stays up.
        bool      hasCompanion = false;
        glm::vec3 companionHome{ 0.0f };
        glm::vec3 companionSize{ 1.0f };
    };

    struct Event
    {
        enum class Type { Crack, Dust, Detach, Land, Snuff };

        Type      type = Type::Dust;
        int       section = -1;
        glm::vec3 at{ 0.0f };
        glm::vec3 extent{ 0.0f };    // half size, for spreading dust
        int       count = 0;
        bool      hitPlayer = false; // Land: it came down on him
        int       torch = -1;        // Snuff
    };

    // A circle on the floor, for collision once a section is down.
    struct Circle
    {
        float x = 0.0f, z = 0.0f, radius = 0.0f;
    };

    // The layout used by the game: three pieces of the chamber's cornice and
    // three corridor beams. GL-free, so the scene, main and the tests all
    // build from the same list.
    static std::vector<Spec> standardLayout();

    int  add(const Spec& spec);
    int  count() const { return static_cast<int>(m_sections.size()); }
    const Spec& spec(int i) const { return m_sections[i].spec; }

    void reset();                 // everything back up, torches lit
    void begin();                 // the treasure was taken
    bool begun() const { return m_begun; }

    // Corridor sections only start while this is on (during the escape).
    void setTriggering(bool on) { m_triggering = on; }

    void update(float deltaTime, const glm::vec3& player);

    Stage     stage(int i) const { return m_sections[i].stage; }
    glm::vec3 position(int i) const { return m_sections[i].position; }
    glm::vec3 rotation(int i) const { return m_sections[i].rotation; }   // degrees
    float     crack(int i) const { return m_sections[i].crack; }         // 0..1
    glm::vec3 companionPosition(int i) const { return m_sections[i].companionPosition; }
    glm::vec3 companionRotation(int i) const { return m_sections[i].companionRotation; }

    // The floor circles a section occupies once it is down. Known up front,
    // so they can be registered at start-up and simply switched on.
    std::vector<Circle> footprint(int i) const;

    bool popEvent(Event& out);

    float shake() const { return m_shake; }

    // 0..1: the chamber's own sections are on the move (or only just down).
    float tremor() const { return m_tremor; }

    // 0 = out, 1 = burning normally; in between while it gutters.
    float torchLevel(int torch, float time) const;

    static const char* stageName(Stage stage);

private:
    struct Section
    {
        Spec  spec;
        Stage stage = Stage::Intact;
        float t = 0.0f;               // seconds in this stage
        float sign = 1.0f;            // which way round it tips
        float fallTime = 1.0f;
        float dustTimer = 0.0f;

        glm::vec3 detachEnd{ 0.0f };  // centre when it breaks free

        glm::vec3 position{ 0.0f };
        glm::vec3 rotation{ 0.0f };
        float     crack = 0.0f;

        glm::vec3 companionPosition{ 0.0f };
        glm::vec3 companionRotation{ 0.0f };
    };

    std::vector<Section> m_sections;
    std::vector<Event>   m_events;

    bool  m_begun = false;
    bool  m_triggering = false;
    float m_clock = 0.0f;
    float m_shake = 0.0f;
    float m_tremor = 0.0f;           // the chamber shaking as a whole, 0..1
    float m_torchOut[kMaxTorches];   // seconds since it went out, < 0 = lit

    std::mt19937 m_rng{ 4242u };
    float randomRange(float low, float high);

    void enter(Section& s, Stage next);
    void pose(Section& s, int index);

    // Rotates `p` about the section's hinge line by `degrees`.
    glm::vec3 aboutHinge(const Spec& spec, const glm::vec3& p, float degrees) const;

    // Half the height of the box at this tilt: how high its centre must be
    // for it to sit on the floor.
    static float restHeight(const Spec& spec, float tiltDegrees);

    glm::vec3 eulerFor(const Spec& spec, float tilt, float yaw) const;
};

#endif // COLLAPSE_H
