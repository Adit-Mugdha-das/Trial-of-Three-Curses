#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <vector>

#include "Light.h"
#include "Mesh.h"
#include "SceneNode.h"
#include "Shader.h"
#include "Texture.h"
#include "TrialState.h"

// Builds and owns the whole chamber: the mesh library plus the node tree.
//
// The public SceneNode* handles are the animation targets. Phases 7-9 drive
// the trial entirely by writing to these - nothing else needs to know the
// tree's shape.
class Scene
{
public:
    void build();
    void update(float time, float deltaTime);
    // Two passes: opaque geometry first, then transparent nodes sorted
    // back-to-front from the camera. Needs the eye position to sort.
    void draw(const Shader& shader, const glm::vec3& cameraPosition) const;

    // Depth-only pass from the light's point of view, for shadow mapping.
    void drawDepth(const Shader& depthShader) const;

    // World position of the light that casts shadows, and the point it aims
    // at. Index 0 of `lights` is always the left torch - see updateLights().
    glm::vec3 shadowLightPosition() const;

    // The lamp mouth, where the Djinn smoke is emitted from.
    glm::vec3 djinnEmitterPosition() const;
    static glm::vec3 shadowLightTarget() { return { 0.0f, 1.6f, 0.0f }; }
    static constexpr int kShadowLightIndex = 0;

    // Small emissive spheres at every light position, for the L key.
    void drawLightMarkers(const Shader& shader) const;

    // Translates the trial's current state into the animation targets below.
    // Call before update(). Phases 7-9 deepen what each state drives.
    void applyTrial(const TrialController& trial);

    SceneNode* root() { return m_root.get(); }

    // The standing props, as circles on the floor, so the player and Medusa
    // can be kept out of them. Filled while the scene is built, which keeps
    // the radii next to the geometry they came from.
    struct PropObstacle
    {
        float x = 0.0f;
        float z = 0.0f;
        float radius = 0.0f;
    };
    std::vector<PropObstacle> propObstacles;

    // --- Medusa's gaze ------------------------------------------------------
    // The beam itself: a translucent cone from between her eyes. A child of
    // her head, so it turns wherever the head turns.
    SceneNode* gazeBeam = nullptr;

    // Tall cylinders that can break her line of sight. Kept apart from
    // propObstacles on purpose: a collision circle is the widest part of a
    // base, whereas the gaze only cares about the shaft.
    struct CoverPillar
    {
        float x = 0.0f, z = 0.0f, radius = 0.0f, height = 0.0f;
    };
    std::vector<CoverPillar> coverPillars;

    // A pillar between the camera and the traveller hides him from the
    // player exactly as it hides him from Medusa - and the chase camera sits
    // on Medusa's side. So any pillar standing in that line fades to a ghost,
    // and he stays visible while he hides.
    void fadeCoverBetween(const glm::vec3& eye, const glm::vec3& target,
                          float deltaTime);

    // The most faded any pillar is right now: 1 = all solid. For diagnostics.
    float minPillarFade() const
    {
        float lowest = 1.0f;
        for (float f : m_pillarFade)
        {
            if (f < lowest) { lowest = f; }
        }
        return lowest;
    }

    // While true, main steers her head, eyes and beam from the Gaze module.
    // While false she simply watches the traveller, as in the cursed ending.
    bool      gazeDriven     = false;
    glm::vec3 gazeAim{ 0.0f };      // world point her head turns toward
    float     gazeEye        = 0.3f;   // 0..1 brightness of her eyes
    float     gazeBeamWidth  = 0.0f;   // 0..1 of the full cone; 0 hides it
    float     gazeBeamLength = 0.0f;

    // Rebuilt every frame from the current node world positions, so lights
    // that are attached to moving geometry follow it automatically.
    std::vector<Light> lights;

    // The textured stone the falling rubble is made of, so Debris matches
    // the walls it is breaking off.
    Material rubbleMaterial;

    // Global ambient. Deliberately low - the torches should be doing the work.
    glm::vec3 ambient{ 0.16f, 0.13f, 0.18f };

    // --- inspection / animation targets ---------------------------------
    // Everything eases toward these, so setting one from a keypress now is
    // the same mechanism the Phase 6 state machine will use. Ranges are 0..1
    // except beamTarget, which is in degrees.
    // When the trial is driving, the targets below are already shaped curves
    // and are applied verbatim. In manual inspection they are step inputs, so
    // update() eases toward them instead. Easing an already-eased curve would
    // smooth the lid's overshoot straight back out.
    bool  directDrive  = false;

    bool  manualBeam   = false;   // true = beamTarget overrides the idle sway
    float beamTarget   = 0.0f;

    // 0 = the heart floats in the traveller's hands, 1 = resting on the pan.
    // Driven directly by the Placing state's progress rather than eased, so
    // the journey finishes exactly when the state does.
    float heartPlace = 0.0f;
    float lidTarget    = 0.0f;    // 0 shut, 1 fully open
    float headTarget   = 0.0f;    // 0 looking away, 1 facing the traveller
    float columnTarget = 0.0f;    // 0 hidden, 1 full Djinn energy column
    float petrifyTarget = 0.0f;   // 0 flesh, 1 fully petrified

    float lidLevel() const { return m_lid; }
    float headLevel() const { return m_head; }
    float columnLevel() const { return m_column; }
    float petrifyLevel() const { return m_petrify; }

    // --- Anubis's scale ---
    SceneNode* beam        = nullptr;   // rotates about Z: the verdict tilt
    SceneNode* panLeft     = nullptr;   // counter-rotates to hang level
    SceneNode* panRight    = nullptr;
    SceneNode* heart       = nullptr;   // translates in, then pulses

    // --- Djinn lamp ---
    SceneNode* lampLid     = nullptr;   // composite hinge rotation
    SceneNode* energyColumn = nullptr;  // rises on a balanced verdict
    std::vector<SceneNode*> energyRings;

    // --- Medusa ---
    SceneNode* medusaRoot  = nullptr;   // her body; the head aims within it
    SceneNode* medusaHead  = nullptr;   // yaws toward the traveller
    std::vector<SceneNode*> snakeSegments;

    // 0 = idle sway, 1 = fully roused. Widens and speeds the snakes' motion.
    float snakeAgitation = 0.0f;

    // 0..1. Floods the exit with light as the traveller reaches it.
    float exitGlow = 0.0f;

    // --- Traveller ---
    SceneNode* traveller   = nullptr;
    SceneNode* petrifyMeter = nullptr;

    // Limbs, for the walk cycle. Each rotates about a pivot at its hip or
    // shoulder rather than about its own centre.
    SceneNode* travellerHead  = nullptr;   // hidden in first person
    SceneNode* travellerTorso = nullptr;
    SceneNode* travellerLegL  = nullptr;
    SceneNode* travellerLegR  = nullptr;
    SceneNode* travellerArmL  = nullptr;
    SceneNode* travellerArmR  = nullptr;

    // How fast he is actually travelling, 0..1 of his top speed. Written by
    // main from the Player each frame; drives the whole walk cycle.
    float travellerSpeed01 = 0.0f;

    // --- Treasure ---
    SceneNode* treasure     = nullptr;   // the whole assembly; rises out of the floor
    SceneNode* treasureJewel = nullptr;  // spins and bobs on top of the pedestal

    // 0 = sunk below the floor, 1 = fully risen. Driven by the state machine
    // in Step E; until then by a debug key.
    float treasureReveal = 0.0f;

    // Latched the moment the player walks into it.
    bool treasureTaken = false;

    // Where the jewel actually is, for the pickup test.
    glm::vec3 treasureWorldPosition() const;

    // --- Escape corridor ---
    SceneNode* gate = nullptr;           // slab that slides up out of the doorway
    std::vector<SceneNode*> corridorTorches;

    // Glowing crystal shards set into the corridor. Emissive, so they cost
    // nothing from the light budget; only the nearest cluster is given a
    // real light to pool colour on the floor.
    std::vector<SceneNode*> shineShards;
    std::vector<SceneNode*> shineClusters;

    // 0 shut, 1 fully raised. Eased internally - it is a heavy stone slab.
    float gateTarget = 0.0f;
    float gateLevel() const { return m_gate; }

    // --- Torches (light anchors for Phase 4) ---
    SceneNode* torchLeft   = nullptr;
    SceneNode* torchRight  = nullptr;

private:
    std::unique_ptr<SceneNode> m_root;

    // Shared mesh library. Every snake segment is the same cylinder, so these
    // are uploaded once and pointed at by many nodes.
    Mesh m_cube;
    Mesh m_sphere;
    Mesh m_cylinder;
    Mesh m_cone;
    Mesh m_torus;
    Mesh m_plane;

    // Texture library. Generated procedurally in buildTextures(); swap any
    // one for Texture::loadFromFile("assets/textures/...") to use a real image.
    struct TextureSet
    {
        Texture sandstone;
        Texture floorTiles;
        Texture gold;
        Texture brass;
        Texture brassSpecular;
        Texture stone;
        Texture stoneSpecular;
        Texture snake;
        Texture snakeSpecular;
        Texture sandstoneNormal;
        Texture stoneNormal;
        Texture snakeNormal;
    };
    TextureSet m_textures;

    // Per-segment phase offsets, so the snakes do not writhe in lockstep.
    std::vector<float> m_snakePhases;
    std::vector<float> m_shinePhases;

    // Eased current values chasing the *Target fields above.
    float m_lid     = 0.0f;
    float m_head    = 0.0f;
    float m_column  = 0.0f;
    float m_petrify = 0.0f;

    // Advances with distance travelled, not with time, so the stride stays
    // locked to his feet at any speed and never jumps when the speed changes.
    float m_walkPhase = 0.0f;
    float m_gate = 0.0f;

    // Her head glides toward its aim rather than snapping to it, so a sudden
    // change of target (the end of an attack, or being caught) never jerks.
    std::vector<std::vector<SceneNode*>> m_pillarParts;
    std::vector<float> m_pillarFade;

    float m_headYaw   = 0.0f;
    float m_headPitch = 0.0f;

    void buildTextures();
    void buildChamber();
    void buildScale();
    void buildLamp();
    void buildMedusa();
    void buildTraveller();
    void buildTreasure();
    void buildCorridor();
    void buildAnubisStatue();

    void updateLights(float time);
};

#endif // SCENE_H
