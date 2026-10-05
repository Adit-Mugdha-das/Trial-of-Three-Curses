#ifndef DEBRIS_H
#define DEBRIS_H

#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "Material.h"
#include "Mesh.h"
#include "Shader.h"

class Player;

// Stones falling from the corridor ceiling during the collapse.
//
// A fixed pool, recycled, exactly like ParticleSystem - nothing is allocated
// once the chase is running. Each stone falls under gravity, lands, briefly
// blocks the corridor, then crumbles away.
//
// Every falling stone paints a warning ring on the floor beneath it. Without
// that the hits feel arbitrary rather than avoidable, and an unavoidable
// hazard in a chase is just a random loss.
class Debris
{
public:
    // Splitting these lets the falling / hitting / landing logic be tested
    // without a GL context; only initMeshes needs one.
    void initPool(int maxStones);
    bool init(int maxStones);

    // Exposed for testing: how many stones are in flight or settled.
    int activeCount() const;
    void shutdown();

    void reset();
    void setActive(bool active) { m_active = active; }
    bool active() const { return m_active; }

    // Scales the spawn rate. The verdict feeds this: a false heart brings
    // the chamber down harder.
    void setIntensity(float intensity) { m_intensity = intensity; }

    // Confines spawning to the corridor.
    void setCorridor(float gateZ, float halfWidth, float exitZ, float ceilingY);

    void setMaterial(const Material& material) { m_material = material; }

    // Keeps stones from landing inside something solid, such as a pillar. A
    // stone is made to fall somewhere else instead.
    void addAvoid(float x, float z, float radius);

    // Drops a volley of stones around a point straight away, ignoring the
    // spawn timer. Used to collapse the corridor behind an escaping player.
    void collapseAround(const glm::vec3& centre, int count, float spread);

    // Chunks breaking off a ceiling section: dropped from exactly `centre`,
    // scattered by up to `spread` in x and z, anywhere in the level. Scenery,
    // like collapseAround - the section itself is the hazard.
    void dropChunks(const glm::vec3& centre, int count, float spread);

    // Returns true if a falling stone struck the player this frame.
    bool update(float deltaTime, const glm::vec3& playerPosition);

    // Landed stones are solid; this shoves the player back out of them.
    void pushPlayerOut(Player& player) const;

    void draw(const Shader& shader) const;          // the stones themselves
    void drawWarnings(const Shader& shader) const;  // floor rings, blended

    // Landing positions waiting for a dust burst. main drains this.
    bool popImpact(glm::vec3& outPosition);

    // Decaying camera shake from recent impacts, 0..1.
    float shake() const { return m_shake; }

    int airborneCount() const;

    // The nearest stone still in the air that is about to land in the band
    // just ahead of `from`. Used to steer around one; a HUD cue could use
    // the same query.
    bool nearestThreat(const glm::vec3& from, float lookAhead,
                       glm::vec3& outPosition) const;

private:
    struct Stone
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        glm::vec3 spin{ 0.0f };       // degrees per second
        glm::vec3 rotation{ 0.0f };

        float radius = 0.6f;
        float life   = 0.0f;          // seconds left once landed
        bool  landed = false;
        bool  active = false;
        bool  hasHit = false;         // only ever stuns once
    };

    std::vector<Stone> m_stones;
    std::vector<glm::vec3> m_impacts;

    Mesh m_stoneMesh;
    Mesh m_ringMesh;
    Material m_material;

    bool  m_active    = false;
    float m_intensity = 1.0f;
    float m_spawnTimer = 0.0f;
    float m_shake      = 0.0f;

    struct Avoid { float x = 0.0f, z = 0.0f, radius = 0.0f; };
    static constexpr int kMaxAvoid = 16;
    Avoid m_avoid[kMaxAvoid];
    int   m_avoidCount = 0;

    // Is a stone of this size clear of everything on the avoid list?
    bool clearOfAvoid(float x, float z, float stoneRadius) const;

    float m_gateZ     = 11.5f;
    float m_halfWidth = 4.0f;
    float m_exitZ     = 70.0f;
    float m_ceilingY  = 6.8f;

    std::mt19937 m_rng{ 90210u };
    float randomRange(float low, float high);

    void spawn(Stone& stone, const glm::vec3& playerPosition);
};

#endif // DEBRIS_H
