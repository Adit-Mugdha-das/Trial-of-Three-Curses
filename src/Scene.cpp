#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

#include "Easing.h"
#include "EscapeTuning.h"
#include "Primitives.h"
#include "ProceduralTexture.h"

namespace
{
    // Working copies of the Phase 4 material table, with Phase 5 texture maps
    // attached. Filled by initMaterials() before the tree is built, so every
    // add() call site below stays unchanged.
    Material kGold      = Materials::gold;
    Material kBrass     = Materials::brass;
    Material kStone     = Materials::stone;
    Material kDarkStone = Materials::darkStone;
    Material kSandstone = Materials::sandstone;
    Material kFloor     = Materials::floorStone;
    Material kSnake     = Materials::snakeScale;
    Material kFlesh     = Materials::flesh;
    Material kHeart     = Materials::heart;
    Material kDjinn     = Materials::djinnEnergy;
    Material kFlame     = Materials::flame;
    Material kCloth     = Materials::cloth;
    Material kFeather   = Materials::feather;
    Material kEye       = Materials::eye;
    Material kTreasure  = Materials::treasure;

    // --- the heart's journey ------------------------------------------------
    // The heart is a child of the left pan for its whole life, so it inherits
    // the tilt once placed without any reparenting. These are both expressed
    // in the pan's local space.
    //
    // That works because the pan's local space equals world space up to a
    // translation while the beam is level - the arm node divides out the
    // beam's scale, and Placing always runs with the beam at rest.
    const glm::vec3 kHeartHome{ 2.09f, -1.15f, 6.60f };   // in the traveller's hands
    const glm::vec3 kHeartRest{ 0.00f,  0.35f, 0.00f };   // seated on the pan
    constexpr float kHeartArcHeight = 1.7f;

    // The treasure's resting place: clear floor just in front of the scale.
    // NOT on the scale itself - its base is a cylinder of radius 1.3 and the
    // jewel would sit inside it.
    const glm::vec3 kTreasureSpot{ 0.0f, 0.0f, 0.8f };
    constexpr float kTreasureJewelY = 1.35f;
    constexpr float kTreasureSunkY  = -1.9f;   // far enough down to be hidden

    // Feet to the top of the head. The petrification front travels this far.
    constexpr float kTravellerHeight = 3.0f;

    SceneNode* add(SceneNode* parent, const char* name, const Mesh* mesh,
                   const glm::vec3& position, const glm::vec3& scale,
                   const Material& material)
    {
        SceneNode* node = parent->createChild(name);
        node->mesh     = mesh;
        node->position = position;
        node->scale    = scale;
        node->material = material;
        return node;
    }
}

void Scene::buildTextures()
{
    auto upload = [](Texture& texture, const Procedural::Image& image)
    {
        texture.createFromPixels(image.width, image.height, image.channels,
                                 image.pixels.data());
    };

    // Kept on the CPU so the normal maps can be derived from them.
    const Procedural::Image sandstoneImage = Procedural::sandstone(256);
    const Procedural::Image stoneImage     = Procedural::roughStone(256);
    const Procedural::Image snakeImage     = Procedural::snakeScales(256);

    upload(m_textures.sandstone,     sandstoneImage);
    upload(m_textures.floorTiles,    Procedural::floorTiles(256));
    upload(m_textures.gold,          Procedural::goldBrushed(256));
    upload(m_textures.brass,         Procedural::brassPatina(256));
    upload(m_textures.brassSpecular, Procedural::brassSpecular(256));
    upload(m_textures.stone,         stoneImage);
    upload(m_textures.stoneSpecular, Procedural::stoneSpecular(256));
    upload(m_textures.snake,         snakeImage);
    upload(m_textures.snakeSpecular, Procedural::snakeSpecular(256));

    // Relief derived from each diffuse map's own luminance, so the lighting
    // response lines up exactly with the pattern you can see.
    upload(m_textures.sandstoneNormal,
           Procedural::normalFromLuminance(sandstoneImage, 2.2f));
    upload(m_textures.stoneNormal,
           Procedural::normalFromLuminance(stoneImage, 2.0f));
    upload(m_textures.snakeNormal,
           Procedural::normalFromLuminance(snakeImage, 3.4f));

    // --- attach maps to the material palette --------------------------------
    kSandstone.diffuseMap = &m_textures.sandstone;
    kSandstone.normalMap  = &m_textures.sandstoneNormal;
    kSandstone.uvScale    = { 6.0f, 2.0f };   // walls are long and not tall

    kFloor.diffuseMap = &m_textures.floorTiles;
    kFloor.uvScale    = { 5.0f, 5.0f };

    kGold.diffuseMap = &m_textures.gold;

    // The brass pair is the clearest specular-map demo in the scene: the
    // diffuse turns green where verdigris sits, and the specular map kills
    // the highlight in exactly those patches.
    kBrass.diffuseMap  = &m_textures.brass;
    kBrass.specularMap = &m_textures.brassSpecular;

    kStone.diffuseMap  = &m_textures.stone;
    kStone.specularMap = &m_textures.stoneSpecular;
    kStone.normalMap   = &m_textures.stoneNormal;

    kDarkStone.diffuseMap  = &m_textures.stone;
    kDarkStone.specularMap = &m_textures.stoneSpecular;
    kDarkStone.normalMap   = &m_textures.stoneNormal;

    // The strongest relief in the scene: the scales are pure geometry
    // expressed entirely through the normal map.
    kSnake.diffuseMap  = &m_textures.snake;
    kSnake.specularMap = &m_textures.snakeSpecular;
    kSnake.normalMap   = &m_textures.snakeNormal;
    kSnake.uvScale     = { 1.0f, 1.0f };

    rubbleMaterial = kStone;
}

void Scene::build()
{
    buildTextures();

    // Unit meshes, scaled per node. Everything is centred on its own origin
    // and Y-up, so node scales read as real-world sizes.
    m_cube.upload(Primitives::cube(1.0f, 1.0f, 1.0f));
    m_sphere.upload(Primitives::sphere(0.5f, 24, 48));
    m_cylinder.upload(Primitives::cylinder(0.5f, 1.0f, 32));
    m_cone.upload(Primitives::cone(0.5f, 1.0f, 32));
    m_torus.upload(Primitives::torus(0.5f, 0.14f, 40, 20));
    m_plane.upload(Primitives::plane(1.0f, 1.0f, 12));

    m_root = std::make_unique<SceneNode>("Root");

    buildChamber();
    buildScale();
    buildLamp();
    buildMedusa();
    buildTraveller();
    buildTreasure();
    buildAnubisStatue();
    buildCorridor();
    buildCeilingWork();

    m_root->updateWorld();
}

void Scene::buildChamber()
{
    SceneNode* chamber = m_root->createChild("Chamber");

    add(chamber, "Floor", &m_plane,
        { 0.0f, 0.0f, 0.0f }, { 26.0f, 1.0f, 24.0f }, kFloor);

    // Walls are thin boxes rather than planes so they read as solid from any
    // angle once back-face culling is on.
    add(chamber, "BackWall", &m_cube,
        { 0.0f, 4.0f, -11.0f }, { 26.0f, 8.0f, 0.6f }, kSandstone);
    add(chamber, "LeftWall", &m_cube,
        { -13.0f, 4.0f, 0.0f }, { 0.6f, 8.0f, 24.0f }, kSandstone);
    add(chamber, "RightWall", &m_cube,
        { 13.0f, 4.0f, 0.0f }, { 0.6f, 8.0f, 24.0f }, kSandstone);

    // Two torch brackets on the back wall. The flame spheres double as the
    // visible marker for the Phase 4 point lights.
    const float torchX[2] = { -7.0f, 7.0f };
    for (int i = 0; i < 2; ++i)
    {
        SceneNode* torch = chamber->createChild(i == 0 ? "TorchL" : "TorchR");
        torch->position = { torchX[i], 4.4f, -10.6f };

        add(torch, "Bracket", &m_cylinder,
            { 0.0f, -0.35f, 0.25f }, { 0.16f, 0.9f, 0.16f }, kDarkStone);
        add(torch, "Bowl", &m_cone,
            { 0.0f, 0.05f, 0.35f }, { 0.5f, 0.45f, 0.5f }, kBrass);
        add(torch, "Flame", &m_sphere,
            { 0.0f, 0.35f, 0.35f }, { 0.42f, 0.6f, 0.42f }, kFlame);

        if (i == 0) { torchLeft = torch; } else { torchRight = torch; }
    }
}

void Scene::buildScale()
{
    SceneNode* scale = m_root->createChild("AnubisScale");
    scale->position = { 0.0f, 0.0f, -2.0f };

    add(scale, "Base", &m_cylinder,
        { 0.0f, 0.25f, 0.0f }, { 2.6f, 0.5f, 2.6f }, kDarkStone);

    // Solid: the base is 2.6 across, so radius 1.3 with a little margin.
    propObstacles.push_back({ scale->position.x, scale->position.z, 1.45f });
    add(scale, "BaseTrim", &m_torus,
        { 0.0f, 0.5f, 0.0f }, { 2.4f, 0.6f, 2.4f }, kGold);

    SceneNode* pillar = add(scale, "Pillar", &m_cylinder,
        { 0.0f, 2.6f, 0.0f }, { 0.3f, 4.2f, 0.3f }, kGold);

    // Anubis's head crowning the pillar: muzzle forward, two upright ears.
    SceneNode* anubis = pillar->createChild("AnubisHead");
    anubis->position = { 0.0f, 0.62f, 0.0f };            // local to the pillar
    anubis->scale    = { 1.0f / 0.3f, 1.0f / 4.2f, 1.0f / 0.3f };  // undo pillar scale

    add(anubis, "Skull", &m_sphere,
        { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.62f }, kGold);
    add(anubis, "Muzzle", &m_cone,
        { 0.0f, -0.06f, 0.34f }, { 0.24f, 0.42f, 0.24f }, kGold)
        ->rotation = { 90.0f, 0.0f, 0.0f };
    add(anubis, "EarL", &m_cone,
        { -0.17f, 0.34f, -0.02f }, { 0.16f, 0.4f, 0.16f }, kGold);
    add(anubis, "EarR", &m_cone,
        { 0.17f, 0.34f, -0.02f }, { 0.16f, 0.4f, 0.16f }, kGold);

    // --- the beam: the single node whose rotation decides the whole story ---
    beam = scale->createChild("Beam");
    beam->position = { 0.0f, 4.75f, 0.0f };
    beam->mesh     = &m_cube;
    beam->scale    = { 4.6f, 0.14f, 0.14f };
    beam->material    = kGold;

    // Children are declared in the beam's UNSCALED space, so undo the beam's
    // own scale on each child. (Phase 4 note: this is why uNormalMatrix has to
    // be an inverse-transpose - these scales are wildly non-uniform.)
    const glm::vec3 unbeam(1.0f / 4.6f, 1.0f / 0.14f, 1.0f / 0.14f);

    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -0.455f : 0.455f;   // in beam-local units

        SceneNode* arm = beam->createChild(side == 0 ? "ArmL" : "ArmR");
        arm->position = { x, 0.0f, 0.0f };
        arm->scale    = unbeam;

        add(arm, "Chain", &m_cylinder,
            { 0.0f, -0.65f, 0.0f }, { 0.06f, 1.3f, 0.06f }, kDarkStone);

        // The pan hangs from the chain and counter-rotates in update() so it
        // stays level however far the beam tilts.
        SceneNode* pan = arm->createChild(side == 0 ? "PanL" : "PanR");
        pan->position = { 0.0f, -1.3f, 0.0f };

        add(pan, "Dish", &m_cylinder,
            { 0.0f, 0.0f, 0.0f }, { 1.5f, 0.08f, 1.5f }, kGold);
        add(pan, "Rim", &m_torus,
            { 0.0f, 0.02f, 0.0f }, { 1.6f, 0.5f, 1.6f }, kGold);

        if (side == 0) { panLeft = pan; } else { panRight = pan; }
    }

    // The heart starts beside the scale and translates onto the left pan in
    // Phase 7. Parented to the pan already so it inherits the tilt.
    heart = panLeft->createChild("Heart");
    heart->position = { 0.0f, 0.35f, 0.0f };
    heart->mesh     = &m_sphere;
    heart->scale    = { 0.6f, 0.6f, 0.6f };
    heart->material    = kHeart;

    // Counterweight: the Feather of Ma'at.
    add(panRight, "Feather", &m_cube,
        { 0.0f, 0.28f, 0.0f }, { 0.1f, 0.55f, 0.28f }, kFeather)
        ->rotation = { 0.0f, 0.0f, 12.0f };
}

void Scene::buildLamp()
{
    SceneNode* lamp = m_root->createChild("DjinnLamp");
    lamp->position = { -6.0f, 0.0f, 1.5f };

    add(lamp, "Pedestal", &m_cylinder,
        { 0.0f, 0.55f, 0.0f }, { 2.0f, 1.1f, 2.0f }, kDarkStone);

    // The pedestal is 2.0 across; the lamp body overhangs it, so 1.25.
    propObstacles.push_back({ lamp->position.x, lamp->position.z, 1.25f });

    // Squashed sphere for the lamp body - a clear non-uniform scale, which is
    // exactly the case a naive normal matrix gets wrong.
    add(lamp, "Body", &m_sphere,
        { 0.0f, 1.55f, 0.0f }, { 1.7f, 1.0f, 1.3f }, kBrass);

    add(lamp, "Spout", &m_cone,
        { -1.15f, 1.6f, 0.0f }, { 0.34f, 1.1f, 0.34f }, kBrass)
        ->rotation = { 0.0f, 0.0f, 78.0f };

    add(lamp, "Handle", &m_torus,
        { 1.05f, 1.7f, 0.0f }, { 1.0f, 1.0f, 1.0f }, kBrass)
        ->rotation = { 0.0f, 90.0f, 90.0f };

    add(lamp, "Collar", &m_cylinder,
        { 0.0f, 2.0f, 0.0f }, { 0.6f, 0.22f, 0.6f }, kBrass);

    // COMPOSITE TRANSFORM: the lid sits at the top of the lamp, but its pivot
    // is offset to the rim. Rotating about Z therefore swings it open on a
    // hinge instead of spinning it through the lamp body.
    lampLid = lamp->createChild("Lid");
    lampLid->position = { 0.0f, 2.12f, 0.0f };
    lampLid->pivot    = { -0.32f, 0.0f, 0.0f };
    lampLid->mesh     = &m_cone;
    lampLid->scale    = { 0.7f, 0.4f, 0.7f };
    lampLid->material    = kBrass;

    // Blessing effect, hidden until the verdict comes back balanced.
    energyColumn = lamp->createChild("EnergyColumn");
    energyColumn->position = { 0.0f, 2.2f, 0.0f };
    energyColumn->mesh     = &m_cylinder;
    energyColumn->scale    = { 0.55f, 3.2f, 0.55f };
    energyColumn->material    = kDjinn;
    energyColumn->visible  = false;

    // Rings ride the column, so they inherit its rise for free.
    for (int i = 0; i < 5; ++i)
    {
        SceneNode* ring = energyColumn->createChild("EnergyRing");
        ring->mesh  = &m_torus;
        ring->material = kDjinn;

        // Undo the column's scale so ring sizes are readable in world units.
        const float t = static_cast<float>(i) / 4.0f;
        ring->position = { 0.0f, -0.5f + t, 0.0f };
        ring->scale    = { 2.6f / 0.55f, 0.4f / 3.2f, 2.6f / 0.55f };

        energyRings.push_back(ring);
    }
}

void Scene::buildMedusa()
{
    SceneNode* medusa = m_root->createChild("Medusa");
    medusa->position = { 6.0f, 0.0f, -1.5f };
    medusa->rotation = { 0.0f, -35.0f, 0.0f };   // starts looking away
    medusaRoot = medusa;

    add(medusa, "Base", &m_cylinder,
        { 0.0f, 0.2f, 0.0f }, { 2.4f, 0.4f, 2.4f }, kDarkStone);

    // Coiled tail: three stacked, shrinking rings.
    for (int i = 0; i < 3; ++i)
    {
        const float t = static_cast<float>(i);
        add(medusa, "Coil", &m_torus,
            { 0.0f, 0.5f + t * 0.42f, 0.0f },
            { 2.4f - t * 0.45f, 0.9f, 2.4f - t * 0.45f },
            kStone);
    }

    add(medusa, "Torso", &m_cylinder,
        { 0.0f, 2.35f, 0.0f }, { 1.15f, 2.0f, 1.15f }, kStone);
    add(medusa, "Shoulders", &m_sphere,
        { 0.0f, 3.3f, 0.0f }, { 1.3f, 0.7f, 1.0f }, kStone);

    // --- the head: yaws toward the traveller in Phase 9 ---
    medusaHead = medusa->createChild("MedusaHead");
    medusaHead->position = { 0.0f, 3.85f, 0.0f };

    add(medusaHead, "Skull", &m_sphere,
        { 0.0f, 0.0f, 0.0f }, { 0.9f, 1.0f, 0.9f }, kStone);

    // Eyes on the head's local +Z, which is what worldForward() reports and
    // what the Phase 9 spotlights will aim along.
    add(medusaHead, "EyeL", &m_sphere,
        { -0.21f, 0.08f, 0.4f }, { 0.16f, 0.16f, 0.1f }, kEye);
    add(medusaHead, "EyeR", &m_sphere,
        { 0.21f, 0.08f, 0.4f }, { 0.16f, 0.16f, 0.1f }, kEye);

    // The gaze made visible: a long translucent cone from between her eyes.
    // The cone mesh has its apex at +Y; rotating -90 degrees about X sends
    // +Y to -Z, so the apex sits at her eyes and the wide end points away
    // along the head's forward axis. Length and width are set every frame.
    {
        Material beam;
        beam.ka = { 0.0f, 0.0f, 0.0f };
        beam.kd = { 0.0f, 0.0f, 0.0f };
        beam.ks = { 0.0f, 0.0f, 0.0f };
        beam.emissive = { 0.35f, 0.95f, 0.25f };
        beam.opacity  = 0.2f;

        gazeBeam = add(medusaHead, "GazeBeam", &m_cone,
                       { 0.0f, 0.08f, 1.0f }, { 1.0f, 1.0f, 1.0f }, beam);
        gazeBeam->rotation = { -90.0f, 0.0f, 0.0f };
        gazeBeam->visible  = false;
    }

    // --- snakes: the deepest hierarchy in the scene ---
    // 7 snakes, each a chain of 4 segments. A segment is a child of the one
    // below it, so bending one bends everything above it, and the whole head
    // of snakes follows medusaHead's yaw automatically.
    constexpr int kSnakeCount = 7;
    constexpr int kSegments   = 4;

    for (int s = 0; s < kSnakeCount; ++s)
    {
        const float angle = (static_cast<float>(s) / kSnakeCount) * 360.0f;
        const float rad   = glm::radians(angle);

        SceneNode* root = medusaHead->createChild("SnakeRoot");
        root->position = { 0.36f * std::cos(rad), 0.34f, 0.36f * std::sin(rad) };
        root->rotation = { 18.0f, angle, 0.0f };

        SceneNode* parent = root;
        for (int seg = 0; seg < kSegments; ++seg)
        {
            SceneNode* segment = parent->createChild("SnakeSegment");
            segment->position = { 0.0f, (seg == 0) ? 0.16f : 0.3f, 0.0f };
            segment->rotation = { 12.0f, 0.0f, 0.0f };

            // Mesh child, so the segment node itself stays a clean pivot and
            // its scale does not cascade to the next segment.
            const float taper = 1.0f - 0.13f * static_cast<float>(seg);
            add(segment, "SnakeBody", &m_cylinder,
                { 0.0f, 0.15f, 0.0f },
                { 0.17f * taper, 0.32f, 0.17f * taper },
                kSnake);

            if (seg == kSegments - 1)
            {
                add(segment, "SnakeHead", &m_sphere,
                    { 0.0f, 0.34f, 0.06f }, { 0.2f, 0.18f, 0.28f }, kSnake);
            }

            snakeSegments.push_back(segment);
            m_snakePhases.push_back(static_cast<float>(s) * 0.9f +
                                    static_cast<float>(seg) * 0.55f);

            parent = segment;
        }
    }
}

void Scene::buildTraveller()
{
    traveller = m_root->createChild("Traveller");
    traveller->position = { 0.0f, 0.0f, 5.5f };
    traveller->rotation = { 0.0f, 180.0f, 0.0f };   // facing the scale

    // A limb must swing from its joint, not its middle. The pivot field does
    // exactly that: local = T(position) * T(pivot) * R * T(-pivot) * S, and
    // because the pivot is applied AFTER the scale it is measured in real
    // world units - half the limb's scaled length puts it at the joint.
    travellerLegL = add(traveller, "LegL", &m_cylinder,
        { -0.22f, 0.6f, 0.0f }, { 0.26f, 1.2f, 0.26f }, kFlesh);
    travellerLegL->pivot = { 0.0f, 0.6f, 0.0f };          // hip

    travellerLegR = add(traveller, "LegR", &m_cylinder,
        { 0.22f, 0.6f, 0.0f }, { 0.26f, 1.2f, 0.26f }, kFlesh);
    travellerLegR->pivot = { 0.0f, 0.6f, 0.0f };

    SceneNode* torso = add(traveller, "Torso", &m_cube,
        { 0.0f, 1.75f, 0.0f }, { 0.8f, 1.2f, 0.42f }, kCloth);

    // Leaning pivots at the waist, so the shoulders move and the hips do not.
    torso->pivot = { 0.0f, -0.6f, 0.0f };
    travellerTorso = torso;

    // Undo the torso's scale so the limbs and head can be placed in real units.
    const glm::vec3 untorso(1.0f / 0.8f, 1.0f / 1.2f, 1.0f / 0.42f);

    SceneNode* upper = torso->createChild("UpperBody");
    upper->scale = untorso;

    travellerHead = add(upper, "Head", &m_sphere,
        { 0.0f, 0.9f, 0.0f }, { 0.5f, 0.56f, 0.5f }, kFlesh);

    travellerArmL = add(upper, "ArmL", &m_cylinder,
        { -0.55f, 0.05f, 0.0f }, { 0.2f, 1.0f, 0.2f }, kFlesh);
    travellerArmL->rotation = { 0.0f, 0.0f, 10.0f };
    travellerArmL->pivot    = { 0.0f, 0.5f, 0.0f };       // shoulder

    travellerArmR = add(upper, "ArmR", &m_cylinder,
        { 0.55f, 0.05f, 0.0f }, { 0.2f, 1.0f, 0.2f }, kFlesh);
    travellerArmR->rotation = { 0.0f, 0.0f, -10.0f };
    travellerArmR->pivot    = { 0.0f, 0.5f, 0.0f };

    // Opt the whole body into the shader's petrification blend. Done before
    // the meter is created below, so the indicator itself stays unaffected.
    traveller->setPetrifiesRecursive(true);

    // Petrification indicator: grows from the floor as the curse takes hold.
    petrifyMeter = traveller->createChild("PetrifyMeter");
    petrifyMeter->position = { 1.2f, 0.05f, 0.0f };
    petrifyMeter->mesh     = &m_cylinder;
    petrifyMeter->scale    = { 0.18f, 0.1f, 0.18f };
    petrifyMeter->material    = kStone;
}

void Scene::applyTrial(const TrialController& trial)
{
    const TrialState state = trial.state();
    const float      t     = trial.progress();

    // Default every target to rest, then let the current state raise the ones
    // it owns. Stating the whole target set each frame means a state can never
    // leak a stale value into the next one.
    directDrive    = true;
    manualBeam     = true;
    beamTarget     = 0.0f;
    lidTarget      = 0.0f;
    headTarget     = 0.0f;
    columnTarget   = 0.0f;
    snakeAgitation = 0.0f;
    exitGlow       = 0.0f;
    petrifyTarget  = trial.petrification();
    treasureReveal = trial.treasureReveal();

    switch (state)
    {
        case TrialState::Waiting:
            // Hand the beam back to its idle sway while nothing is committed.
            manualBeam = false;
            heartPlace = 0.0f;
            break;

        case TrialState::Placing:
            // The beam stays level until the heart actually lands - the pan's
            // local space is only equal to world space while it does.
            beamTarget = 0.0f;
            heartPlace = t;
            break;

        case TrialState::Weighing:
            // Swing to the verdict angle with a decaying wobble, so the beam
            // reads as something with mass rather than a value being set.
            beamTarget = trial.verdictAngle() * Easing::dampedSettle(t);
            heartPlace = 1.0f;
            break;

        case TrialState::Balanced:
            beamTarget = trial.verdictAngle();
            heartPlace = 1.0f;

            // The lid pops open first, then the energy follows it out.
            lidTarget    = Easing::easeOutBack(Easing::clamp01(t / 0.35f));
            columnTarget = Easing::smoothstep01(Easing::clamp01((t - 0.2f) / 0.5f));
            break;

        case TrialState::Cursed:
            beamTarget = trial.verdictAngle();
            heartPlace = 1.0f;

            // She wakes and turns. She no longer petrifies anyone here - that
            // is now the Caught ending, if the escape goes badly.
            headTarget = Easing::smoothstep01(Easing::clamp01(t / 0.3f));

            // The snakes rouse just ahead of her head finishing its turn, so
            // the threat registers before the gaze lands.
            snakeAgitation = Easing::smoothstep01(Easing::clamp01(t / 0.22f));
            break;

        case TrialState::TreasureRevealed:
            // Only a balanced verdict reaches this state, so the lamp is
            // always open here - and it stays open, because its magic is
            // what is holding the treasure up.
            beamTarget   = trial.verdictAngle();
            heartPlace   = 1.0f;
            lidTarget    = 1.0f;
            columnTarget = 1.0f;
            break;

        case TrialState::Escape:
            // The scale is forgotten. She is awake, watching, and moving.
            beamTarget     = trial.verdictAngle();
            heartPlace     = 1.0f;
            headTarget     = 1.0f;
            snakeAgitation = 1.0f;
            break;

        case TrialState::Escaped:
            // The lamp answers from back in the chamber, and daylight floods
            // the doorway he just came through.
            lidTarget    = 1.0f;
            columnTarget = Easing::smoothstep01(Easing::clamp01(t / 0.4f));
            exitGlow     = Easing::smoothstep01(Easing::clamp01(t / 0.35f));
            break;

        case TrialState::Caught:
            // Her gaze is locked on; the snakes settle as the work is done.
            headTarget     = 1.0f;
            snakeAgitation = 1.0f - 0.6f * Easing::smoothstep01(t);
            break;

        case TrialState::Reset:
            // The beam levels off first, then the heart flies home over the
            // back half, so the two motions read as separate beats.
            heartPlace = 1.0f - Easing::smoothstep01(Easing::clamp01((t - 0.35f) / 0.65f));

            // Settle back down rather than snapping to calm - but only if the
            // curse actually roused them. A blessing leaves them undisturbed.
            snakeAgitation = trial.isBalanced()
                           ? 0.0f
                           : (1.0f - Easing::smoothstep01(t));
            break;
    }
}

void Scene::buildAnubisStatue()
{
    // A monumental enthroned Anubis filling the back of the chamber, behind
    // the scale. Everything here is built from the same seven primitives as
    // the rest of the project - the silhouette does the work, not detail.
    SceneNode* statue = m_root->createChild("AnubisStatue");
    statue->position = { 0.0f, 0.0f, -8.5f };

    // Authored at a comfortable working size, then shrunk to fit under the
    // 8-unit walls. Scaling the root is safe because nothing under it is
    // animated, so the cascade costs nothing.
    statue->scale = glm::vec3(0.78f);

    Material obsidian = kDarkStone;
    obsidian.kd *= 0.75f;              // darker than the chamber's masonry

    Material eyeGlow = kGold;
    eyeGlow.emissive = { 0.85f, 0.62f, 0.18f };

    // --- stepped dais ---------------------------------------------------
    add(statue, "DaisLower",  &m_cube, { 0.0f, 0.25f, 0.0f }, { 9.0f, 0.5f, 5.0f }, kSandstone);
    add(statue, "DaisMiddle", &m_cube, { 0.0f, 0.75f, -0.3f }, { 7.6f, 0.5f, 4.2f }, kSandstone);
    add(statue, "DaisUpper",  &m_cube, { 0.0f, 1.25f, -0.6f }, { 6.4f, 0.5f, 3.4f }, kSandstone);

    // --- throne -----------------------------------------------------------
    add(statue, "Seat", &m_cube, { 0.0f, 2.65f, -0.4f }, { 4.4f, 0.7f, 3.2f }, obsidian);
    add(statue, "Backrest", &m_cube, { 0.0f, 5.4f, -1.75f }, { 4.4f, 5.0f, 0.55f }, obsidian);

    // Gold banding down the back, echoing the reference's inlay.
    add(statue, "BackInlay", &m_cube, { 0.0f, 5.4f, -1.42f }, { 0.5f, 4.4f, 0.12f }, kGold);

    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -1.95f : 1.95f;
        add(statue, "Armrest", &m_cube, { x, 3.5f, -0.2f }, { 0.55f, 0.55f, 2.8f }, obsidian);
        add(statue, "ArmrestPost", &m_cube, { x, 3.0f, 1.0f }, { 0.5f, 0.6f, 0.5f }, obsidian);
    }

    // --- seated legs ------------------------------------------------------
    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -1.0f : 1.0f;
        add(statue, "Thigh", &m_cube, { x, 3.35f, 0.8f }, { 1.05f, 0.9f, 2.6f }, obsidian);
        add(statue, "Shin",  &m_cube, { x, 2.42f, 2.0f }, { 0.95f, 1.85f, 1.0f }, obsidian);
        add(statue, "Foot",  &m_cube, { x, 1.68f, 2.6f }, { 1.1f, 0.4f, 1.5f }, obsidian);
    }

    // --- torso ------------------------------------------------------------
    add(statue, "Torso", &m_cube, { 0.0f, 4.6f, -0.5f }, { 3.4f, 2.6f, 1.9f }, obsidian);
    add(statue, "Chest", &m_sphere, { 0.0f, 5.75f, -0.45f }, { 3.5f, 1.3f, 2.0f }, obsidian);

    // The broad usekh collar: the one piece that reads as gold from anywhere.
    add(statue, "Collar", &m_cylinder, { 0.0f, 5.95f, -0.4f }, { 3.7f, 0.42f, 2.6f }, kGold);
    add(statue, "CollarTrim", &m_torus, { 0.0f, 5.72f, -0.4f }, { 3.5f, 0.6f, 2.4f }, kGold);

    // --- arms crossed over the chest --------------------------------------
    add(statue, "ArmUpper", &m_cube, { -0.1f, 5.15f, 0.75f }, { 3.0f, 0.72f, 0.75f }, obsidian)
        ->rotation = { 0.0f, 0.0f, 16.0f };
    add(statue, "ArmLower", &m_cube, { 0.1f, 4.55f, 0.85f }, { 3.0f, 0.72f, 0.75f }, obsidian)
        ->rotation = { 0.0f, 0.0f, -16.0f };

    // --- crook and flail ---------------------------------------------------
    SceneNode* crook = statue->createChild("Crook");
    crook->position = { -1.25f, 5.6f, 1.15f };
    crook->rotation = { 0.0f, 0.0f, 14.0f };
    add(crook, "Shaft", &m_cylinder, { 0.0f, 0.0f, 0.0f }, { 0.22f, 2.6f, 0.22f }, kGold);
    add(crook, "Hook",  &m_torus,    { -0.22f, 1.35f, 0.0f }, { 0.75f, 0.5f, 0.35f }, kGold);

    SceneNode* flail = statue->createChild("Flail");
    flail->position = { 1.25f, 5.55f, 1.15f };
    flail->rotation = { 0.0f, 0.0f, -14.0f };
    add(flail, "Shaft", &m_cylinder, { 0.0f, 0.0f, 0.0f }, { 0.22f, 2.4f, 0.22f }, kGold);
    for (int i = 0; i < 3; ++i)
    {
        const float x = (static_cast<float>(i) - 1.0f) * 0.26f;
        add(flail, "Strand", &m_cylinder, { x, 1.55f, 0.0f }, { 0.11f, 0.9f, 0.11f }, kGold);
    }

    // --- the jackal head ---------------------------------------------------
    SceneNode* head = statue->createChild("JackalHead");
    head->position = { 0.0f, 7.05f, -0.15f };

    // Nemes headdress: two flared panels either side of the face. These are
    // what make the silhouette read as Egyptian from across the room.
    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -1.45f : 1.45f;
        const float tilt = (side == 0) ? 11.0f : -11.0f;

        add(head, "NemesPanel", &m_cube, { x, -0.35f, 0.15f },
            { 0.95f, 2.5f, 1.5f }, kGold)->rotation = { 0.0f, 0.0f, tilt };
        add(head, "NemesStripe", &m_cube, { x * 1.06f, -0.35f, 0.92f },
            { 0.55f, 2.3f, 0.12f }, obsidian)->rotation = { 0.0f, 0.0f, tilt };
    }

    add(head, "NemesCrown", &m_cube, { 0.0f, 0.85f, -0.1f }, { 3.1f, 0.7f, 2.2f }, kGold);

    add(head, "Skull",  &m_sphere, { 0.0f, 0.0f, -0.1f }, { 1.7f, 1.75f, 2.0f }, obsidian);
    add(head, "Brow",   &m_cube,   { 0.0f, 0.5f, 0.62f }, { 1.5f, 0.28f, 0.7f }, kGold);

    // Long tapered muzzle, pointing out over the scale.
    add(head, "Muzzle", &m_cone, { 0.0f, -0.32f, 1.35f }, { 0.85f, 2.3f, 0.85f }, obsidian)
        ->rotation = { 90.0f, 0.0f, 0.0f };
    add(head, "Nose", &m_sphere, { 0.0f, -0.32f, 2.35f }, { 0.4f, 0.36f, 0.4f }, obsidian);

    // Tall upright ears - the most recognisable part of the silhouette.
    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -0.68f : 0.68f;
        const float tilt = (side == 0) ? -5.0f : 5.0f;

        add(head, "Ear", &m_cone, { x, 1.65f, -0.35f }, { 0.62f, 2.0f, 0.62f }, obsidian)
            ->rotation = { 0.0f, 0.0f, tilt };
        add(head, "EarInner", &m_cone, { x, 1.55f, -0.18f }, { 0.34f, 1.5f, 0.34f }, kGold)
            ->rotation = { 0.0f, 0.0f, tilt };
    }

    add(head, "EyeL", &m_sphere, { -0.48f, 0.12f, 1.02f }, { 0.3f, 0.22f, 0.22f }, eyeGlow);
    add(head, "EyeR", &m_sphere, {  0.48f, 0.12f, 1.02f }, { 0.3f, 0.22f, 0.22f }, eyeGlow);

    // --- flanking columns ---------------------------------------------------
    // They frame the statue and give the back of the room some depth.
    for (int side = 0; side < 2; ++side)
    {
        const float x = (side == 0) ? -7.4f : 7.4f;

        SceneNode* column = statue->createChild("Column");
        column->position = { x, 0.0f, -0.9f };

        add(column, "Base",    &m_cube,     { 0.0f, 0.35f, 0.0f }, { 2.6f, 0.7f, 2.6f }, kSandstone);
        add(column, "Shaft",   &m_cylinder, { 0.0f, 4.4f, 0.0f },  { 1.9f, 7.4f, 1.9f }, kSandstone);
        add(column, "Band",    &m_torus,    { 0.0f, 5.9f, 0.0f },  { 2.1f, 0.7f, 2.1f }, kGold);
        add(column, "Capital", &m_cube,     { 0.0f, 8.3f, 0.0f },  { 2.8f, 0.9f, 2.8f }, kSandstone);
    }
}

void Scene::buildCorridor()
{
    SceneNode* corridor = m_root->createChild("Corridor");

    // Long surfaces need far more tiling than the chamber's. Local copies, so
    // the shared palette entry is untouched.
    Material wallMaterial = kSandstone;
    wallMaterial.uvScale = { 18.0f, 2.0f };

    Material floorMaterial = kFloor;
    floorMaterial.uvScale = { 3.0f, 22.0f };

    const float gateZ  = Tuning::kGateZ;
    const float halfW  = Tuning::kCorridorHalfWidth;
    const float farZ   = Tuning::kExitZ + 4.0f;
    const float length = farZ - gateZ;
    const float midZ   = (gateZ + farZ) * 0.5f;

    constexpr float kDoorHalfWidth = 3.5f;
    constexpr float kDoorHeight    = 5.0f;
    constexpr float kWallHeight    = 8.0f;

    // --- the front wall of the chamber, with a doorway punched in it --------
    // Three pieces rather than one: left of the door, right of it, and a
    // lintel across the top. A single box would have no opening.
    const float sideWidth = 13.0f - kDoorHalfWidth;
    const float sideCentre = kDoorHalfWidth + sideWidth * 0.5f;

    add(corridor, "FrontWallL", &m_cube,
        { -sideCentre, kWallHeight * 0.5f, gateZ },
        { sideWidth, kWallHeight, 0.6f }, kSandstone);
    add(corridor, "FrontWallR", &m_cube,
        { sideCentre, kWallHeight * 0.5f, gateZ },
        { sideWidth, kWallHeight, 0.6f }, kSandstone);
    add(corridor, "Lintel", &m_cube,
        { 0.0f, (kDoorHeight + kWallHeight) * 0.5f, gateZ },
        { kDoorHalfWidth * 2.0f, kWallHeight - kDoorHeight, 0.6f }, kSandstone);

    // --- the gate slab -------------------------------------------------------
    gate = corridor->createChild("Gate");
    gate->position = { 0.0f, kDoorHeight * 0.5f, gateZ };
    gate->mesh     = &m_cube;
    gate->scale    = { kDoorHalfWidth * 2.0f - 0.1f, kDoorHeight - 0.05f, 0.45f };
    gate->material = kDarkStone;

    // --- the corridor itself -------------------------------------------------
    // A box whose top face sits exactly at y = 0, flush with the chamber floor.
    add(corridor, "CorridorFloor", &m_cube,
        { 0.0f, -0.15f, midZ }, { halfW * 2.0f, 0.3f, length }, floorMaterial);

    add(corridor, "CorridorWallL", &m_cube,
        { -(halfW + 0.3f), 3.5f, midZ }, { 0.6f, 7.0f, length }, wallMaterial);
    add(corridor, "CorridorWallR", &m_cube,
        { halfW + 0.3f, 3.5f, midZ }, { 0.6f, 7.0f, length }, wallMaterial);

    add(corridor, "CorridorCeiling", &m_cube,
        { 0.0f, 7.2f, midZ }, { halfW * 2.0f + 1.2f, 0.4f, length }, wallMaterial);

    // --- torches down its length ---------------------------------------------
    // Only the nearest couple are ever uploaded - see updateLights().
    for (int i = 0; i < 5; ++i)
    {
        const float z    = gateZ + 7.0f + static_cast<float>(i) * 12.0f;
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;

        SceneNode* torch = corridor->createChild("CorridorTorch");
        torch->position = { side * (halfW - 0.25f), 4.2f, z };

        add(torch, "Bracket", &m_cylinder,
            { -side * 0.18f, -0.3f, 0.0f }, { 0.14f, 0.8f, 0.14f }, kDarkStone);
        add(torch, "Bowl", &m_cone,
            { -side * 0.32f, 0.05f, 0.0f }, { 0.42f, 0.4f, 0.42f }, kBrass);
        add(torch, "Flame", &m_sphere,
            { -side * 0.32f, 0.32f, 0.0f }, { 0.36f, 0.52f, 0.36f }, kFlame);

        corridorTorches.push_back(torch);
    }

    // --- cover pillars -------------------------------------------------------
    // Tall enough to break Medusa's line of sight, and solid. The collision
    // circle is the BASE radius - the widest part - while the gaze only cares
    // about the shaft, so the two are recorded separately.
    {
        Material shaftMaterial = kSandstone;
        shaftMaterial.uvScale = { 3.0f, 3.0f };

        const float height = Tuning::kPillarHeight;
        const float shaftD = Tuning::kPillarShaftRadius * 2.0f;
        const float baseD  = Tuning::kPillarBaseRadius  * 2.0f;

        for (int i = 0; i < Tuning::kPillarCount; ++i)
        {
            const float px = Tuning::kPillarX[i];
            const float pz = Tuning::kPillarZ[i];

            SceneNode* pillar = corridor->createChild("CoverPillar");
            pillar->position = { px, 0.0f, pz };

            // Every part is remembered, so the whole pillar can fade as one.
            std::vector<SceneNode*> parts;
            parts.push_back(add(pillar, "Shaft", &m_cylinder,
                { 0.0f, height * 0.5f, 0.0f }, { shaftD, height, shaftD }, shaftMaterial));
            parts.push_back(add(pillar, "Plinth", &m_cylinder,
                { 0.0f, 0.3f, 0.0f }, { baseD, 0.6f, baseD }, kDarkStone));
            parts.push_back(add(pillar, "BandLow", &m_torus,
                { 0.0f, 0.95f, 0.0f }, { shaftD, 0.8f, shaftD }, kGold));
            parts.push_back(add(pillar, "BandHigh", &m_torus,
                { 0.0f, height - 1.4f, 0.0f }, { shaftD, 0.8f, shaftD }, kGold));
            parts.push_back(add(pillar, "Capital", &m_cylinder,
                { 0.0f, height - 0.3f, 0.0f }, { baseD, 0.6f, baseD }, kDarkStone));

            m_pillarParts.push_back(parts);
            m_pillarFade.push_back(1.0f);

            coverPillars.push_back({ px, pz, Tuning::kPillarShaftRadius, height });
            propObstacles.push_back({ px, pz, Tuning::kPillarBaseRadius + 0.02f });
        }
    }

    // --- shine stones --------------------------------------------------------
    // Crystal clusters bedded into the walls and floor. Between the torches
    // the corridor was simply dark; these pick out its length and give the
    // eye something to follow while running.
    Material crystal;
    crystal.ka        = { 0.05f, 0.14f, 0.16f };
    crystal.kd        = { 0.14f, 0.52f, 0.58f };
    crystal.ks        = { 0.75f, 0.95f, 1.00f };
    crystal.shininess = 96.0f;
    crystal.emissive  = { 0.22f, 0.78f, 0.86f };
    crystal.opacity   = 0.88f;

    auto addShineCluster = [&](const glm::vec3& position, float tiltZ, float size)
    {
        SceneNode* cluster = corridor->createChild("ShineStone");
        cluster->position = position;
        cluster->rotation = { 0.0f, 0.0f, tiltZ };
        cluster->scale    = glm::vec3(size);

        // A dull socket of rock, so the shards look grown out of the wall
        // rather than stuck onto it.
        add(cluster, "Socket", &m_sphere,
            { 0.0f, 0.04f, 0.0f }, { 0.62f, 0.3f, 0.62f }, kDarkStone);

        // Three shards of different heights leaning apart. One cone reads as
        // a traffic cone; three read as a crystal.
        SceneNode* a = add(cluster, "Shard", &m_cone,
            { 0.0f, 0.52f, 0.0f }, { 0.34f, 1.15f, 0.34f }, crystal);

        SceneNode* b = add(cluster, "Shard", &m_cone,
            { -0.24f, 0.34f, 0.13f }, { 0.22f, 0.78f, 0.22f }, crystal);
        b->rotation = { 0.0f, 0.0f, -24.0f };

        SceneNode* c = add(cluster, "Shard", &m_cone,
            { 0.21f, 0.28f, -0.12f }, { 0.18f, 0.62f, 0.18f }, crystal);
        c->rotation = { 0.0f, 0.0f, 27.0f };

        shineClusters.push_back(cluster);

        for (SceneNode* shard : { a, b, c })
        {
            shineShards.push_back(shard);
            // Spread the twinkle out so they never pulse in unison.
            m_shinePhases.push_back(
                static_cast<float>(shineShards.size()) * 1.37f);
        }
    };

    int shineIndex = 0;
    for (float z = gateZ + 3.0f; z < Tuning::kExitZ - 1.0f; z += 5.0f)
    {
        const float side = (shineIndex % 2 == 0) ? -1.0f : 1.0f;

        // Set into the wall at chest height, leaning out into the corridor.
        addShineCluster({ side * (halfW - 0.12f), 1.9f, z },
                        side * -62.0f, 1.0f);

        // A smaller one low on the opposite side, so both walls carry light.
        addShineCluster({ -side * (halfW - 0.25f), 0.18f, z + 2.4f },
                        -side * 18.0f, 0.62f);

        ++shineIndex;
    }

    // --- the way out ----------------------------------------------------------
    // An archway and a bright doorway, so the goal is visible from the moment
    // the gate opens. A chase needs somewhere to run TO.
    add(corridor, "ExitArch", &m_cube,
        { 0.0f, 6.0f, Tuning::kExitZ },
        { halfW * 2.0f + 1.0f, 2.4f, 0.8f }, kGold);
}

void Scene::buildTreasure()
{
    treasure = m_root->createChild("Treasure");
    treasure->position = kTreasureSpot;
    treasure->visible  = false;          // sunk until the verdict reveals it

    add(treasure, "Pedestal", &m_cylinder,
        { 0.0f, 0.35f, 0.0f }, { 1.5f, 0.7f, 1.5f }, kDarkStone);
    add(treasure, "PedestalTrim", &m_torus,
        { 0.0f, 0.68f, 0.0f }, { 1.45f, 0.5f, 1.45f }, kGold);

    // The jewel spins and bobs inside the assembly, so it keeps moving while
    // the assembly as a whole rises.
    treasureJewel = treasure->createChild("Jewel");
    treasureJewel->position = { 0.0f, kTreasureJewelY, 0.0f };

    // A gem shape from two cones meeting base to base - the cone generator
    // already gives a correct slanted normal, so the facets catch light
    // properly with no new geometry code.
    add(treasureJewel, "GemTop", &m_cone,
        { 0.0f, 0.16f, 0.0f }, { 0.62f, 0.55f, 0.62f }, kTreasure);
    add(treasureJewel, "GemBottom", &m_cone,
        { 0.0f, -0.16f, 0.0f }, { 0.62f, 0.55f, 0.62f }, kTreasure)
        ->rotation = { 180.0f, 0.0f, 0.0f };

    // A halo that counter-rotates, so the silhouette is never static.
    add(treasureJewel, "Halo", &m_torus,
        { 0.0f, 0.0f, 0.0f }, { 1.5f, 0.6f, 1.5f }, kGold);
}

void Scene::update(float time, float deltaTime)
{
    // Exponential ease toward each target. Frame-rate independent, and the
    // same curve the state machine will drive in Phases 7-9.
    auto ease = [deltaTime](float current, float target, float speed)
    {
        const float t = std::min(1.0f, deltaTime * speed);
        return current + (target - current) * t;
    };

    if (directDrive)
    {
        m_lid     = lidTarget;
        m_head    = headTarget;
        m_column  = columnTarget;
        m_petrify = petrifyTarget;
    }
    else
    {
        m_lid     = ease(m_lid,     lidTarget,     3.0f);
        m_head    = ease(m_head,    headTarget,    2.2f);
        m_column  = ease(m_column,  columnTarget,  2.5f);
        m_petrify = ease(m_petrify, petrifyTarget, 1.6f);
    }

    // --- Phase 3 demonstration ---------------------------------------------
    // A slow idle sway on the beam, purely so the hierarchy is visible before
    // the state machine exists. Phase 7 replaces this with the real verdict
    // angle driven by the heart's weight.
    if (beam != nullptr)
    {
        beam->rotation.z = manualBeam ? beamTarget
                                      : 14.0f * std::sin(time * 0.6f);

        // The pans hang from the beam, so without this they would tilt with
        // it. Cancelling the parent's rotation in the child is the clearest
        // demonstration of composed transforms in the whole scene.
        if (panLeft  != nullptr) { panLeft->rotation.z  = -beam->rotation.z; }
        if (panRight != nullptr) { panRight->rotation.z = -beam->rotation.z; }
    }

    // --- the heart -----------------------------------------------------------
    if (heart != nullptr)
    {
        const float t = Easing::clamp01(heartPlace);

        // Ease the horizontal travel, then add a vertical hump. A straight
        // lerp would slide the heart through the pan's rim on the way in.
        const float travel = Easing::smoothstep01(t);

        // Where the heart starts: the traveller's hands. Computed from his
        // CURRENT position rather than a constant, because he is now
        // player-controlled and can be standing anywhere when the trial
        // begins. Converting into the pan's local space is valid for the same
        // reason it always was - that space equals world space up to a
        // translation while the beam is level, and Placing always runs level.
        glm::vec3 home = kHeartHome;
        if (traveller != nullptr && panLeft != nullptr)
        {
            const float yaw = glm::radians(traveller->rotation.y);
            const glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));

            const glm::vec3 handsWorld = traveller->worldPosition()
                                       + glm::vec3(0.0f, 2.10f, 0.0f)
                                       + forward * 0.55f;

            home = glm::vec3(glm::inverse(panLeft->world()) *
                             glm::vec4(handsWorld, 1.0f));
        }

        glm::vec3 position = glm::mix(home, kHeartRest, travel);

        constexpr float kPi = 3.14159265358979323846f;
        position.y += kHeartArcHeight * std::sin(travel * kPi);

        // A slow bob while it waits to be committed, fading out as it leaves.
        position.y += 0.09f * std::sin(time * 1.6f) * (1.0f - travel);

        heart->position = position;

        // Turns over once during the flight, settling square on the pan.
        heart->rotation.y = 360.0f * Easing::easeOutCubic(t);

        // Beats faster while it is being judged.
        const float rate  = 5.0f + 3.5f * travel;
        const float pulse = 1.0f + 0.07f * std::sin(time * rate);

        // Deliberately non-uniform - wider as it flattens - which is exactly
        // the case that needs the inverse-transpose normal matrix.
        heart->scale = glm::vec3(0.6f * pulse, 0.6f / pulse, 0.6f * pulse);
    }

    // Snakes sway, each segment offset in phase so the chain whips rather
    // than swinging rigidly.
    const float agitation = Easing::clamp01(snakeAgitation);

    for (std::size_t i = 0; i < snakeSegments.size(); ++i)
    {
        const float phase = m_snakePhases[i];

        // Roused, the snakes both whip wider and move faster. Amplitude alone
        // reads as slow motion; frequency alone reads as a nervous twitch.
        const float amplitudeX = 7.0f + 15.0f * agitation;
        const float amplitudeZ = 5.0f + 11.0f * agitation;
        const float speed      = 1.8f + 3.4f * agitation;

        snakeSegments[i]->rotation.x =
            12.0f + amplitudeX * std::sin(time * speed + phase);
        snakeSegments[i]->rotation.z =
            amplitudeZ * std::sin(time * speed * 0.72f + phase * 1.4f);
    }

    // --- composite hinge: the lid swings on its rim, not its centre --------
    if (lampLid != nullptr)
    {
        lampLid->rotation.z = 82.0f * m_lid;
    }

    // --- Djinn energy column and its rings ---------------------------------
    if (energyColumn != nullptr)
    {
        energyColumn->visible = m_column > 0.01f;

        const float columnWidth  = 0.55f;
        const float columnHeight = std::max(0.05f, 3.2f * m_column);

        energyColumn->scale = { columnWidth, columnHeight, columnWidth };

        // Rising out of the lamp mouth as it grows.
        energyColumn->position.y = 2.2f + 1.4f * m_column;

        // Translucent so the lamp and the rings read through it.
        energyColumn->material.opacity = 0.42f * m_column;

        constexpr float kPi = 3.14159265358979323846f;

        for (std::size_t i = 0; i < energyRings.size(); ++i)
        {
            // Each ring runs the same 0..1 cycle, offset in phase, so they
            // read as a sequence travelling up the column rather than one
            // pulsing blob. fmod keeps each ring looping independently.
            const float offset = static_cast<float>(i) / static_cast<float>(energyRings.size());
            const float cycle  = std::fmod(time * 0.55f + offset, 1.0f);

            // Expands as it rises: small and tight at the lamp mouth, wide
            // and faint by the time it leaves the top.
            const float radius = 0.7f + 2.9f * Easing::easeOutQuad(cycle);

            // The column's own scale is divided out so the rings keep their
            // real-world proportions however tall the column currently is.
            energyRings[i]->scale = {
                radius / columnWidth,
                0.35f  / columnHeight,
                radius / columnWidth
            };

            energyRings[i]->position.y = (-1.4f + 3.4f * cycle) / columnHeight;

            // sin gives a fade in AND out, so rings neither pop into
            // existence at the lamp mouth nor vanish abruptly at the top.
            energyRings[i]->material.opacity =
                m_column * std::sin(cycle * kPi) * 0.9f;

            // Cool as they expand and lose energy.
            energyRings[i]->material.emissive =
                glm::mix(glm::vec3(0.45f, 0.95f, 1.0f),
                         glm::vec3(0.10f, 0.35f, 0.65f),
                         cycle);
        }
    }

    // --- Medusa's gaze ------------------------------------------------------
    if (medusaHead != nullptr && medusaRoot != nullptr && traveller != nullptr)
    {
        // Aim at the traveller's head, not their feet.
        const glm::vec3 targetWorld = gazeDriven
            ? gazeAim
            : traveller->worldPosition() + glm::vec3(0.0f, 2.35f, 0.0f);

        // Solve the aim in Medusa's own space. Her body is turned -35 degrees,
        // so a world-space angle would be wrong by exactly that much; pulling
        // the target into her local frame makes the body's pose irrelevant.
        const glm::vec3 targetLocal =
            glm::vec3(glm::inverse(medusaRoot->world()) * glm::vec4(targetWorld, 1.0f));

        const glm::vec3 d = targetLocal - medusaHead->position;

        // localMatrix composes R = Ry * Rx * Rz, so the head's forward is
        //   (sinY*cosX, -sinX, cosY*cosX)
        // and these two are its exact inverse.
        const float aimYaw   = glm::degrees(std::atan2(d.x, d.z));
        const float aimPitch = glm::degrees(
            std::atan2(-d.y, std::sqrt(d.x * d.x + d.z * d.z)));

#ifdef TRIAL_DEBUG_AIM
        // Force a full turn so the aim can be checked against the true
        // direction; see the comparison print below.
        m_head = 1.0f;
#endif

#ifdef TRIAL_DEBUG_AIM
        const float follow = 1.0f;
#else
        const float follow = std::min(1.0f, deltaTime * 20.0f);
#endif
        m_headYaw   += Easing::shortestAngleDelta(m_headYaw, aimYaw * m_head) * follow;
        m_headPitch += (aimPitch * m_head - m_headPitch) * follow;

        medusaHead->rotation.y = m_headYaw;
        medusaHead->rotation.x = m_headPitch;
    }

    // --- the gaze made visible -----------------------------------------------
    if (medusaHead != nullptr)
    {
        // Her eyes flare as an attack is charged - the first thing the player
        // should notice - and smoulder the rest of the time.
        const float eyeLevel = gazeDriven
            ? gazeEye
            : (m_head > 0.01f ? 0.3f + 0.7f * m_head : 0.3f);

        for (const char* eyeName : { "EyeL", "EyeR" })
        {
            if (SceneNode* eye = medusaHead->find(eyeName))
            {
                eye->material.emissive = Materials::eye.emissive * (0.6f + 2.6f * eyeLevel);
            }
        }

        if (gazeBeam != nullptr)
        {
            float width  = 0.0f;
            float length = 0.0f;

            if (gazeDriven)
            {
                width  = gazeBeamWidth;
                length = gazeBeamLength;
            }
            else if (m_head > 0.01f && traveller != nullptr)
            {
                // The cursed ending: the gaze simply rests on him.
                width  = Easing::smoothstep01(m_head);
                length = glm::length(traveller->worldPosition()
                                     + glm::vec3(0.0f, 2.35f, 0.0f)
                                     - medusaHead->worldPosition()) + 0.6f;
            }

            gazeBeam->visible = (width > 0.02f && length > 0.8f);

            if (gazeBeam->visible)
            {
                // The full cone matches the angle the gameplay test uses, so
                // what is lit is what hurts.
                const float full = 2.0f * length
                                 * std::tan(glm::radians(Tuning::kGazeConeHalf));
                const float w = full * width;

                gazeBeam->scale    = { w, length, w };
                gazeBeam->position = { 0.0f, 0.08f, 0.35f + length * 0.5f };

                gazeBeam->material.opacity  = 0.07f + 0.17f * width;
                gazeBeam->material.emissive =
                    glm::vec3(0.35f, 0.95f, 0.25f) * (0.5f + 0.9f * width);
            }
        }
    }

    // --- petrification indicator -------------------------------------------
    if (petrifyMeter != nullptr)
    {
        const float height = 0.1f + 2.6f * m_petrify;
        petrifyMeter->scale    = { 0.18f, height, 0.18f };
        petrifyMeter->position = { 1.2f, height * 0.5f, 0.0f };
    }

    // --- the gate ------------------------------------------------------------
    // Always eased, never direct-driven: it is a heavy stone slab and should
    // grind upward regardless of what the state machine is doing.
    m_gate = ease(m_gate, Easing::clamp01(gateTarget), 1.1f);

    if (gate != nullptr)
    {
        constexpr float kDoorHeight = 5.0f;
        gate->position.y = kDoorHeight * 0.5f
                         + kDoorHeight * Easing::smoothstep01(m_gate) * 1.02f;
    }

    // --- shine stones --------------------------------------------------------
    // Emissive only, so this is the whole of their animation.
    for (std::size_t i = 0; i < shineShards.size(); ++i)
    {
        const float pulse = 0.62f + 0.38f * std::sin(time * 1.7f + m_shinePhases[i]);

        shineShards[i]->material.emissive =
            glm::vec3(0.22f, 0.78f, 0.86f) * pulse;
    }

    // --- the treasure --------------------------------------------------------
    if (treasure != nullptr)
    {
        const float reveal = Easing::clamp01(treasureReveal);

        treasure->visible = (reveal > 0.005f) && !treasureTaken;

        // easeOutBack overshoots past 1, so it breaks the floor, rises a
        // little too far and settles - the same curve as the lamp lid, and
        // the reason the arrival reads as an event rather than a slide.
        const float rise = Easing::easeOutBack(reveal);
        treasure->position.y = Easing::mix(kTreasureSunkY, 0.0f, rise);

        if (treasureJewel != nullptr)
        {
            // Turns steadily. Not eased: a constant spin is what makes an
            // object read as "an item", and it never has to stop.
            treasureJewel->rotation.y = std::fmod(time * 52.0f, 360.0f);
            treasureJewel->position.y = kTreasureJewelY
                                      + 0.14f * std::sin(time * 1.7f);

            // A slow tumble on X as well, so the facets catch the torches.
            treasureJewel->rotation.x = 9.0f * std::sin(time * 0.9f);
        }
    }

    // --- the traveller's walk cycle -----------------------------------------
    {
        const float pace = Easing::clamp01(travellerSpeed01);

        // Advance the phase in proportion to speed. Driving it from raw time
        // instead would make the legs cycle at a fixed rate while he slides
        // along at whatever speed he happens to be going - the "moonwalk"
        // that gives away a walk cycle bolted on as an afterthought.
        m_walkPhase += deltaTime * (7.4f * pace);

        // Wrap, or the float loses precision after a long session.
        constexpr float kTwoPi = 6.28318530718f;
        if (m_walkPhase > kTwoPi) { m_walkPhase -= kTwoPi; }

        const float swing = std::sin(m_walkPhase);

        // Amplitude fades with speed, so slowing to a halt eases the limbs
        // back to rest wherever the phase happens to be.
        const float legAmplitude = 36.0f * pace;
        const float armAmplitude = 26.0f * pace;

        if (travellerLegL != nullptr) { travellerLegL->rotation.x =  legAmplitude * swing; }
        if (travellerLegR != nullptr) { travellerLegR->rotation.x = -legAmplitude * swing; }

        // Arms oppose the legs - the left arm goes forward with the right leg.
        if (travellerArmL != nullptr) { travellerArmL->rotation.x = -armAmplitude * swing; }
        if (travellerArmR != nullptr) { travellerArmR->rotation.x =  armAmplitude * swing; }

        if (travellerTorso != nullptr)
        {
            // Bob at twice the stride frequency: the body rises once per step,
            // and there are two steps per full cycle.
            travellerTorso->position.y = 1.75f + 0.05f * pace * std::sin(m_walkPhase * 2.0f);

            // Lean into the run. Positive X rotation tips the top toward local
            // +Z, which is the direction he faces.
            travellerTorso->rotation.x = 7.0f * pace;
        }
    }

    // Torch flames flicker in size; Phase 4 ties light intensity to this.
    // The collapse's torchPower shrinks and dims them on top, down to
    // nothing once a torch has been put out.
    auto powerOf = [this](int id)
    {
        return (id >= 0 && id < static_cast<int>(torchPower.size())) ? torchPower[id] : 1.0f;
    };
    auto dressFlame = [&](SceneNode* flame, const glm::vec3& baseScale, float power)
    {
        flame->scale    = baseScale * (0.35f + 0.65f * power);
        flame->visible  = power > 0.03f;
        flame->material.emissive = kFlame.emissive * (0.25f + 0.75f * power);
    };

    {
        int id = 0;
        for (SceneNode* torch : { torchLeft, torchRight })
        {
            if (torch != nullptr)
            {
                if (SceneNode* flame = torch->find("Flame"))
                {
                    const float f = 1.0f + 0.10f * std::sin(time * 9.0f + torch->position.x);
                    dressFlame(flame, glm::vec3(0.42f, 0.6f, 0.42f) * f, powerOf(id));
                }
            }
            ++id;
        }

        for (std::size_t i = 0; i < corridorTorches.size(); ++i)
        {
            if (SceneNode* flame = corridorTorches[i]->find("Flame"))
            {
                dressFlame(flame, { 0.36f, 0.52f, 0.36f },
                           powerOf(Collapse::kChamberTorches + static_cast<int>(i)));
            }
        }
    }

    m_root->updateWorld();

// Build with -DTRIAL_DEBUG_PLACEMENT to dump the world-space frame the heart's
// arc is defined against. The basis vectors must come back as identity: the
// arc is authored in the pan's local space and only equals world space while
// the beam is level.
#ifdef TRIAL_DEBUG_PLACEMENT
    {
        static bool once = false;
        if (!once)
        {
            once = true;
            const glm::mat4& panWorld = panLeft->world();
            printf("[dbg] pan world   = %.3f %.3f %.3f\n",
                   panWorld[3][0], panWorld[3][1], panWorld[3][2]);
            printf("[dbg] pan basis X = %.3f %.3f %.3f\n",
                   panWorld[0][0], panWorld[0][1], panWorld[0][2]);
            printf("[dbg] pan basis Y = %.3f %.3f %.3f\n",
                   panWorld[1][0], panWorld[1][1], panWorld[1][2]);
            const glm::vec3 h = heart->worldPosition();
            printf("[dbg] heart@home  = %.3f %.3f %.3f\n", h.x, h.y, h.z);
            const glm::vec3 t = traveller->worldPosition();
            printf("[dbg] traveller   = %.3f %.3f %.3f\n", t.x, t.y, t.z);
            fflush(stdout);
        }
    }
#endif

#ifdef TRIAL_DEBUG_AIM
    {
        static bool onceAim = false;
        if (!onceAim && medusaHead != nullptr && traveller != nullptr)
        {
            onceAim = true;

            const glm::vec3 eye  = medusaHead->worldPosition();
            const glm::vec3 want = glm::normalize(
                (traveller->worldPosition() + glm::vec3(0.0f, 2.35f, 0.0f)) - eye);
            const glm::vec3 got  = medusaHead->worldForward();

            printf("[aim] head at   = %.3f %.3f %.3f\n", eye.x, eye.y, eye.z);
            printf("[aim] want dir  = %.3f %.3f %.3f\n", want.x, want.y, want.z);
            printf("[aim] got  dir  = %.3f %.3f %.3f\n", got.x, got.y, got.z);
            printf("[aim] dot       = %.5f  (1.000 = pointing exactly at target)\n",
                   glm::dot(want, got));
            fflush(stdout);
        }
    }
#endif

    // Lights read world positions, so they must be rebuilt only after the
    // transforms above have been flushed through the tree.
    updateLights(time);
}

void Scene::updateLights(float time)
{
    lights.clear();

    // --- two warm torch point lights ---------------------------------------
    // Anchored to the flame nodes, so they inherit any movement for free.
    int chamberTorchId = -1;
    for (SceneNode* torch : { torchLeft, torchRight })
    {
        ++chamberTorchId;
        if (torch == nullptr) { continue; }

        SceneNode* flame = torch->find("Flame");
        if (flame == nullptr) { continue; }

        const float power = torchPower[chamberTorchId];
        if (power < 0.02f) { continue; }

        Light torchLight;
        torchLight.type      = LightType::Point;
        torchLight.position  = flame->worldPosition();
        torchLight.color     = { 1.0f, 0.58f, 0.24f };

        // Two summed sines at unrelated frequencies read as an irregular
        // flicker; a single sine reads as a mechanical pulse.
        const float seed = torch->position.x;
        torchLight.intensity = (2.5f
                                + 0.30f * std::sin(time * 8.3f + seed)
                                + 0.16f * std::sin(time * 19.7f + seed * 2.0f))
                             * power;

        torchLight.constant  = 1.0f;
        torchLight.linear    = 0.07f;
        torchLight.quadratic = 0.012f;

        lights.push_back(torchLight);
    }

    // --- the glowing heart carries its own light ----------------------------
    // Anchored to the heart node, so it rides the arc and then the pan tilt
    // for free. Brightest in flight, when it is the focus of the shot.
    if (heart != nullptr && heart->visible)
    {
        Light heartLight;
        heartLight.type      = LightType::Point;
        heartLight.position  = heart->worldPosition();
        heartLight.color     = { 1.0f, 0.22f, 0.26f };
        heartLight.intensity = 1.5f + 1.6f * Easing::smoothstep01(heartPlace);

        // Tight falloff: it should pool on the pan, not light the room.
        heartLight.constant  = 1.0f;
        heartLight.linear    = 0.22f;
        heartLight.quadratic = 0.14f;

        lights.push_back(heartLight);
    }

    // --- the treasure's own glow ---------------------------------------------
    if (treasure != nullptr && treasure->visible && treasureJewel != nullptr)
    {
        Light glow;
        glow.type      = LightType::Point;
        glow.position  = treasureJewel->worldPosition();
        glow.color     = { 1.0f, 0.82f, 0.36f };

        // Pulses, so it draws the eye across the room the moment it appears.
        glow.intensity = (3.0f + 0.7f * std::sin(time * 3.1f))
                       * Easing::clamp01(treasureReveal);

        glow.constant  = 1.0f;
        glow.linear    = 0.10f;
        glow.quadratic = 0.028f;

        lights.push_back(glow);
    }

    // --- Djinn energy: a cold point light with sharp falloff ----------------
    if (energyColumn != nullptr && m_column > 0.01f)
    {
        Light djinn;
        djinn.type      = LightType::Point;
        djinn.position  = energyColumn->worldPosition() + glm::vec3(0.0f, 1.2f, 0.0f);
        djinn.color     = { 0.22f, 0.85f, 1.0f };
        djinn.intensity = 5.0f * m_column;

        // Much tighter attenuation than the torches, so it reads as a local
        // magical glow rather than room lighting.
        djinn.constant  = 1.0f;
        djinn.linear    = 0.14f;
        djinn.quadratic = 0.06f;

        lights.push_back(djinn);
    }

    // --- Medusa's eyes: two narrow spotlights -------------------------------
    const float spotLevel = gazeDriven ? gazeEye : m_head;
    if (medusaHead != nullptr && spotLevel > 0.01f)
    {
        const glm::vec3 gaze = medusaHead->worldForward();

        for (const char* eyeName : { "EyeL", "EyeR" })
        {
            SceneNode* eye = medusaHead->find(eyeName);
            if (eye == nullptr) { continue; }

            Light spot;
            spot.type      = LightType::Spot;
            spot.position  = eye->worldPosition();
            spot.direction = gaze;
            spot.color     = { 0.55f, 1.0f, 0.35f };
            // Fiercer as the stone takes hold, so the gaze is visibly doing
            // the work rather than merely pointing at him.
            spot.intensity = 6.0f * spotLevel * (1.0f + 1.1f * m_petrify);

            spot.constant  = 1.0f;
            spot.linear    = 0.05f;
            spot.quadratic = 0.010f;

            // Narrow cone with a soft edge, per PROJECT_PLAN section 5.
            spot.innerAngle = 12.0f;
            spot.outerAngle = 18.0f;

            lights.push_back(spot);
        }
    }

    // --- corridor torches, nearest few only ---------------------------------
    // There are five down the corridor and a hard cap on how many lights the
    // shader can take. Uploading only the closest keeps the count bounded
    // however long the corridor gets, and the far ones contribute almost
    // nothing anyway once attenuation has had its way with them.
    if (!corridorTorches.empty() && traveller != nullptr)
    {
        constexpr int kCorridorTorchBudget = 2;

        const glm::vec3 listener = traveller->worldPosition();

        // Small fixed-size selection rather than a sort: with five candidates
        // and a budget of two, repeatedly taking the nearest is cheaper and
        // allocates nothing.
        std::vector<const SceneNode*> chosen;
        std::vector<float> chosenPower;
        std::vector<bool> used(corridorTorches.size(), false);

        for (int pick = 0; pick < kCorridorTorchBudget; ++pick)
        {
            int   best = -1;
            float bestDistance = 0.0f;

            for (std::size_t i = 0; i < corridorTorches.size(); ++i)
            {
                if (used[i]) { continue; }

                // A torch that has been put out lights nothing; spend the
                // budget on the next lit one instead.
                if (torchPower[Collapse::kChamberTorches + i] < 0.05f) { continue; }

                const glm::vec3 d = corridorTorches[i]->worldPosition() - listener;
                const float d2 = glm::dot(d, d);

                if (best < 0 || d2 < bestDistance)
                {
                    best = static_cast<int>(i);
                    bestDistance = d2;
                }
            }

            if (best < 0) { break; }
            used[best] = true;
            chosen.push_back(corridorTorches[best]);
            chosenPower.push_back(torchPower[Collapse::kChamberTorches + best]);
        }

        for (std::size_t c = 0; c < chosen.size(); ++c)
        {
            const SceneNode* torch = chosen[c];
            const SceneNode* flame = const_cast<SceneNode*>(torch)->find("Flame");
            if (flame == nullptr) { continue; }

            Light light;
            light.type     = LightType::Point;
            light.position = flame->worldPosition();
            light.color    = { 1.0f, 0.55f, 0.22f };

            const float seed = torch->position.z;
            light.intensity = (2.2f
                               + 0.28f * std::sin(time * 7.9f + seed)
                               + 0.14f * std::sin(time * 17.3f + seed * 2.0f))
                            * chosenPower[c];

            light.constant  = 1.0f;
            light.linear    = 0.08f;
            light.quadratic = 0.015f;

            lights.push_back(light);
        }
    }

    // --- the nearest shine stone casts for real ------------------------------
    // Just one. Emissive geometry glows but lights nothing, so without this
    // the crystals would float in a black corridor. One is enough to pool
    // colour on the floor, and keeps the light count comfortably inside the
    // shader's twelve.
    if (!shineClusters.empty() && traveller != nullptr)
    {
        const glm::vec3 listener = traveller->worldPosition();

        const SceneNode* best = nullptr;
        float bestDistance = 0.0f;

        for (const SceneNode* cluster : shineClusters)
        {
            const glm::vec3 d = cluster->worldPosition() - listener;
            const float d2 = glm::dot(d, d);

            if (best == nullptr || d2 < bestDistance)
            {
                best = cluster;
                bestDistance = d2;
            }
        }

        if (best != nullptr)
        {
            Light shine;
            shine.type      = LightType::Point;
            shine.position  = best->worldPosition() + glm::vec3(0.0f, 0.4f, 0.0f);
            shine.color     = { 0.28f, 0.82f, 0.92f };
            shine.intensity = 2.0f + 0.5f * std::sin(time * 1.7f);
            shine.constant  = 1.0f;
            shine.linear    = 0.14f;
            shine.quadratic = 0.05f;

            lights.push_back(shine);
        }
    }

    // --- the way out ----------------------------------------------------------
    // A cool, bright pool at the exit, so the goal reads from far down the
    // corridor. It is the only thing in the chamber lit like daylight.
    {
        Light exitLight;
        exitLight.type      = LightType::Point;
        exitLight.position  = { 0.0f, 4.2f, Tuning::kExitZ + 1.5f };
        exitLight.color     = { 0.80f, 0.90f, 1.0f };
        exitLight.intensity = 5.5f + 14.0f * Easing::clamp01(exitGlow);
        exitLight.constant  = 1.0f;
        exitLight.linear    = 0.05f;
        exitLight.quadratic = 0.006f;
        lights.push_back(exitLight);
    }

    // --- dim blue directional fill ------------------------------------------
    // Stops the far walls reading as pure black without flattening the scene.
    Light fill;
    fill.type      = LightType::Directional;
    fill.direction = { -0.3f, -1.0f, -0.45f };
    fill.color     = { 0.30f, 0.36f, 0.62f };
    fill.intensity = 0.22f;
    lights.push_back(fill);
}

void Scene::buildCeilingWork()
{
    // --- what can fall ---------------------------------------------------------
    // The same list main and the tests use, so the blocks that are drawn are
    // exactly the blocks that are simulated.
    const std::vector<Collapse::Spec> layout = Collapse::standardLayout();

    Material blockMaterial = kStone;
    blockMaterial.uvScale = { 2.0f, 0.6f };

    // Anything a falling section or its staying half already occupies is left
    // out of the static masonry, or there would be two blocks in one place.
    auto occupied = [&layout](const glm::vec3& centre)
    {
        for (const Collapse::Spec& spec : layout)
        {
            if (glm::length(spec.home - centre) < 0.5f) { return true; }
            if (spec.hasCompanion && glm::length(spec.companionHome - centre) < 0.5f)
            {
                return true;
            }
        }
        return false;
    };

    // --- the chamber's cornice ---------------------------------------------------
    // A ledge of blocks round the top of the walls. It gives the chamber a
    // structure that can visibly fail - before, the walls simply stopped.
    // Only 2 deep at the front and sides and 1.2 at the back, where the
    // statue's ears come up close to the wall.
    SceneNode* cornice = m_root->createChild("Cornice");
    constexpr float kLedgeY = 7.6f;
    constexpr float kLedgeH = 0.7f;
    constexpr float kGap    = 0.05f;

    auto ledgeRow = [&](float from, float to, int pieces, bool alongX,
                        float across, float depth)
    {
        const float step = (to - from) / static_cast<float>(pieces);
        for (int k = 0; k < pieces; ++k)
        {
            const float along = from + step * (static_cast<float>(k) + 0.5f);
            const glm::vec3 centre = alongX ? glm::vec3(along, kLedgeY, across)
                                            : glm::vec3(across, kLedgeY, along);
            if (occupied(centre)) { continue; }

            const glm::vec3 size = alongX ? glm::vec3(step - kGap, kLedgeH, depth)
                                          : glm::vec3(depth, kLedgeH, step - kGap);
            add(cornice, "CorniceBlock", &m_cube, centre, size, blockMaterial);
        }
    };

    ledgeRow(-12.7f, 12.7f, 7, true,  10.2f, 2.0f);   // front, over the doorway
    ledgeRow(-10.7f,  9.2f, 5, false, -11.7f, 2.0f);  // left
    ledgeRow(-10.7f,  9.2f, 5, false,  11.7f, 2.0f);  // right
    ledgeRow(-10.7f, 10.7f, 6, true, -10.1f, 1.2f);   // back

    // --- the corridor's ceiling beams --------------------------------------------
    // Cross-beams under the ceiling, midway between the pillars. The ones that
    // break are built below in two halves instead.
    SceneNode* beams = m_root->createChild("CeilingBeams");
    for (int k = 0; k < 8; ++k)
    {
        const float z = 16.5f + 7.0f * static_cast<float>(k);
        bool breaks = false;
        for (const Collapse::Spec& spec : layout)
        {
            if (spec.hingeAlongZ && std::fabs(spec.home.z - z) < 0.5f) { breaks = true; }
        }
        if (breaks) { continue; }

        add(beams, "CeilingBeam", &m_cube, { 0.0f, 6.7f, z },
            { Tuning::kCorridorHalfWidth * 2.0f, 0.6f, 1.0f }, blockMaterial);
    }

    // --- the sections that fall ----------------------------------------------------
    for (std::size_t i = 0; i < layout.size(); ++i)
    {
        const Collapse::Spec& spec = layout[i];
        CollapsePiece piece;

        // A mesh-less parent, posed by the collapse. The block hangs off it
        // as a child so the cracks are not stretched by the block's scale.
        piece.node = m_root->createChild("CeilingSection");
        piece.node->position = spec.home;
        add(piece.node, "Block", &m_cube, { 0.0f, 0.0f, 0.0f }, spec.size, blockMaterial);
        addCracks(piece.node, spec.size, static_cast<int>(i) * 2, piece.cracks);

        if (spec.hasCompanion)
        {
            piece.companion = m_root->createChild("CeilingSectionStays");
            piece.companion->position = spec.companionHome;
            add(piece.companion, "Block", &m_cube, { 0.0f, 0.0f, 0.0f },
                spec.companionSize, blockMaterial);
            addCracks(piece.companion, spec.companionSize,
                      static_cast<int>(i) * 2 + 1, piece.companionCracks);
        }

        m_collapsePieces.push_back(piece);
    }
}

void Scene::addCracks(SceneNode* parent, const glm::vec3& size, int seed,
                      std::vector<Crack>& out)
{
    // Thin dark slivers laid on the underside and on the -Z face, the two
    // faces the chase camera sees as he runs at them. They start at zero
    // length and grow, one after another, as the section cracks.
    Material crackMaterial;
    crackMaterial.ka = { 0.01f, 0.01f, 0.01f };
    crackMaterial.kd = { 0.03f, 0.025f, 0.02f };
    crackMaterial.ks = { 0.0f, 0.0f, 0.0f };
    crackMaterial.shininess = 4.0f;

    std::mt19937 rng(9001u + static_cast<unsigned>(seed) * 977u);
    auto random = [&rng](float low, float high)
    {
        std::uniform_real_distribution<float> dist(low, high);
        return dist(rng);
    };

    const glm::vec3 h = size * 0.5f;
    const bool alongX = size.x >= size.z;
    const float lengthHalf = alongX ? h.x : h.z;
    const float widthHalf  = alongX ? h.z : h.x;

    struct Segment { glm::vec3 a, b; bool onFace; };
    std::vector<Segment> segments;

    // Underside: a zigzag down the long axis, with two branches off it.
    const float under = -h.y - 0.012f;
    auto onUnderside = [&](float along, float lateral)
    {
        return alongX ? glm::vec3(along, under, lateral) : glm::vec3(lateral, under, along);
    };

    glm::vec3 points[4];
    for (int k = 0; k < 4; ++k)
    {
        const float along = -0.85f * lengthHalf + 1.7f * lengthHalf * static_cast<float>(k) / 3.0f;
        points[k] = onUnderside(along, random(-0.6f, 0.6f) * widthHalf);
    }
    for (int k = 0; k < 3; ++k) { segments.push_back({ points[k], points[k + 1], false }); }

    for (int k = 1; k <= 2; ++k)
    {
        const float lateral = (k == 1 ? 1.0f : -1.0f) * random(0.5f, 0.85f) * widthHalf;
        const float along = random(-0.35f, 0.35f) * lengthHalf;
        const glm::vec3 p = points[k];
        const glm::vec3 offset = alongX ? glm::vec3(along, 0.0f, lateral)
                                        : glm::vec3(lateral, 0.0f, along);
        segments.push_back({ p, p + offset * 0.6f, false });
    }

    // The -Z face: two cracks running down from the top edge.
    const float face = -h.z - 0.012f;
    for (int k = 0; k < 2; ++k)
    {
        const float x0 = random(-0.7f, 0.7f) * h.x;
        const glm::vec3 top(x0, h.y * 0.95f, face);
        const glm::vec3 mid(x0 + random(-0.3f, 0.3f), random(-0.2f, 0.2f) * h.y, face);
        const glm::vec3 bottom(mid.x + random(-0.3f, 0.3f), -h.y * 0.95f, face);
        segments.push_back({ top, mid, true });
        segments.push_back({ mid, bottom, true });
    }

    const float n = static_cast<float>(segments.size());
    for (std::size_t k = 0; k < segments.size(); ++k)
    {
        const Segment& seg = segments[k];
        const glm::vec3 d = seg.b - seg.a;
        const float length = glm::length(d);
        if (length < 1e-3f) { continue; }

        Crack crack;
        crack.start = seg.a;
        crack.direction = d / length;
        crack.length = length;
        crack.threshold = 0.6f * static_cast<float>(k) / n;

        crack.node = add(parent, "Crack", &m_cube, seg.a, { 0.001f, 0.001f, 0.001f },
                         crackMaterial);
        if (seg.onFace)
        {
            crack.node->rotation = { 0.0f, 0.0f, glm::degrees(std::atan2(d.y, d.x)) };
            crack.node->scale    = { 0.001f, 0.10f, 0.025f };
        }
        else
        {
            crack.node->rotation = { 0.0f, glm::degrees(std::atan2(-d.z, d.x)), 0.0f };
            crack.node->scale    = { 0.001f, 0.025f, 0.10f };
        }
        crack.node->visible = false;

        out.push_back(crack);
    }
}

void Scene::growCracks(std::vector<Crack>& cracks, float level)
{
    for (Crack& crack : cracks)
    {
        const float g = Easing::clamp01((level - crack.threshold) / 0.3f);
        crack.node->visible = g > 0.02f;

        // Grows from its start point, so it reads as a crack running
        // across the stone rather than a line fading in.
        const float length = std::max(0.001f, crack.length * g);
        crack.node->scale.x  = length;
        crack.node->position = crack.start + crack.direction * (length * 0.5f);
    }
}

void Scene::applyCollapse(const Collapse& collapse, float time)
{
    const int n = std::min(collapse.count(), static_cast<int>(m_collapsePieces.size()));
    for (int i = 0; i < n; ++i)
    {
        CollapsePiece& piece = m_collapsePieces[i];

        piece.node->position = collapse.position(i);
        piece.node->rotation = collapse.rotation(i);
        growCracks(piece.cracks, collapse.crack(i));

        if (piece.companion != nullptr)
        {
            piece.companion->position = collapse.companionPosition(i);
            piece.companion->rotation = collapse.companionRotation(i);
            growCracks(piece.companionCracks, collapse.crack(i));
        }
    }

    for (int t = 0; t < static_cast<int>(torchPower.size()); ++t)
    {
        torchPower[t] = collapse.torchLevel(t, time);
    }
}

glm::vec3 Scene::torchFlamePosition(int torch) const
{
    SceneNode* holder = nullptr;
    if (torch == 0)      { holder = torchLeft; }
    else if (torch == 1) { holder = torchRight; }
    else if (torch - Collapse::kChamberTorches < static_cast<int>(corridorTorches.size())
             && torch >= Collapse::kChamberTorches)
    {
        holder = corridorTorches[torch - Collapse::kChamberTorches];
    }

    if (holder == nullptr) { return glm::vec3(0.0f); }
    if (SceneNode* flame = holder->find("Flame")) { return flame->worldPosition(); }
    return holder->worldPosition();
}

void Scene::fadeCoverBetween(const glm::vec3& eye, const glm::vec3& target,
                             float deltaTime)
{
    const float follow = std::min(1.0f, deltaTime * 10.0f);

    for (std::size_t i = 0; i < m_pillarParts.size() && i < coverPillars.size(); ++i)
    {
        const CoverPillar& c = coverPillars[i];

        // The segment eye -> target against the pillar's footprint, with a
        // margin: the pillar should ghost a moment BEFORE it covers him, not
        // after, or the traveller pops in and out at its edge.
        const float reach = c.radius + 0.45f;

        const float dx = target.x - eye.x;
        const float dz = target.z - eye.z;
        const float fx = eye.x - c.x;
        const float fz = eye.z - c.z;

        bool occludes = false;
        const float A = dx * dx + dz * dz;
        if (A > 1e-6f && std::min(eye.y, target.y) < c.height)
        {
            const float B = 2.0f * (fx * dx + fz * dz);
            const float C = fx * fx + fz * fz - reach * reach;
            const float disc = B * B - 4.0f * A * C;
            if (disc >= 0.0f)
            {
                const float root = std::sqrt(disc);
                const float t1 = (-B - root) / (2.0f * A);
                const float t2 = (-B + root) / (2.0f * A);
                occludes = (t2 >= 0.0f && t1 <= 1.0f);
            }
        }

        const float want = occludes ? 0.25f : 1.0f;
        m_pillarFade[i] += (want - m_pillarFade[i]) * follow;

        // Snap back to fully opaque once it is close, so the pillar returns
        // to the opaque pass (and its shadow) instead of lingering at 0.999.
        const float shown = (m_pillarFade[i] > 0.985f) ? 1.0f : m_pillarFade[i];
        for (SceneNode* part : m_pillarParts[i])
        {
            part->material.opacity = shown;
        }
    }
}

void Scene::draw(const Shader& shader, const glm::vec3& cameraPosition) const
{
    if (m_root == nullptr)
    {
        return;
    }

    // --- petrification uniforms, shared by every draw this frame ------------
    shader.setFloat("uPetrify", m_petrify);
    shader.setFloat("uPetrifyBaseY",
                    traveller != nullptr ? traveller->worldPosition().y : 0.0f);
    shader.setFloat("uPetrifyHeight", kTravellerHeight);

    shader.setVec3 ("uStoneKa", Materials::petrified.ka);
    shader.setVec3 ("uStoneKd", Materials::petrified.kd);
    shader.setVec3 ("uStoneKs", Materials::petrified.ks);
    shader.setFloat("uStoneShininess", Materials::petrified.shininess);

    // --- pass 1: opaque, depth writes on ------------------------------------
    m_root->draw(shader);

    // --- pass 2: transparent, sorted back-to-front --------------------------
    std::vector<const SceneNode*> transparent;
    m_root->collectTransparent(transparent);

    if (transparent.empty())
    {
        return;
    }

    // Farthest first. Alpha blending is order-dependent: a near surface drawn
    // before a far one blends against the background instead of against what
    // is actually behind it.
    std::sort(transparent.begin(), transparent.end(),
              [&cameraPosition](const SceneNode* a, const SceneNode* b)
              {
                  // Squared distance is enough for ordering and avoids a sqrt.
                  const glm::vec3 da = a->worldPosition() - cameraPosition;
                  const glm::vec3 db = b->worldPosition() - cameraPosition;
                  return glm::dot(da, da) > glm::dot(db, db);
              });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Depth TEST stays on so the rings are correctly hidden behind the lamp
    // and walls. Depth WRITES go off, so overlapping rings do not occlude
    // each other and blend properly instead.
    glDepthMask(GL_FALSE);

    // The rings are thin shells; culling their back faces would drop half of
    // each one and make them look like broken arcs. Remember whether culling
    // was on rather than assuming - the B key can have turned it off.
    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);
    glDisable(GL_CULL_FACE);

    for (const SceneNode* node : transparent)
    {
        node->drawSelf(shader);
    }

    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Scene::drawDepth(const Shader& depthShader) const
{
    if (m_root != nullptr)
    {
        m_root->drawDepth(depthShader);
    }
}

glm::vec3 Scene::shadowLightPosition() const
{
    // Must match lights[kShadowLightIndex] exactly, or the shadows will be
    // cast from somewhere the light visibly is not.
    if (torchLeft != nullptr)
    {
        if (const SceneNode* flame = const_cast<SceneNode*>(torchLeft)->find("Flame"))
        {
            return flame->worldPosition();
        }
    }

    return { -7.0f, 4.75f, -10.25f };
}

glm::vec3 Scene::treasureWorldPosition() const
{
    if (treasureJewel != nullptr)
    {
        return treasureJewel->worldPosition();
    }
    return kTreasureSpot + glm::vec3(0.0f, kTreasureJewelY, 0.0f);
}

glm::vec3 Scene::djinnEmitterPosition() const
{
    if (energyColumn != nullptr)
    {
        // The column node sits at the lamp mouth; drop to its base so the
        // smoke appears to pour out of the lamp rather than mid-air.
        return energyColumn->worldPosition() - glm::vec3(0.0f, 1.2f, 0.0f);
    }

    return { -6.0f, 2.2f, 1.5f };
}

void Scene::drawLightMarkers(const Shader& shader) const
{
    Material marker;
    marker.ka = { 0.0f, 0.0f, 0.0f };
    marker.kd = { 0.0f, 0.0f, 0.0f };
    marker.ks = { 0.0f, 0.0f, 0.0f };

    // These bypass drawSelf(), so the flag has to be cleared by hand - the
    // last node drawn may well have been part of the traveller.
    shader.setInt("uPetrifyEnabled", 0);

    for (const Light& light : lights)
    {
        // A directional light has no position to mark.
        if (light.type == LightType::Directional) { continue; }

        marker.emissive = light.color;
        marker.upload(shader);

        glm::mat4 model(1.0f);
        model = glm::translate(model, light.position);
        model = glm::scale(model, glm::vec3(0.22f));

        shader.setMat4("uModel", model);
        shader.setMat3("uNormalMatrix", glm::mat3(1.0f));

        m_sphere.draw();
    }
}
