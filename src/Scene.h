#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <vector>

#include "Light.h"
#include "Mesh.h"
#include "SceneNode.h"
#include "Collapse.h"
#include "Ascension.h"
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

    // --- the collapse ---------------------------------------------------------
    // Poses every breakable ceiling section from the collapse, grows its
    // cracks, and takes each torch's brightness from it.
    void applyCollapse(const Collapse& collapse, float time);

    // 0..1 per torch, by Collapse's torch ids: 0-1 the chamber's, 2+ the
    // corridor's. Scales each flame's size, glow and light.
    std::vector<float> torchPower = std::vector<float>(Collapse::kMaxTorches, 1.0f);

    // Where a torch's flame is, for the puff of smoke when it goes out.
    glm::vec3 torchFlamePosition(int torch) const;

    // --- the constellation gate and the sanctuary -----------------------------
    // Three concentric stone rings on the gate, each turning about its centre.
    // Index 0 is the outer ring.
    std::vector<SceneNode*> gateRings;

    // Where the rings are, for the puzzle: their centre on the gate's face,
    // the radius of each ring's band (0 = outer), and the sockets' circle.
    glm::vec3 gateCentre{ 0.0f };
    float     gateBand[3] = { 0.0f, 0.0f, 0.0f };
    float     gateSocketRadius = 0.0f;

    // Set by main from the puzzle each frame.
    float gateRingAngle[3] = { 0.0f, 0.0f, 0.0f };   // degrees about Z
    int   gateSelected = -1;                         // -1: none highlighted
    float gateLock = 0.0f;                           // 0..1 as they lock

    // 0..1: he fades to a ghost while the camera looks past him at the gate,
    // so he never hides a ring's star.
    float travellerGhost = 0.0f;

    // --- the hidden garden ----------------------------------------------------
    // 0 shut, 1 the gate fully sunk into the floor.
    float gardenOpen = 0.0f;

    // The three relics (for the fireflies that gather round them) and what
    // in the garden is solid.
    std::vector<glm::vec3>    gardenRelics;
    std::vector<PropObstacle> gardenObstacles;

    // --- the garden of lost souls ----------------------------------------------
    // The wave of life: a circle spreading from the altar. Radius <= 0: the
    // garden is still dead. Main grows it; the shader and the plants read it.
    glm::vec3 gardenWaveCentre{ 0.0f };
    float     gardenWaveRadius = -1.0f;
    glm::vec3 gardenAltar{ 0.0f };          // the ankh's pedestal

    // 0..1: how alive the garden is at this spot right now.
    float gardenLifeAt(const glm::vec3& where) const;

    // The offering: the treasure's jewel floating from his hands into the
    // ankh's loop. < 0 hidden, 0..1 in flight, 1 resting there.
    float     offeringFlight = -1.0f;
    glm::vec3 offeringFrom{ 0.0f };
    glm::vec3 offeringPosition() const;

    // --- dawn ---------------------------------------------------------------------
    // 0 night, 1 morning. Moves the sun, fades the moon and stars, colours the
    // clouds, and warms and brightens the garden's light. Main sets it, and
    // also colours the sky (the clear colour) and the ambient from it.
    float dawn = 0.0f;

    // Which entry of `lights` casts the shadow map this frame (the chamber's
    // left torch), or -1 if it is not there - in the garden it is skipped.
    int shadowLightIndex() const { return m_shadowLight; }

    // Where the sun is, for the soft halo main draws round it.
    glm::vec3 sunPosition() const;

    // --- the ascension -------------------------------------------------------------
    // Reads the boat, clouds, sky sun and his glow from the timeline.
    void applyAscension(const Ascension& ascension);
    bool charmShattered = false;    // the charm cracks into light as he ascends
    void placeSkyDome(const glm::vec3& eye);   // keeps the sky dome round the camera

    // --- ray tracing -------------------------------------------------------------
    // The garden pool's reflections are ray traced in the shader against the
    // garden's own shapes, sent up each frame. Z toggles it.
    bool rayTracing = true;
    int  rayTracedShapes() const { return m_rtShapes; }
    glm::vec3 poolCentre() const { return m_poolCentre; }
    static constexpr float kPoolRadius = 2.35f;
    static constexpr float kWaterLevel = 0.07f;
    std::vector<glm::vec3> boatLamps() const;  // for the glow round each lamp
    std::vector<glm::vec3> oarBlades() const;  // for the sparkles dripping off them
    std::vector<glm::vec3> baBirds() const;    // where each soul-bird is, for its glow

    // --- the souls ---------------------------------------------------------------
    // Each statue, once the wave reaches it, glows, crumbles and frees a soul
    // that rises into the sky as a firefly. Main draws the souls as particles.
    int       soulCount() const { return static_cast<int>(m_statues.size()); }
    bool      soulVisible(int i) const;
    glm::vec3 soulPosition(int i) const;
    float     soulGlow(int i) const;          // 0..1
    glm::vec3 statueChest(int i) const;
    bool      popFreedStatue(int& index);     // one per statue as the wave reaches it
    SceneNode* sanctuaryDome = nullptr;

    float sanctuaryLevel   = 0.0f;   // 0 none, 1 fully raised
    float sanctuaryFlash   = 0.0f;   // brightens where she presses against it
    float sanctuaryWarning = 0.0f;   // 0..1 over its last seconds: it flickers

    // --- the Djinn made of smoke (drawn by main as particles) -----------------
    // Fed back here so the charm leaves from his hand and the lamp's light
    // moves up into his chest.
    glm::vec3 djinnHand{ 0.0f };
    glm::vec3 djinnChest{ 0.0f };
    float     djinnPresence = 0.0f;

    // --- the Djinn's charm ----------------------------------------------------
    SceneNode* charm    = nullptr;   // flies from the column, then circles him
    SceneNode* charmGem = nullptr;
    float charmLevel() const { return m_charmFlight; }
    glm::vec3 charmWorldPosition() const;

    // One firefly mid-flash casts a small real light. Level 0 = none.
    glm::vec3 fireflyLightPosition{ 0.0f };
    float     fireflyLightLevel = 0.0f;

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
    Mesh m_thinTorus;    // fine rims on the gate's rings
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

    // Medusa's arms: shoulder pivots that sway, and reach further when she is roused.
    std::vector<SceneNode*> m_medusaArms;
    std::vector<float>      m_medusaArmSides;
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

    // One per Collapse section, in the same order.
    struct Crack
    {
        SceneNode* node = nullptr;
        glm::vec3  start{ 0.0f };
        glm::vec3  direction{ 1.0f, 0.0f, 0.0f };
        float      length = 1.0f;
        float      threshold = 0.0f;   // how cracked the section must be first
    };
    struct CollapsePiece
    {
        SceneNode* node = nullptr;          // posed each frame
        SceneNode* companion = nullptr;     // the half of a beam that stays up
        std::vector<Crack> cracks;
        std::vector<Crack> companionCracks;
    };
    std::vector<CollapsePiece> m_collapsePieces;

    void buildCeilingWork();
    void addCracks(SceneNode* parent, const glm::vec3& size, int seed,
                   std::vector<Crack>& out);
    static void growCracks(std::vector<Crack>& cracks, float level);

    float m_headYaw   = 0.0f;
    float m_headPitch = 0.0f;

    // The charm: how far along its flight (1 = with him), how big (it shrinks
    // away on Reset), how bright (it dims if he is caught), and its two spins.
    float m_charmFlight = 0.0f;
    float m_charmScale  = 0.0f;
    float m_charmPower  = 1.0f;
    float m_charmOrbit  = 0.0f;
    float m_charmSpin   = 0.0f;
    float m_ghost       = 0.0f;

    SceneNode* m_gateRoot = nullptr;           // sinks into the floor to open
    std::vector<SceneNode*> m_relicSpinners;   // relics that turn slowly
    float m_relicSpin = 0.0f;
    int   m_shadowLight = -1;

    // The ascension: his gold glow and fading body, the cloud sea, Ra's boat
    // and its oars, the sun above the clouds, and night falling.
    float m_ascendGlow = 0.0f;
    float m_ascendFade = 0.0f;
    float m_ascendNight = 0.0f;
    float m_boatLevel = 0.0f;
    std::vector<SceneNode*> m_cloudSea;
    SceneNode* m_cloudFloor = nullptr;
    std::vector<float> m_cloudShade;    // 0 = a cloud's shadowed base, 1 = its sunlit top
    SceneNode* m_skyDome = nullptr;

    SceneNode* m_gardenRoot = nullptr;
    SceneNode* m_poolWater = nullptr;
    glm::vec3  m_poolCentre{ 0.0f };
    mutable int m_rtShapes = 0;
    void uploadRayTracing(const Shader& shader) const;
    std::vector<SceneNode*> m_boatLampNodes;
    std::vector<SceneNode*> m_oarBlades;

    // The Ba birds: golden soul-birds with human heads, flying beside the boat.
    struct BaBird
    {
        SceneNode* root = nullptr;
        SceneNode* leftWing = nullptr;
        SceneNode* rightWing = nullptr;
        glm::vec3  offset{ 0.0f };    // from the boat
        float      phase = 0.0f;
    };
    std::vector<BaBird> m_baBirds;
    std::vector<SceneNode*> m_oars;
    SceneNode* m_boat = nullptr;
    SceneNode* m_skySun = nullptr;
    void buildAscension();

    // The sky over the garden.
    SceneNode* m_moon = nullptr;
    SceneNode* m_sun = nullptr;
    std::vector<SceneNode*> m_stars;
    std::vector<float>      m_starBrightness;
    std::vector<SceneNode*> m_clouds;
    SceneNode* m_offering = nullptr;   // the jewel laid on the altar
    float m_offeringSpin = 0.0f;

    // A garden node whose SHAPE changes with life: fronds hanging limp or
    // spread, flowers closed or open. Its colour changes in the shader.
    struct Living
    {
        SceneNode* node = nullptr;
        glm::vec3  where{ 0.0f };            // where on the lawn it stands
        glm::vec3  alivePosition{ 0.0f }, deadPosition{ 0.0f };
        glm::vec3  aliveScale{ 1.0f },    deadScale{ 1.0f };
        glm::vec3  aliveRotation{ 0.0f }, deadRotation{ 0.0f };
    };
    std::vector<Living> m_living;

    // The stone travellers. Part 3 frees them.
    struct Statue
    {
        SceneNode* root = nullptr;
        SceneNode* body = nullptr;           // everything but the plinth
        std::vector<SceneNode*> parts;       // the stone limbs, head and torso
        glm::vec3  position{ 0.0f };
        float      size = 1.0f;
        float      freed = -1.0f;            // seconds since the wave reached it; < 0 not yet
        float      phase = 0.0f;             // its soul's own spiral
    };
    std::vector<Statue> m_statues;
    std::vector<int>    m_freedEvents;       // statues just reached, for main's bursts
    SceneNode* m_relicOrb = nullptr;           // pulses

    // The rune stones ringing the dome on the floor, and where each sits
    // relative to its centre when fully raised.
    std::vector<SceneNode*> m_runes;
    std::vector<glm::vec3>  m_runeOffsets;

    void buildTextures();
    void buildChamber();
    void buildScale();
    void buildLamp();
    void buildMedusa();
    void buildTraveller();
    void buildTreasure();
    void buildCorridor();
    void buildAnubisStatue();
    void buildCharm();
    void buildGate();
    void buildSanctuary();
    void buildGarden();

    void updateLights(float time);
};

#endif // SCENE_H
