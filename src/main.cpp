#include <algorithm>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Camera.h"
#include "CameraDirector.h"
#include "Debris.h"
#include "Gaze.h"
#include "Collapse.h"
#include "Fireflies.h"
#include "GatePuzzle.h"
#include "DjinnForm.h"
#include "Hud.h"
#include "EscapeTuning.h"
#include "Easing.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Pursuer.h"
#include "PostProcess.h"
#include "Scene.h"
#include "Shader.h"
#include "ShadowMap.h"

namespace
{
    int gWindowWidth  = 1280;
    int gWindowHeight = 720;

    Camera gCamera;
    CameraDirector gDirector;

    // How the scene is being watched. TAB cycles.
    enum class ViewMode
    {
        Cinematic,    // the director frames each beat; WASD walks the traveller
        FirstPerson,  // through the traveller's own eyes; mouse aims
        FreeCamera    // detached; WASD flies the camera, the traveller stands
    };

    ViewMode gViewMode = ViewMode::Cinematic;

    const char* viewModeName(ViewMode mode)
    {
        switch (mode)
        {
            case ViewMode::Cinematic:   return "Cinematic";
            case ViewMode::FirstPerson: return "First person";
            case ViewMode::FreeCamera:  return "Free camera";
        }
        return "?";
    }

    // Is a point somewhere a camera may legitimately sit? The level is a
    // wide chamber joined to a narrow corridor, and an orbit camera happily
    // swings straight through a wall without this.
    // Pillars the camera must not end up inside: x, z, radius.
    struct CameraBlocker { float x, z, radius; };

    // Once the gate is down the camera may follow him into the garden.
    bool gGardenOpen = false;

#ifdef TRIAL_AUTOPLAY
    // Diagnostic: the frame as the GPU drew it, saved as a BMP. Independent of
    // the desktop's screen capture, which can stop seeing an OpenGL window.
    void saveFrameBmp(const std::string& path, int w, int h)
    {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(w) * h * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

        const int row = (w * 3 + 3) & ~3;
        const unsigned dataSize = static_cast<unsigned>(row * h);
        const unsigned fileSize = 54u + dataSize;
        unsigned char header[54] = { 'B', 'M' };
        auto put32 = [&header](int at, unsigned v)
        {
            for (int i = 0; i < 4; ++i) { header[at + i] = static_cast<unsigned char>(v >> (8 * i)); }
        };
        put32(2, fileSize);  put32(10, 54u);  put32(14, 40u);
        put32(18, static_cast<unsigned>(w));  put32(22, static_cast<unsigned>(h));
        header[26] = 1;  header[28] = 24;  put32(34, dataSize);

        FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) { return; }
        std::fwrite(header, 1, 54, f);
        const unsigned char pad[3] = { 0, 0, 0 };
        for (int y = 0; y < h; ++y)     // GL rows are bottom-up, as BMP's are
        {
            std::fwrite(&pixels[static_cast<std::size_t>(y) * w * 3], 1, static_cast<std::size_t>(w) * 3, f);
            std::fwrite(pad, 1, static_cast<std::size_t>(row - w * 3), f);
        }
        std::fclose(f);
    }
#endif
    std::vector<CameraBlocker> gCameraBlockers;

    bool cameraPositionIsInside(const glm::vec3& p, const glm::vec3& subject,
                                bool allowThroughDoorway)
    {
        if (p.y < 0.7f) { return false; }              // through the floor

        // In the thickness of the front wall, anywhere but the open doorway.
        // The chase camera rides at about 5 high and the doorway is only 5
        // tall, so following him through it used to put the camera inside
        // the lintel for a moment - a frame of solid brown.
        if (std::fabs(p.z - Tuning::kGateZ) < 0.6f
            && (p.y > 4.8f || std::fabs(p.x) > 3.3f))
        {
            return false;
        }

        // Following him out through the doorway: until the camera is through
        // it, it has to stay below the lintel, or the wall above the door
        // fills the whole screen for the moment he passes under it. Only when
        // what it is looking at is already at the door - the overview shots
        // that look down into the chamber from above are left alone.
        if (subject.z > Tuning::kGateZ - 1.0f
            && p.z > Tuning::kGateZ - 3.0f && p.z < Tuning::kGateZ + 0.6f
            && p.y > 4.5f)
        {
            return false;
        }

        // Inside a pillar means looking out at the back of its faces. Pulling
        // the camera toward its subject until it is clear reads as it easing
        // past an obstruction.
        for (const CameraBlocker& b : gCameraBlockers)
        {
            const float dx = p.x - b.x;
            const float dz = p.z - b.z;
            const float reach = b.radius + 0.35f;
            if (p.y < Tuning::kPillarHeight && dx * dx + dz * dz < reach * reach)
            {
                return false;
            }
        }

        // The front wall separates the chamber from the corridor. A camera on
        // the far side of it from its subject is looking through stone - even
        // though the spot it is standing in is itself perfectly valid, which
        // is exactly why checking the position alone was not enough.
        if (!allowThroughDoorway)
        {
            const bool cameraBeyond  = p.z       > Tuning::kGateZ;
            const bool subjectBeyond = subject.z > Tuning::kGateZ;
            if (cameraBeyond != subjectBeyond) { return false; }
        }

        if (p.z <= 11.0f)
        {
            // The chamber has no ceiling, so looking down from high up is
            // fine; only the walls, the floor and the cornice of blocks round
            // the top of the walls constrain it.
            const bool inCornice = p.y > 7.0f && p.y < 8.2f
                                && (std::fabs(p.x) > 10.4f || p.z > 8.9f || p.z < -9.2f);
            return std::fabs(p.x) <= 12.2f && p.z >= -10.2f && !inCornice;
        }

        // The corridor is enclosed, so the ceiling matters here.
        // The cross-beams hang down to 6.4.
        // The garden is open to the sky: only its walls constrain it.
        if (gGardenOpen && p.z > Tuning::kGardenFrontZ + 0.4f)
        {
            return std::fabs(p.x) <= Tuning::kGardenHalfWidth - 0.3f && p.y <= 16.0f
                && p.z <= Tuning::kGardenBackZ - 0.4f;
        }

        // ...and the constellation gate closes the far end - until it opens.
        return std::fabs(p.x) <= 3.9f && p.y <= 6.2f
            && p.z <= (gGardenOpen ? Tuning::kGardenFrontZ + 0.4f : Tuning::kGateSlabZ - 0.6f);
    }

    // Pulls an orbit camera in along its own view ray until it is back
    // inside. Shortening the distance keeps the framing and the angle; only
    // the standoff changes, which reads as the camera easing past an
    // obstruction rather than jumping.
    void keepCameraOutOfWalls(Camera& camera, TrialState state)
    {
        if (camera.mode() != CameraMode::Orbit) { return; }

        // The chase camera legitimately follows him through the open doorway,
        // so it is the one shot allowed to straddle the front wall.
        const bool chasing = (state == TrialState::Escape);

        const glm::vec3 subject = camera.target();

        if (cameraPositionIsInside(camera.position(), subject, chasing)) { return; }

        const float wanted = camera.distance();

        for (int i = 1; i <= 24; ++i)
        {
            const float shrunk = wanted * (1.0f - i / 25.0f);
            if (shrunk < 1.5f) { break; }

            camera.setDistance(shrunk);
            if (cameraPositionIsInside(camera.position(), subject, chasing)) { return; }
        }

        // Nowhere on the ray works; sit as close in as allowed.
        camera.setDistance(1.5f);
    }

    // Seconds left before the director takes the camera back after the user
    // has dragged it. Without this, one stray drag froze the camera for the
    // rest of the session.
    float  gDirectorResume = 0.0f;

    bool   gDragging   = false;
    double gLastMouseX = 0.0;
    double gLastMouseY = 0.0;

    void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
    {
        gWindowWidth  = width;
        gWindowHeight = height;
        glViewport(0, 0, width, height);
    }

    void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT)
        {
            return;
        }

        if (action == GLFW_PRESS)
        {
            gDragging = true;
            glfwGetCursorPos(window, &gLastMouseX, &gLastMouseY);
        }
        else if (action == GLFW_RELEASE)
        {
            gDragging = false;
        }
    }

    void cursorPosCallback(GLFWwindow* /*window*/, double xpos, double ypos)
    {
        if (!gDragging)
        {
            return;
        }

        const float dx = static_cast<float>(xpos - gLastMouseX);
        const float dy = static_cast<float>(ypos - gLastMouseY);

        gLastMouseX = xpos;
        gLastMouseY = ypos;

        // In first person the mouse IS the aim, so the director is already
        // off and there is nothing to hand back.
        if (gViewMode == ViewMode::Cinematic)
        {
            // Taking hold of the camera hands control to the user - fighting
            // the director for the view would be maddening - but only for a
            // moment. It takes the shot back once the hands come off.
            gDirector.setEnabled(false);
            gDirectorResume = 2.5f;
        }

        gCamera.look(dx, dy);
    }

    void scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
    {
        gCamera.zoom(static_cast<float>(yoffset));
    }

    // Returns true on the frame a key goes down. Keeps the toggle handling in
    // the main loop from firing every frame the key is held.
    bool pressed(GLFWwindow* window, int key, bool& heldFlag)
    {
        const bool down = glfwGetKey(window, key) == GLFW_PRESS;
        const bool fired = down && !heldFlag;
        heldFlag = down;
        return fired;
    }
}

int main()
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        gWindowWidth,
        gWindowHeight,
        "The Trial of Three Curses",
        nullptr,
        nullptr
    );

    if (window == nullptr)
    {
        std::cout << "Failed to create OpenGL 3.3 window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer:       " << glGetString(GL_RENDERER) << '\n';

    glViewport(0, 0, gWindowWidth, gWindowHeight);
    glEnable(GL_DEPTH_TEST);

    // Phase 3 check: with culling on, any triangle wound the wrong way simply
    // disappears. Press B to compare against culling off.
    bool cullFaces = true;
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    Shader shader;
    if (!shader.load("shaders/phong.vert", "shaders/phong.frag"))
    {
        std::cout << "Shader load failed. Run from the project root so the "
                     "shaders/ folder resolves.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    Shader depthShader;
    ShadowMap shadowMap;

    bool shadowsAvailable = depthShader.load("shaders/depth.vert",
                                             "shaders/depth.frag")
                         && shadowMap.init(1024);

    if (!shadowsAvailable)
    {
        std::cout << "Shadow mapping unavailable; rendering without it.\n";
    }

    PostProcess post;
    const bool postAvailable = post.init(gWindowWidth, gWindowHeight);
    if (!postAvailable)
    {
        std::cout << "Post-processing unavailable; rendering direct.\n";
    }

    ParticleSystem particles;
    const bool particlesAvailable = particles.init(700);
    if (!particlesAvailable)
    {
        std::cout << "Particles unavailable; rendering without them.\n";
    }

    Scene scene;
    scene.build();

    // The constellation gate's puzzle, and the fireflies that give its answer.
    // They hover just in front of the rings.
    GatePuzzle puzzle;
    puzzle.setLayout({ scene.gateCentre.x, scene.gateCentre.y, Tuning::kGateSlabZ - 1.2f },
                     scene.gateBand, scene.gateSocketRadius);
    puzzle.reset(1u, scene.gateCentre);
    bool puzzleActive = false;      // from the sanctuary until the next run
    bool patternShownOnce = false;
    bool ringUpHeld = false, ringDownHeld = false, ringLeftHeld = false, ringRightHeld = false;
    bool autoSolveHeld = false;

    // The garden: what is solid there, and the fireflies round each relic.
    Fireflies gardenFlies[3];
    for (int i = 0; i < 3 && i < static_cast<int>(scene.gardenRelics.size()); ++i)
    {
        const glm::vec3 r = scene.gardenRelics[i];
        gardenFlies[i].init(22, r + glm::vec3(-1.8f, -1.6f, -1.8f), r + glm::vec3(1.8f, 1.2f, 1.8f),
                            101u + static_cast<unsigned>(i));
    }
    float gardenOpenLevel = 0.0f;

    // The offering: seconds since he laid the jewel on the altar; < 0 not yet.
    float offerTime = -1.0f;
    glm::vec3 offerFrom(0.0f);
    std::mt19937 sparkleRng(2718u);

    // The Djinn, made of smoke, who rises when the lamp opens.
    DjinnForm djinn;
    djinn.init();

    // Fireflies in the chamber: below the cornice, inside the walls.
    Fireflies fireflies;
    fireflies.init(60, { -11.5f, 0.8f, -8.5f }, { 11.5f, 5.2f, 10.0f });

    TrialController trial;

    Player player;
    player.reset({ 0.0f, 0.0f, 5.5f }, 180.0f);
    player.setBounds({ Tuning::kRoomMinX, Tuning::kRoomMinZ },
                     { Tuning::kRoomMaxX, Tuning::kRoomMaxZ });
    player.setCorridor(Tuning::kGateZ, 3.5f,
                       Tuning::kCorridorHalfWidth, Tuning::kExitZ);

    // Medusa is confined by exactly the same geometry as the player.
    WorldShape world;
    world.gateZ             = Tuning::kGateZ;
    world.doorHalfWidth     = 3.5f;
    world.corridorHalfWidth = Tuning::kCorridorHalfWidth;
    world.exitZ             = Tuning::kExitZ;

    // The standing props are solid for both of them. The radii were recorded
    // next to the geometry as the scene was built, rather than written out
    // again here where they could drift apart from it.
    for (const Scene::PropObstacle& prop : scene.propObstacles)
    {
        player.addObstacle(prop.x, prop.z, prop.radius);
        world.addObstacle(prop.x, prop.z, prop.radius);

        std::cout << "[Collision] prop at (" << prop.x << ", " << prop.z
                  << ") radius " << prop.radius << std::endl;
    }

    // The garden's pedestals, pool and palms. Only he ever walks there.
    for (const Scene::PropObstacle& o : scene.gardenObstacles)
    {
        player.addObstacle(o.x, o.z, o.radius);
    }

    // The ceiling sections that can fall. Where each one will lie is known
    // up front, so its floor circles are registered now, switched off, and
    // switched on the moment it lands - for Medusa as much as for him.
    Collapse collapse;
    for (const Collapse::Spec& spec : Collapse::standardLayout())
    {
        collapse.add(spec);
    }
    collapse.reset();

    std::vector<std::vector<int>> collapsePlayerIds(collapse.count());
    std::vector<std::vector<int>> collapseWorldIds(collapse.count());
    for (int i = 0; i < collapse.count(); ++i)
    {
        for (const Collapse::Circle& c : collapse.footprint(i))
        {
            collapsePlayerIds[i].push_back(player.addObstacle(c.x, c.z, c.radius, false, true));
            collapseWorldIds[i].push_back(world.addObstacle(c.x, c.z, c.radius, false, true));
        }
    }
    std::cout << "[Collapse] " << collapse.count() << " ceiling sections ready" << std::endl;

    // The constellation gate closes the corridor's far end.
    const float gateFaceZ = Tuning::kGateSlabZ - 0.3f;
    player.setEndWall(gateFaceZ);
    world.endZ = gateFaceZ;

    // Where she rests between runs, captured before anything moves her.
    const glm::vec3 medusaHome =
        (scene.medusaRoot != nullptr) ? scene.medusaRoot->position
                                      : glm::vec3(6.0f, 0.0f, -1.5f);
    const float medusaHomeHeading =
        (scene.medusaRoot != nullptr) ? scene.medusaRoot->rotation.y : -35.0f;

    Pursuer pursuer;
    pursuer.setWorld(world);
    pursuer.reset(medusaHome, medusaHomeHeading, true);

    auto setCollapseSolid = [&](int section, bool solid)
    {
        for (int id : collapsePlayerIds[section]) { player.setObstacleActive(id, solid); }
        for (int id : collapseWorldIds[section])  { pursuer.setObstacleActive(id, solid); }
    };
    auto clearCollapse = [&]()
    {
        for (int i = 0; i < collapse.count(); ++i) { setCollapseSolid(i, false); }
    };

    Debris debris;
    debris.init(Tuning::kMaxStones);
    debris.setCorridor(Tuning::kGateZ, Tuning::kCorridorHalfWidth,
                       Tuning::kExitZ, 6.8f);
    debris.setMaterial(scene.rubbleMaterial);

    // Medusa's gaze. Its cover comes straight from the scene's pillars, so the
    // line-of-sight test and the geometry can never disagree about where a
    // pillar stands.
    Gaze gaze;
    gaze.setDoorway(Tuning::kGateZ, 3.5f);
    for (const Scene::CoverPillar& pillar : scene.coverPillars)
    {
        gaze.addCover(pillar.x, pillar.z, pillar.radius, pillar.height);
        debris.addAvoid(pillar.x, pillar.z, Tuning::kPillarBaseRadius);
    }
    gaze.reset();

    // Random stones should not drop onto where a beam will be lying.
    for (int i = 0; i < collapse.count(); ++i)
    {
        const Collapse::Spec& spec = collapse.spec(i);
        if (spec.startAt >= 0.0f) { continue; }
        const float side = (spec.rest.x > 0.0f) ? 1.0f : -1.0f;
        debris.addAvoid(side * 1.0f, spec.rest.y, 1.0f);
        debris.addAvoid(side * 3.0f, spec.rest.y, 1.0f);
    }

    for (const Scene::CoverPillar& pillar : scene.coverPillars)
    {
        gCameraBlockers.push_back({ pillar.x, pillar.z, Tuning::kPillarBaseRadius });
    }

    // Wall torches too. The camera hugs the wall when the traveller does, and
    // hiding behind pillars makes that more likely - a flame filling a quarter
    // of the screen is not a view anyone wants.
    for (SceneNode* torch : scene.corridorTorches)
    {
        gCameraBlockers.push_back({ torch->worldPosition().x,
                                    torch->worldPosition().z, 0.55f });
    }

    std::cout << "[Gaze] " << scene.coverPillars.size()
              << " cover pillars registered" << std::endl;

    // How much of the traveller has turned to stone so far. Held across the
    // moment he is caught, so the shader does not snap back to flesh and
    // climb all over again.
    float petrifyHold = 0.0f;

    // The sanctuary: how far raised, and the flare where Medusa hits it.
    float sanctuaryLevel = 0.0f;
    float sanctuaryFlash = 0.0f;
    bool  medusaAtDome   = false;

    // Decaying camera shake, driven by impacts.
    float cameraShake = 0.0f;

    // Paces the repeating particle work in the two ending states.
    float endingPulse = 0.0f;

    // 0 = she is far away, 1 = about to be caught.
    float chaseDanger = 0.0f;

    // The state as of the END of the previous frame. Transitions are detected
    // against this rather than around trial.update(), because the three that
    // matter most - taking the treasure, reaching the exit, being caught -
    // are fired by events much later in the frame.
    TrialState lastTrialState = trial.state();

    Hud hud;
    if (!hud.init())
    {
        // Not fatal - the scene is still perfectly watchable without the
        // overlay, and the title bar carries the same information.
        std::cout << "HUD failed to initialise; continuing without it.\n";
    }

    // Start already on the Waiting shot, rather than at an arbitrary spot the
    // director then has to drag the camera away from. The old default sat at
    // z = 12.6, outside the front wall, so the very first frame opened up
    // looking at the inside of a block of stone.
    gCamera.setTarget({ 0.0f, 2.6f, -2.0f });
    gCamera.setDistance(15.0f);
    gCamera.setYaw(85.0f);
    gCamera.setPitch(32.0f);

    float lastFrame  = 0.0f;
    float fpsTimer   = 0.0f;
    int   frameCount = 0;

    bool reloadHeld  = false;
    bool modeHeld    = false;
    bool normalsHeld = false;
    bool wireHeld    = false;
    bool cullHeld    = false;
    bool lidHeld     = false;
    bool headHeld    = false;
    bool columnHeld  = false;
    bool petrifyHeld = false;
    bool resetHeld   = false;
    bool markerHeld  = false;
    bool textureHeld = false;
    bool beginHeld   = false;
    bool trialResetHeld = false;
    bool forceGoodHeld  = false;
    bool forceBadHeld   = false;
    bool inspectHeld    = false;
    bool cinematicHeld  = false;
    bool hudHeld        = false;
    bool showHud        = true;
    bool shadowHeld     = false;
    bool useShadows     = true;
    bool particleHeld   = false;
    bool useParticles   = true;
    bool postHeld       = false;
    bool usePost        = true;
    bool normalMapHeld  = false;
    bool useNormalMaps  = true;
    bool treasureHeld   = false;
    bool viewHeld       = false;

    // When true the trial stops driving the scene and the Phase 3 keys take
    // over, for inspecting one mechanism in isolation.
    bool inspectMode = false;

    bool debugNormals    = false;
    bool wireframe       = false;
    bool showLightMarkers = false;
    bool useTextures      = true;

    std::cout << "\nControls\n"
              << "  Left-drag  orbit / look\n"
              << "  Scroll     zoom (orbit distance, or FOV in free-fly)\n"
              << "  TAB        cycle view: Cinematic / First person / Free camera\n"
              << "  C          jump to the free camera and back\n"
              << "  WASD QE    fly the camera (free-fly mode only)\n"
              << "  N          debug view: normals as RGB\n"
              << "  T          toggle wireframe\n"
              << "  B          toggle back-face culling\n"
              << "  L          toggle light position markers\n"
              << "  Y          toggle textures on / off\n"
              << "  F          toggle the cinematic camera (drag also cancels it)\n"
              << "  H          toggle the on-screen bars\n"
              << "  G          toggle shadows\n"
              << "  J          toggle Djinn smoke particles\n"
              << "  U          toggle post-processing\n"
              << "  V          toggle normal mapping\n"
              << "  F5         reload shaders\n"
              << "  ESC        quit\n"
              << "\nThe traveller\n"
              << "  WASD or    walk the traveller, camera-relative\n"
              << "  arrows     (available while Waiting)\n"
              << "\nThe trial\n"
              << "  SPACE      place the heart and begin\n"
              << "  - / =      set the heart's weight (0.38-0.62 passes)\n"
              << "  1 / 2      force a balanced / cursed verdict\n"
              << "  3          debug: get caught\n"
              << "  R          abandon the trial and reset\n"
              << "  I          switch between trial and manual inspection\n"
              << "\nThe constellation gate (at the end of the corridor)\n"
              << "  The charm raises a sanctuary; it lasts 35 seconds.\n"
              << "  Watch the fireflies form a pattern on the gate: one star per ring.\n"
              << "  Up/Down choose a ring, Left/Right turn it, Space shows the pattern again.\n"
              << "  Point every ring's gold arrow at its star to open the gate.\n"
              << "  Enter solves it for you, so you can watch the rings turn.\n"
              << "\nMedusa's gaze (during the escape)\n"
              << "  Green caps flash either side of the top bar = she is about to look.\n"
              << "  Her beam sweeps the whole corridor: running or strafing will NOT save\n"
              << "  you. Get a tall PILLAR between you and her until it passes (about 1 s).\n"
              << "  The top bar fills as you turn to stone; full bar = petrified.\n"
              << "\nManual inspection (press I first)\n"
              << "  [ ]        tilt the scale beam by hand\n"
              << "  O          open / shut the lamp lid (hinge pivot)\n"
              << "  M          turn Medusa's head\n"
              << "  K          show / hide the Djinn energy column\n"
              << "  P          grow / shrink the petrification meter\n"
              << "  X          reset all of the above\n"
              << std::endl;

    while (!glfwWindowShouldClose(window))
    {
        const float now = static_cast<float>(glfwGetTime());

        // Clamped: the first frame arrives seconds after glfwInit, because
        // the scene, its textures and the shadow map are all built in
        // between. An unclamped delta teleports everything on frame one.
        const float deltaTime = std::min(now - lastFrame, 0.05f);
        lastFrame = now;

        ++frameCount;
        fpsTimer += deltaTime;
        if (fpsTimer >= 0.25f)
        {
            char title[256];
            snprintf(title, sizeof(title),
                     "The Trial of Three Curses  |  %.0f FPS  |  %s  |  weight %.2f (%s)  |  %s",
                     frameCount / fpsTimer,
                     inspectMode ? "INSPECT" : trial.stateName(),
                     trial.heartWeight(),
                     trial.isBalanced() ? "balanced" : "unbalanced",
                     viewModeName(gViewMode));
            glfwSetWindowTitle(window, title);

            fpsTimer   = 0.0f;
            frameCount = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        if (pressed(window, GLFW_KEY_F5, reloadHeld))  { shader.reload(); }
        if (pressed(window, GLFW_KEY_C, modeHeld))
        {
            gViewMode = (gViewMode == ViewMode::FreeCamera) ? ViewMode::Cinematic
                                                            : ViewMode::FreeCamera;
            gCamera.setMode(gViewMode == ViewMode::FreeCamera ? CameraMode::FreeFly
                                                              : CameraMode::Orbit);
            gDirector.setEnabled(gViewMode == ViewMode::Cinematic);
            std::cout << "[View] " << viewModeName(gViewMode) << std::endl;
        }
        if (pressed(window, GLFW_KEY_N,  normalsHeld)) { debugNormals = !debugNormals; }
        if (pressed(window, GLFW_KEY_L,  markerHeld))  { showLightMarkers = !showLightMarkers; }
        if (pressed(window, GLFW_KEY_Y,  textureHeld)) { useTextures = !useTextures; }
        if (pressed(window, GLFW_KEY_H,  hudHeld))     { showHud = !showHud; }
        if (pressed(window, GLFW_KEY_G,  shadowHeld))  { useShadows = !useShadows; }
        if (pressed(window, GLFW_KEY_J,  particleHeld)) { useParticles = !useParticles; }
        if (pressed(window, GLFW_KEY_U,  postHeld))     { usePost = !usePost; }
        if (pressed(window, GLFW_KEY_V,  normalMapHeld)) { useNormalMaps = !useNormalMaps; }

        // Debug: lose on demand. Without this you have to deliberately run
        // badly every time you want to check the losing ending, which gets
        // old within about two attempts.
        if (pressed(window, GLFW_KEY_3, treasureHeld))
        {
            trial.caught();
        }

        // TAB cycles the three ways of watching.
        if (pressed(window, GLFW_KEY_TAB, viewHeld))
        {
            gViewMode = (gViewMode == ViewMode::Cinematic)  ? ViewMode::FirstPerson
                      : (gViewMode == ViewMode::FirstPerson) ? ViewMode::FreeCamera
                                                             : ViewMode::Cinematic;

            if (gViewMode == ViewMode::Cinematic)
            {
                gCamera.setMode(CameraMode::Orbit);
                gDirector.setEnabled(true);
            }
            else
            {
                // Both other modes drive position and orientation directly,
                // which is what FreeFly already is underneath.
                gCamera.setMode(CameraMode::FreeFly);
                gDirector.setEnabled(false);
            }

            std::cout << "[View] " << viewModeName(gViewMode) << std::endl;
        }

        if (pressed(window, GLFW_KEY_F, cinematicHeld))
        {
            gDirector.setEnabled(!gDirector.enabled());
            std::cout << "[Camera] director "
                      << (gDirector.enabled() ? "on" : "off") << std::endl;
        }

        if (pressed(window, GLFW_KEY_T, wireHeld))
        {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }

        if (pressed(window, GLFW_KEY_B, cullHeld))
        {
            cullFaces = !cullFaces;
            if (cullFaces) { glEnable(GL_CULL_FACE); } else { glDisable(GL_CULL_FACE); }
        }

        // WASD and the arrows are read once into forward/strafe, then given
        // to whichever owns them: the camera while free-flying, otherwise the
        // traveller. Only one consumer, so they can never fight.
        // At the gate the arrows turn the rings instead, and only WASD walks.
        const bool arrowsWalk = (trial.state() != TrialState::Sanctuary);
        auto key = [window](int k) { return glfwGetKey(window, k) == GLFW_PRESS; };

        float inputForward = 0.0f;
        float inputStrafe  = 0.0f;
        if (key(GLFW_KEY_W) || (arrowsWalk && key(GLFW_KEY_UP)))    { inputForward += 1.0f; }
        if (key(GLFW_KEY_S) || (arrowsWalk && key(GLFW_KEY_DOWN)))  { inputForward -= 1.0f; }
        if (key(GLFW_KEY_D) || (arrowsWalk && key(GLFW_KEY_RIGHT))) { inputStrafe  += 1.0f; }
        if (key(GLFW_KEY_A) || (arrowsWalk && key(GLFW_KEY_LEFT)))  { inputStrafe  -= 1.0f; }

        // --- the gate's rings -----------------------------------------------------
        if (trial.state() == TrialState::Sanctuary)
        {
            if (pressed(window, GLFW_KEY_UP,    ringUpHeld))    { puzzle.select(-1); }
            if (pressed(window, GLFW_KEY_DOWN,  ringDownHeld))  { puzzle.select(+1); }
            if (pressed(window, GLFW_KEY_RIGHT, ringRightHeld)) { puzzle.rotate(+1); }
            if (pressed(window, GLFW_KEY_LEFT,  ringLeftHeld))  { puzzle.rotate(-1); }

            // Enter: solve it for me, slowly enough to watch.
            if (pressed(window, GLFW_KEY_ENTER, autoSolveHeld) && !puzzle.autoSolving())
            {
                puzzle.startAutoSolve();
                std::cout << "[Gate] solving the rings for you - watch them turn" << std::endl;
            }
        }

        // Ownership follows the VIEW mode, not the camera mode: first person
        // also runs the camera in FreeFly, but there WASD must still walk.
        const bool cameraOwnsMovement = (gViewMode == ViewMode::FreeCamera);

        if (cameraOwnsMovement)
        {
            glm::vec3 moveDir(inputStrafe, 0.0f, inputForward);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { moveDir.y += 1.0f; }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { moveDir.y -= 1.0f; }
            gCamera.move(moveDir, deltaTime);
        }

        // --- the traveller ----------------------------------------------------
        // Step A: he walks only while Waiting. Step E extends this to the
        // treasure and the escape. During the weighing he must stand still -
        // the heart's flight arc is measured from where he is standing.
        const bool playerHasControl = !cameraOwnsMovement
                                   && !inspectMode
                                   && trial.playerHasControl();

        glm::vec3 walkDirection(0.0f);
        if (playerHasControl)
        {
            // Camera-relative, flattened onto the floor: pressing forward
            // means "away from the camera", which is what a third-person
            // control scheme has to mean when the camera swings around.
            glm::vec3 camForward = gCamera.front();
            camForward.y = 0.0f;

            if (glm::dot(camForward, camForward) < 1e-6f)
            {
                camForward = glm::vec3(0.0f, 0.0f, -1.0f);   // camera looking straight down
            }
            camForward = glm::normalize(camForward);

            const glm::vec3 camRight =
                glm::normalize(glm::cross(camForward, glm::vec3(0.0f, 1.0f, 0.0f)));

            walkDirection = camForward * inputForward + camRight * inputStrafe;
        }

        // In first person the mouse decides which way he faces, so his
        // heading is an input rather than a consequence of his velocity.
        // Leaving auto-facing on would have the two fight each other.
        player.setAutoFace(gViewMode != ViewMode::FirstPerson);
        if (gViewMode == ViewMode::FirstPerson)
        {
            player.setHeading(gCamera.yaw());
        }

        // --- the collapse ------------------------------------------------------
        {
            const bool struck = debris.update(deltaTime, player.position());

            if (struck)
            {
                // A stun is a speed CAP, not a scaled input - update()
                // normalises the input direction, so scaling it would do
                // nothing at all.
                player.stun(Tuning::kStunDuration);
                std::cout << "[Stone] a block clips the traveller" << std::endl;
            }

            // Settled rubble is solid.
            debris.pushPlayerOut(player);

            // Dust where each one landed.
            glm::vec3 impact;
            while (particlesAvailable && debris.popImpact(impact))
            {
                particles.burst(impact + glm::vec3(0.0f, 0.2f, 0.0f),
                                30, glm::vec3(0.62f, 0.55f, 0.45f), 2.6f);
            }

            cameraShake = std::max(cameraShake, debris.shake());
        }

        // --- the building coming apart -------------------------------------------
        {
            collapse.setTriggering(trial.state() == TrialState::Escape);
            collapse.update(deltaTime, player.position());

            const glm::vec3 dust(0.50f, 0.44f, 0.34f);
            Collapse::Event event;
            while (collapse.popEvent(event))
            {
                const Collapse::Spec& spec = collapse.spec(event.section);

                switch (event.type)
                {
                    case Collapse::Event::Type::Crack:
                        std::cout << "[Collapse] " << spec.name << " cracks" << std::endl;
                        if (particlesAvailable)
                        {
                            particles.sprinkle(event.at, event.extent, event.count, dust);
                        }
                        break;

                    case Collapse::Event::Type::Dust:
                        if (particlesAvailable)
                        {
                            particles.sprinkle(event.at, event.extent, event.count, dust);
                        }
                        break;

                    case Collapse::Event::Type::Detach:
                        std::cout << "[Collapse] " << spec.name << " breaks away" << std::endl;
                        debris.dropChunks(event.at, event.count,
                                          std::max(event.extent.x, event.extent.z));
                        if (particlesAvailable)
                        {
                            particles.sprinkle(event.at, event.extent, 12, dust);
                        }
                        break;

                    case Collapse::Event::Type::Land:
                    {
                        std::cout << "[Collapse] " << spec.name << " comes down"
                                  << (event.hitPlayer ? " ON the traveller" : "") << std::endl;

                        setCollapseSolid(event.section, true);

                        if (particlesAvailable)
                        {
                            // Thrown out along its whole length, not from one point.
                            const bool alongX = event.extent.x >= event.extent.z;
                            for (int k = -1; k <= 1; ++k)
                            {
                                const float f = 0.6f * static_cast<float>(k);
                                const glm::vec3 along = alongX
                                    ? glm::vec3(event.extent.x * f, 0.0f, 0.0f)
                                    : glm::vec3(0.0f, 0.0f, event.extent.z * f);
                                particles.burst(event.at + along, 20, dust, 3.4f);
                            }
                        }

                        // A block of masonry is worse than a stone.
                        if (event.hitPlayer && trial.state() == TrialState::Escape)
                        {
                            player.stun(Tuning::kStunDuration * 1.5f);
                        }
                        break;
                    }

                    case Collapse::Event::Type::Snuff:
                        std::cout << "[Collapse] the falling stone puts out torch "
                                  << event.torch << std::endl;
                        if (particlesAvailable)
                        {
                            particles.burst(scene.torchFlamePosition(event.torch), 26,
                                            glm::vec3(0.30f, 0.27f, 0.24f), 1.1f);
                        }
                        break;
                }
            }

            cameraShake = std::max(cameraShake, collapse.shake());
        }

        cameraShake = std::max(0.0f, cameraShake - deltaTime * 2.2f);

        // --- the chase ---------------------------------------------------------
        pursuer.setGateOpen(scene.gateLevel() > 0.85f);
        // When the gate opens she turns from its light and slithers away.
        pursuer.update(trial.state() == TrialState::Garden ? glm::vec3(0.0f, 0.0f, 14.0f)
                                                            : player.position(),
                       deltaTime);

        // --- the sanctuary holds her back -------------------------------------
        // The dome grows from nothing, so as it rises it shoves her back
        // with it rather than her appearing on the far side of it.
        sanctuaryLevel = 0.0f;
        if (trial.state() == TrialState::Sanctuary)
        {
            sanctuaryLevel = Easing::smoothstep01(trial.stateTime() / Tuning::kSanctuaryRise);

            const glm::vec3 centre(0.0f, 0.0f, Tuning::kSanctuaryCentreZ);
            const bool touching = pursuer.holdOutside(
                centre, Tuning::kSanctuaryRadius * sanctuaryLevel);

            // A flare the moment she hits it, and a steady glow while she leans on it.
            if (touching && !medusaAtDome) { sanctuaryFlash = 1.0f; }
            if (touching) { sanctuaryFlash = std::max(sanctuaryFlash, 0.45f); }
            medusaAtDome = touching;
        }
        else
        {
            medusaAtDome = false;
        }
        sanctuaryFlash = std::max(0.0f, sanctuaryFlash - deltaTime * 2.5f);

        // Solved: the dome is no longer needed and fades.
        if (trial.state() == TrialState::Garden && puzzleActive)
        {
            sanctuaryLevel = 1.0f - Easing::smoothstep01(trial.stateTime() / 1.2f);
        }

        // --- the gate opening -------------------------------------------------------
        if (trial.state() == TrialState::Garden)
        {
            const float before = gardenOpenLevel;
            gardenOpenLevel = Easing::clamp01((trial.stateTime() - Tuning::kGateOpenDelay)
                                              / Tuning::kGateOpenTime);

            // A rumble while it goes down.
            if (gardenOpenLevel > 0.0f && gardenOpenLevel < 1.0f)
            {
                cameraShake = std::max(cameraShake, 0.35f);
            }
            if (before <= 0.0f && gardenOpenLevel > 0.0f)
            {
                std::cout << "[Gate] it sinks into the floor" << std::endl;
                if (particlesAvailable)
                {
                    for (float x = -3.5f; x <= 3.5f; x += 1.75f)
                    {
                        particles.burst({ x, 0.3f, Tuning::kGateSlabZ - 0.5f }, 25,
                                        glm::vec3(0.55f, 0.48f, 0.38f), 2.5f);
                    }
                }
            }

            // --- the offering, and the wave of life ---------------------------
            if (gGardenOpen && offerTime < 0.0f)
            {
                const glm::vec3 a = scene.gardenAltar;
                const float dx = player.position().x - a.x;
                const float dz = player.position().z - a.z;
                if (dx * dx + dz * dz < Tuning::kOfferReach * Tuning::kOfferReach)
                {
                    offerTime = 0.0f;
                    offerFrom = player.position() + glm::vec3(0.0f, 2.0f, 0.0f);
                    std::cout << "[Garden] he lays the treasure on the altar" << std::endl;
                }
            }

            // Down far enough to step over: the way into the garden is open.
            if (before < 0.85f && gardenOpenLevel >= 0.85f)
            {
                player.setEndWall(1.0e9f);
                player.setGardenOpen(true);
                gGardenOpen = true;
                std::cout << "[Garden] the way is open" << std::endl;
            }
        }


        // --- the constellation gate's puzzle -----------------------------------
        if (puzzleActive)
        {
            // The fireflies show the pattern once on their own, as soon as the
            // camera has turned to the gate.
            if (trial.state() == TrialState::Sanctuary && !patternShownOnce
                && trial.stateTime() >= 4.6f && puzzle.requestPattern())
            {
                patternShownOnce = true;
                std::cout << "[Gate] the fireflies form the pattern - remember it" << std::endl;
            }

            puzzle.update(deltaTime, now);

            if (trial.state() == TrialState::Sanctuary && puzzle.solved())
            {
                std::cout << "[Gate] the rings lock into place" << std::endl;
                trial.gateSolved();
            }
        }

        if (scene.medusaRoot != nullptr)
        {
            pursuer.applyTo(*scene.medusaRoot);
        }

        if (trial.state() == TrialState::Escape && pursuer.hasCaught(player.position()))
        {
#ifndef TRIAL_AUTOPLAY_IMMORTAL     // diagnostic: let the bot reach the gate every run
            trial.caught();
#endif
        }

        // --- Medusa's gaze -----------------------------------------------------
        {
            const bool escaping = (trial.state() == TrialState::Escape);

            // She may begin an attack once her wind-up is over and he is in the
            // corridor. She does not need to be in it herself: the line-of-sight
            // test already refuses to gaze through the front wall, and waiting
            // for her to arrive wasted nearly half of the run.
            gaze.setActive(escaping && !pursuer.windingUp()
                           && player.position().z > Tuning::kGateZ + 1.5f);

            if (escaping)
            {
                const glm::vec3 eye = (scene.medusaHead != nullptr)
                    ? scene.medusaHead->worldPosition()
                    : pursuer.position() + glm::vec3(0.0f, 3.85f, 0.0f);

                gaze.update(eye, player.position(), deltaTime);
                player.setExposureSlow(gaze.exposure());

#ifdef TRIAL_AUTOPLAY_IMMORTAL
                if (false)
#else
                if (gaze.caught())
#endif
                {
                    std::cout << "[Gaze] full exposure - the stone takes hold"
                              << std::endl;
                    trial.caught();
                }
            }
            else
            {
                player.setExposureSlow(0.0f);
            }

            // Stone is not undone by the state changing underneath it.
            if (escaping)                                 { petrifyHold = gaze.exposure(); }
            else if (trial.state() != TrialState::Caught) { petrifyHold = 0.0f; }
        }

        // How close she is, 0..1. Shared by the chase camera and the HUD so
        // they can never disagree about how much trouble he is in.
        {
            const float gap = pursuer.distanceTo(player.position());
            const float nearness = glm::clamp(
                1.0f - (gap - Tuning::kCatchRadius) / 12.0f, 0.0f, 1.0f);

            chaseDanger = (trial.state() == TrialState::Escape)
                        ? nearness * nearness
                        : 0.0f;
        }

        // While she is still coiled she is scenery, and scenery is solid.
        // Once she is chasing, touching her means being caught, so the
        // push-out must stop or he could never be caught at all.
        if (!pursuer.active())
        {
            player.pushOutOf(pursuer.position(), 1.45f + Tuning::kPlayerRadius);
        }

        // The snakes thrash harder the faster she is moving, so the threat is
        // legible from the silhouette alone.
        if (pursuer.active())
        {
            // While she is uncoiling the snakes are what shows she has woken,
            // since nothing else about her is moving yet.
            scene.snakeAgitation = pursuer.windingUp()
                                 ? 0.35f + 0.65f * pursuer.windUp()
                                 : 0.45f + 0.55f * pursuer.speedFraction();
        }

        // --- the endings -------------------------------------------------------
        if (particlesAvailable)
        {
            endingPulse -= deltaTime;

            if (trial.state() == TrialState::Caught && endingPulse <= 0.0f)
            {
                endingPulse = 0.13f;

                // Motes crawling up him, tracking the petrification front so
                // the effect and the shader agree on where the stone is.
                const float frontY = 3.0f * scene.petrifyLevel();
                particles.burst(player.position() + glm::vec3(0.0f, frontY, 0.0f),
                                10, glm::vec3(0.38f, 0.80f, 0.32f), 1.3f);
            }
            else if (trial.state() == TrialState::Escaped && endingPulse <= 0.0f)
            {
                endingPulse = 0.16f;
                particles.burst({ 0.0f, 2.0f, Tuning::kExitZ + 1.0f },
                                14, glm::vec3(1.0f, 0.90f, 0.60f), 3.4f);
            }
        }

        // --- did he get out? ---------------------------------------------------
        // Close to the gate, the charm raises the sanctuary.
        if (trial.state() == TrialState::Escape
            && player.position().z >= Tuning::kSanctuaryTriggerZ)
        {
            trial.reachedSanctuary();
        }

        if (trial.state() == TrialState::Escape
            && player.position().z >= Tuning::kExitZ)
        {
            trial.reachedExit();
        }

        // --- react to the state having changed ---------------------------------
        // Placed after every call that can change it: trial.update() for the
        // timed beats, and treasureCollected / reachedExit / caught for the
        // ones the player causes.
        if (trial.state() != lastTrialState)
        {
            const TrialState from = lastTrialState;
            const TrialState to   = trial.state();
            lastTrialState = to;

            // The story has moved on, so the camera should be looking at the
            // new thing whatever the user was doing a moment ago.
            if (gViewMode == ViewMode::Cinematic)
            {
                gDirector.setEnabled(true);
                gDirectorResume = 0.0f;
            }

            if (to == TrialState::Escape)
            {
                // Only a blessed traveller ever reaches the escape, so there
                // is no longer a handicap to apply - the chase runs at full
                // difficulty for everyone who gets this far.
                pursuer.reset(medusaHome, medusaHomeHeading, false);
                pursuer.setActive(true);
                gaze.reset();

                debris.reset();
                debris.setIntensity(1.0f);
                debris.setActive(true);

                // Taking the treasure is what brings the place down.
                collapse.reset();
                clearCollapse();
                collapse.begin();

                std::cout << "[Medusa] gives chase" << std::endl;
            }
            else if (from == TrialState::Escape && to == TrialState::Sanctuary)
            {
                // She keeps coming - the dome is what stops her now. The
                // stones stop, and her gaze cannot cross it.
                gaze.reset();
                debris.setActive(false);
                std::cout << "[Sanctuary] " << Tuning::kSanctuaryDuration
                          << " seconds before the charm is spent" << std::endl;
                std::cout << "[Gate] Up/Down choose a ring, Left/Right turn it, "
                             "Space asks the fireflies again, Enter solves it" << std::endl;

                // A fresh pattern every run; the swarm streams out of the charm.
                puzzle.reset(static_cast<unsigned>(now * 1000.0f), scene.charmWorldPosition());
                puzzleActive = true;
                patternShownOnce = false;
                if (particlesAvailable)
                {
                    particles.burst(scene.charmWorldPosition(), 90,
                                    glm::vec3(0.35f, 0.9f, 1.0f), 4.0f);
                }
            }
            else if (from == TrialState::Sanctuary && to == TrialState::Garden)
            {
                // She does not catch him now - she retreats from the light.
                gaze.reset();
                debris.setActive(false);
                if (particlesAvailable)
                {
                    // The constellation flares as the rings lock.
                    for (int k = 0; k < GatePuzzle::kRings; ++k)
                    {
                        particles.burst(puzzle.notchPosition(k, puzzle.target(k)), 30,
                                        glm::vec3(0.45f, 1.0f, 1.0f), 2.5f);
                    }
                }
            }
            else if (from == TrialState::Escape || from == TrialState::Sanctuary)
            {
                if (from == TrialState::Sanctuary && to == TrialState::Caught)
                {
                    std::cout << "[Sanctuary] the charm is spent; the dome falls" << std::endl;
                    if (particlesAvailable)
                    {
                        // The dome breaking up into motes.
                        for (int k = 0; k < 5; ++k)
                        {
                            const float a = glm::radians(72.0f * static_cast<float>(k));
                            particles.burst({ std::sin(a) * 2.5f, 2.5f,
                                              Tuning::kSanctuaryCentreZ + std::cos(a) * 2.5f },
                                            30, glm::vec3(0.35f, 0.9f, 1.0f), 3.0f);
                        }
                    }
                }

                pursuer.setActive(false);
                gaze.reset();

                // Stop spawning, but let what is already falling land.
                debris.setActive(false);
                endingPulse = 0.0f;

                if (to == TrialState::Escaped)
                {
                    // The corridor comes down behind him. Scenery only - it
                    // cannot stun someone who is already out.
                    debris.collapseAround(
                        player.position() - glm::vec3(0.0f, 0.0f, 14.0f), 10, 9.0f);

                    if (particlesAvailable)
                    {
                        particles.burst({ 0.0f, 2.2f, Tuning::kExitZ + 1.0f },
                                        140, glm::vec3(1.0f, 0.92f, 0.62f), 5.0f);
                    }
                }
                else if (to == TrialState::Caught && particlesAvailable)
                {
                    particles.burst(player.position() + glm::vec3(0.0f, 1.2f, 0.0f),
                                    70, glm::vec3(0.45f, 0.85f, 0.35f), 2.4f);
                }
            }

            if (to == TrialState::Waiting)
            {
                // A new run: put the traveller and the treasure back.
                player.reset({ 0.0f, 0.0f, 5.5f }, 180.0f);
                scene.treasureTaken = false;
                pursuer.reset(medusaHome, medusaHomeHeading, true);
                debris.reset();
                gaze.reset();
                petrifyHold = 0.0f;

                // Every block back in place, every torch relit.
                collapse.reset();
                clearCollapse();
                puzzleActive = false;

                // The gate closes again behind the garden, and it dies again.
                gardenOpenLevel = 0.0f;
                offerTime = -1.0f;
                gGardenOpen = false;
                player.setGardenOpen(false);
                player.setEndWall(gateFaceZ);
                std::cout << "[Trial] a new traveller enters the chamber" << std::endl;
            }
        }

        // The state machine decides when the treasure rises; Scene::applyTrial
        // has already written scene.treasureReveal from it.
        //
        // The gate opens once the treasure is taken. The player is only let
        // through when the slab is nearly clear, or he walks through stone.
        scene.gateTarget = scene.treasureTaken ? 1.0f : 0.0f;
        player.setGateOpen(scene.gateLevel() > 0.85f);

#ifdef TRIAL_AUTOPLAY
        // Diagnostic build: plays the whole sequence by itself and reports
        // what happens, so the wiring in this file can be checked without a
        // pair of hands on the keyboard.
        {
            static float autoClock = 0.0f;
            static float autoReport = 0.0f;
            autoClock += deltaTime;

            if (autoClock > 1.0f && trial.state() == TrialState::Waiting)
            {
                // The blessed path: only a true heart is ever offered the
                // treasure, so only it reaches the escape and the gaze.
                std::cout << "[auto] pressing 1 (blessed)" << std::endl;
                trial.forceOutcome(true);
            }

            if (trial.state() == TrialState::TreasureRevealed && trial.stateTime() > 3.6f)
            {
                // After watching the Djinn point at it.
                const glm::vec3 d = scene.treasureWorldPosition() - player.position();
                walkDirection = glm::vec3(d.x, 0.0f, d.z);
            }
            else if (trial.state() == TrialState::Escape)
            {
                walkDirection = glm::vec3(0.0f, 0.0f, 1.0f);

#ifdef TRIAL_AUTOPLAY_HIDE
                // The cover-seeking bot from the simulation, ported: commit to
                // one pillar per attack, pass it on the open side, then tuck
                // into the lane it shades - and only while the beam is
                // actually passing, because hiding any longer just hands
                // Medusa the lead.
                static int hideIndex = -1;
                const glm::vec3 me  = player.position();
                const glm::vec3 her = pursuer.position();

                if (gaze.warning() <= 0.0f) { hideIndex = -1; }
                else
                {
                    if (hideIndex < 0)
                    {
                        float bestAhead = 1e9f;
                        for (int k = 0; k < Tuning::kPillarCount; ++k)
                        {
                            const float ahead = Tuning::kPillarZ[k] - me.z;
                            if (ahead > 0.6f && ahead < 16.0f && ahead < bestAhead)
                            {
                                hideIndex = k;
                                bestAhead = ahead;
                            }
                        }
                    }

                    if (hideIndex >= 0)
                    {
                        const float px = Tuning::kPillarX[hideIndex];
                        const float pz = Tuning::kPillarZ[hideIndex];

                        glm::vec2 lane(px - her.x, pz - her.z);
                        const float len = glm::length(lane);
                        if (len > 1e-3f) { lane /= len; }

                        const float tuck = Tuning::kPillarBaseRadius + 1.0f;
                        const glm::vec2 hidden(px + lane.x * tuck, pz + lane.y * tuck);
                        const glm::vec2 onward(px + lane.x * (tuck + 3.0f),
                                               pz + lane.y * (tuck + 3.0f));

                        const float pt = gaze.phaseTime();
                        const bool lockOrSweep =
                            gaze.phase() == Gaze::Phase::Lock ||
                            (gaze.phase() == Gaze::Phase::Sweep && pt < 1.45f);

                        const float toHidden = glm::length(hidden - glm::vec2(me.x, me.z));
                        const float eta = toHidden / Tuning::kPlayerSpeed;
                        const float timeToLock =
                            (gaze.phase() == Gaze::Phase::Telegraph)
                            ? (Tuning::kGazeTelegraph - pt) : 0.0f;

                        const bool pastWindow =
                            gaze.phase() == Gaze::Phase::Sweep && pt >= 1.45f;

                        if (!pastWindow && (lockOrSweep || timeToLock <= eta + 0.25f))
                        {
                            glm::vec2 goal;
                            if (me.z < pz - 0.4f)
                            {
                                const float openSide = (px < 0.0f) ? 1.0f : -1.0f;
                                goal = glm::vec2(px + openSide * 2.1f, pz - 0.2f);
                            }
                            else
                            {
                                goal = (toHidden > 0.5f) ? hidden : onward;
                            }

                            glm::vec2 toGoal = goal - glm::vec2(me.x, me.z);
                            if (glm::length(toGoal) > 0.15f)
                            {
                                toGoal = glm::normalize(toGoal);
                                walkDirection = glm::vec3(toGoal.x, 0.0f, toGoal.y);
                            }
                        }
                    }
                }
#endif

                // A person steers round a fallen block; so does the bot.
                if (glm::length(walkDirection) > 1e-3f)
                {
                    walkDirection = player.world().avoidRubble(
                        player.position(), glm::normalize(walkDirection),
                        Tuning::kPlayerRadius, 6.0f);
                }
            }

            if (trial.state() == TrialState::Sanctuary
                && puzzle.show() != GatePuzzle::Show::Idle && puzzle.patternShows() > 0)
            {
                // Watch the pattern, then press one key every 0.4 s: choose
                // the first wrong ring, turn it the short way round.
                static float autoPress = 0.0f;
                autoPress += deltaTime;
                if (autoPress >= 0.4f && puzzle.show() == GatePuzzle::Show::Holding)
                {
                    autoPress = 0.0f;
                    for (int k = 0; k < GatePuzzle::kRings; ++k)
                    {
                        if (puzzle.ringMatches(k)) { continue; }
                        if (puzzle.selected() != k)
                        {
                            puzzle.select(k > puzzle.selected() ? 1 : -1);
                            printf("[auto] select ring %d\n", puzzle.selected());
                        }
                        else
                        {
                            const int turn = GatePuzzle::shortestTurn(puzzle.step(k), puzzle.target(k));
                            puzzle.rotate(turn > 0 ? 1 : -1);
                            printf("[auto] turn ring %d %s\n", k, turn > 0 ? "right" : "left");
                        }
                        break;
                    }
                }
            }

            if (trial.state() == TrialState::Garden && gGardenOpen)
            {
                // Into the garden, up the stepping stones to the ankh.
                const glm::vec3 d = glm::vec3(0.0f, 0.0f, Tuning::kGardenFrontZ + 5.6f)
                                  - player.position();
                walkDirection = (glm::length(glm::vec3(d.x, 0.0f, d.z)) > 0.4f)
                              ? glm::vec3(d.x, 0.0f, d.z) : glm::vec3(0.0f);
            }

            if (trial.state() == TrialState::Sanctuary)
            {
                const glm::vec3 d = glm::vec3(0.0f, 0.0f, Tuning::kSanctuaryCentreZ + 1.0f)
                                  - player.position();
                walkDirection = (glm::length(glm::vec3(d.x, 0.0f, d.z)) > 0.4f)
                              ? glm::vec3(d.x, 0.0f, d.z) : glm::vec3(0.0f);
            }

            autoReport += deltaTime;
            if (autoReport >= 1.0f)
            {
                autoReport = 0.0f;
                const glm::vec3 pp = player.position();
                const glm::vec3 tp = scene.treasureWorldPosition();
                const glm::vec3 cam = gCamera.position();
                printf("[auto] %-9s cam(%.1f,%.1f,%.1f) dir=%d "
                       "player(%.1f,%.1f) treasure(%.1f,%.1f) "
                       "reveal=%.2f taken=%d gate=%.2f medusa(%.1f,%.1f) "
                       "gap=%.1f stones=%d petrify=%.2f\n",
                       trial.stateName(), cam.x, cam.y, cam.z,
                       gDirector.enabled() ? 1 : 0, pp.x, pp.z, tp.x, tp.z,
                       scene.treasureReveal, scene.treasureTaken ? 1 : 0,
                       scene.gateLevel(),
                       pursuer.position().x, pursuer.position().z,
                       pursuer.distanceTo(pp), debris.activeCount(),
                       scene.petrifyLevel());
                printf("[auto]   gaze %-9s exposure=%.2f visible=%.2f "
                       "beamLen=%.1f eye=%.2f side=%+d slowed=%d minFade=%.2f\n",
                       gaze.phaseName(), gaze.exposure(), gaze.visibility(),
                       gaze.beamLength(), gaze.eyeLevel(), gaze.sweepSide(),
                       player.speedFraction() < 0.9f ? 1 : 0,
                       scene.minPillarFade());
                printf("[auto]   collapse");
                for (int i = 0; i < collapse.count(); ++i)
                {
                    printf(" %s", Collapse::stageName(collapse.stage(i)));
                }
                printf(" | torches");
                for (float power : scene.torchPower) { printf(" %.2f", power); }
                printf("\n");
                fflush(stdout);
            }
        }
#endif

        // Always updated, even with no input, so he decelerates rather than
        // stopping dead the instant control is taken away.
        player.update(walkDirection, deltaTime);

        // Once it is up, the dome keeps him in as well as her out.
        if (trial.state() == TrialState::Sanctuary && sanctuaryLevel >= 1.0f)
        {
            player.keepWithin({ 0.0f, 0.0f, Tuning::kSanctuaryCentreZ },
                              Tuning::kSanctuaryRadius - Tuning::kPlayerRadius - 0.1f);
        }

        if (scene.traveller != nullptr)
        {
            player.applyTo(*scene.traveller);
        }

        // Feed his real speed to the walk cycle, so the legs move at the rate
        // he is actually travelling rather than the rate he is asking for.
        // Stone does not stride. Fading the walk cycle out with the
        // petrification freezes him mid-pose instead of leaving his legs
        // swinging inside a statue.
        scene.travellerSpeed01 =
            player.speedFraction() * (1.0f - scene.petrifyLevel());

        // --- picking up the treasure ------------------------------------------
        // A flat XZ distance test: the whole game happens on one floor, so
        // height would only ever be noise here.
        if (scene.treasure != nullptr && !scene.treasureTaken
            && scene.treasureReveal > 0.6f)
        {
            const glm::vec3 jewel = scene.treasureWorldPosition();
            const float dx = player.position().x - jewel.x;
            const float dz = player.position().z - jewel.z;

            // Squared comparison - the actual distance is never displayed, so
            // there is no reason to pay for the square root.
            if (dx * dx + dz * dz < Tuning::kPickupRadius * Tuning::kPickupRadius)
            {
                scene.treasureTaken = true;

                if (particlesAvailable)
                {
                    particles.burst(jewel, 90, glm::vec3(1.0f, 0.80f, 0.32f), 4.0f);
                }

                // TreasureRevealed -> Escape. The state machine ignores this
                // from any other state, so it cannot be triggered early.
                trial.treasureCollected();
            }
        }

        // --- the trial -------------------------------------------------------
        if (pressed(window, GLFW_KEY_I, inspectHeld))
        {
            inspectMode = !inspectMode;
            std::cout << "[Mode] " << (inspectMode ? "manual inspection"
                                                   : "trial drives the scene")
                      << std::endl;
        }

        if (!inspectMode)
        {
            if (pressed(window, GLFW_KEY_SPACE, beginHeld))
            {
                // At the gate, Space asks the fireflies to show the pattern again.
                if (trial.state() == TrialState::Sanctuary)
                {
                    if (puzzle.requestPattern())
                    {
                        std::cout << "[Gate] the fireflies show the pattern again" << std::endl;
                    }
                }
                else
                {
                    trial.begin();
                }
            }
            if (pressed(window, GLFW_KEY_R,     trialResetHeld)) { trial.reset(); }
            if (pressed(window, GLFW_KEY_1,     forceGoodHeld))  { trial.forceOutcome(true); }
            if (pressed(window, GLFW_KEY_2,     forceBadHeld))   { trial.forceOutcome(false); }

            // Held, not tapped, so the weight sweeps smoothly.
            // On '-' and '=' rather than the arrow keys: the arrows now walk
            // the traveller. Held, not tapped, so the weight sweeps smoothly.
            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
            {
                trial.adjustWeight(0.35f * deltaTime);
            }
            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
            {
                trial.adjustWeight(-0.35f * deltaTime);
            }

            trial.update(deltaTime);
            scene.applyTrial(trial);
        }
        else
        {
            // Manual toggles are step inputs, so let update() ease them.
            scene.directDrive = false;

            // --- Phase 3 inspection keys -------------------------------------
            if (pressed(window, GLFW_KEY_O, lidHeld))
            {
                scene.lidTarget = (scene.lidTarget > 0.5f) ? 0.0f : 1.0f;
            }
            if (pressed(window, GLFW_KEY_M, headHeld))
            {
                scene.headTarget = (scene.headTarget > 0.5f) ? 0.0f : 1.0f;
            }
            if (pressed(window, GLFW_KEY_K, columnHeld))
            {
                scene.columnTarget = (scene.columnTarget > 0.5f) ? 0.0f : 1.0f;
            }
            if (pressed(window, GLFW_KEY_P, petrifyHeld))
            {
                scene.petrifyTarget = (scene.petrifyTarget > 0.5f) ? 0.0f : 1.0f;
            }
            if (pressed(window, GLFW_KEY_X, resetHeld))
            {
                scene.manualBeam    = false;
                scene.beamTarget    = 0.0f;
                scene.lidTarget     = 0.0f;
                scene.headTarget    = 0.0f;
                scene.columnTarget  = 0.0f;
                scene.petrifyTarget = 0.0f;
            }

            // Hold [ or ] to tilt the beam by hand and watch the pans stay level.
            if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
            {
                const float dir =
                    (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) ? 1.0f : -1.0f;

                scene.manualBeam = true;
                scene.beamTarget = glm::clamp(scene.beamTarget + dir * 25.0f * deltaTime,
                                              -20.0f, 20.0f);
            }
        }

        // Hand the camera back to the director once the drag is over. Only
        // in Cinematic view - the other two modes are a deliberate choice by
        // the user and should not be undone.
        if (gViewMode == ViewMode::Cinematic && !gDirector.enabled())
        {
            gDirectorResume -= deltaTime;
            if (gDirectorResume <= 0.0f)
            {
                gDirector.setEnabled(true);
            }
        }

        // The director only acts in orbit mode, and switches itself off the
        // moment the user drags the view. The chase shot rides behind the
        // traveller, so it has to be told where he is.
        gDirector.setFollowTarget(player.position());
        gDirector.setFollowSpeed(player.speedFraction());
        gDirector.setDanger(chaseDanger);
        gDirector.setStateTime(trial.stateTime());

        // Look up at the chamber breaking - but only while he is still in it.
        gDirector.setLookUp((trial.state() == TrialState::Escape
                             && player.position().z < Tuning::kGateZ - 0.5f)
                            ? collapse.tremor() : 0.0f);
        gDirector.setFollowHeading(player.heading());
        gDirector.update(gCamera, trial.state(), deltaTime, now);

        if (gViewMode == ViewMode::FirstPerson)
        {
            // Sit the camera in his head, a little forward of centre so the
            // body is behind the near plane.
            const float yawRadians = glm::radians(gCamera.yaw());
            const glm::vec3 facing(std::sin(yawRadians), 0.0f, std::cos(yawRadians));

            gCamera.setPosition(player.position()
                              + glm::vec3(0.0f, 2.35f, 0.0f)
                              + facing * 0.30f);
        }
        else
        {
            keepCameraOutOfWalls(gCamera, trial.state());
        }

        // Camera shake, applied last so nothing overwrites it. Two sines at
        // unrelated frequencies per axis, so it reads as a rumble rather
        // than a wobble.
        if (cameraShake > 0.001f)
        {
            const float amount = cameraShake * 0.35f;
            const glm::vec3 jolt(
                amount * (std::sin(now * 47.0f) + 0.6f * std::sin(now * 83.0f)),
                amount * (std::sin(now * 61.0f) + 0.6f * std::sin(now * 97.0f)),
                amount * (std::sin(now * 53.0f) + 0.6f * std::sin(now * 71.0f)));

            if (gViewMode == ViewMode::FirstPerson)
            {
                gCamera.setPosition(gCamera.position() + jolt);
            }
            else
            {
                gCamera.setTarget(gCamera.target() + jolt);
            }
        }

        // His own head would fill the view from the inside.
        if (scene.travellerHead != nullptr)
        {
            scene.travellerHead->visible = (gViewMode != ViewMode::FirstPerson);
        }

        // A pillar between the camera and him ghosts out, so he can be seen
        // while he hides behind it.
        scene.fadeCoverBetween(gCamera.position(),
                               player.position() + glm::vec3(0.0f, 1.5f, 0.0f),
                               deltaTime);

        scene.applyCollapse(collapse, now);

        scene.sanctuaryLevel = sanctuaryLevel;
        for (int k = 0; k < GatePuzzle::kRings; ++k)
        {
            scene.gateRingAngle[k] = puzzleActive ? puzzle.ringAngle(k) : 0.0f;
        }
        scene.gateSelected = (trial.state() == TrialState::Sanctuary) ? puzzle.selected() : -1;
        scene.gateLock     = puzzleActive ? puzzle.lockLevel() : 0.0f;
        scene.gardenOpen   = gardenOpenLevel;

        // The jewel's flight, then the circle of life spreading from the altar.
        if (offerTime >= 0.0f)
        {
            const float before = offerTime - Tuning::kOfferFlight;
            offerTime += deltaTime;
            const float waveT = offerTime - Tuning::kOfferFlight;

            scene.offeringFlight = Easing::clamp01(offerTime / Tuning::kOfferFlight);
            scene.offeringFrom   = offerFrom;

            if (waveT >= 0.0f)
            {
                if (before < 0.0f)
                {
                    std::cout << "[Garden] life returns to the garden" << std::endl;
                    cameraShake = std::max(cameraShake, 0.3f);
                    if (particlesAvailable)
                    {
                        particles.burst(scene.offeringPosition(), 120,
                                        glm::vec3(1.0f, 0.85f, 0.45f), 4.0f);
                    }
                }

                // Fast at first, slowing as it reaches the walls.
                const float u = Easing::clamp01(waveT / Tuning::kWaveTime);
                scene.gardenWaveRadius = 0.05f + Tuning::kWaveRadius * (1.0f - (1.0f - u) * (1.0f - u));

                // Gold sparks thrown up all along the edge as it travels.
                if (particlesAvailable && u < 1.0f)
                {
                    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
                    for (int k = 0; k < 6; ++k)
                    {
                        const float a = angle(sparkleRng);
                        const glm::vec3 p = scene.gardenWaveCentre
                            + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * scene.gardenWaveRadius;
                        if (std::fabs(p.x) < Tuning::kGardenHalfWidth - 0.4f
                            && p.z > Tuning::kGardenFrontZ + 0.5f && p.z < Tuning::kGardenBackZ - 0.4f)
                        {
                            particles.burst(p + glm::vec3(0.0f, 0.1f, 0.0f), 2,
                                            glm::vec3(1.0f, 0.82f, 0.42f), 1.4f);
                        }
                    }
                }
            }

            // The ending has played out: a new traveller.
            if (waveT > Tuning::kWaveTime + Tuning::kGardenLinger
                && trial.state() == TrialState::Garden)
            {
                trial.reset();
            }
        }
        else
        {
            scene.offeringFlight   = -1.0f;
            scene.gardenWaveRadius = -1.0f;
        }
        scene.gardenWaveCentre = scene.gardenAltar;
        gDirector.setOfferTime(offerTime);

        // The gate shot starts 4.5 s in (see CameraDirector); from then he
        // is a ghost, in cinematic view only.
        scene.travellerGhost = (trial.state() == TrialState::Sanctuary
                                && trial.stateTime() > 4.3f
                                && gViewMode == ViewMode::Cinematic) ? 1.0f : 0.0f;
        scene.sanctuaryFlash = sanctuaryFlash;
        scene.sanctuaryWarning = (trial.state() == TrialState::Sanctuary)
            ? Easing::clamp01((trial.stateTime()
                               - (Tuning::kSanctuaryDuration - Tuning::kSanctuaryWarning))
                              / Tuning::kSanctuaryWarning)
            : 0.0f;

        // --- the Djinn's charm arrives ------------------------------------------
        {
            static bool hadCharm = false;
            const bool hasCharm = trial.hasCharm();
            if (hasCharm && !hadCharm)
            {
                std::cout << "[Charm] the Djinn grants a protective charm" << std::endl;
                if (particlesAvailable)
                {
                    particles.burst(scene.charmWorldPosition(), 40,
                                    glm::vec3(0.35f, 0.9f, 1.0f), 2.2f);
                }
            }
            hadCharm = hasCharm;
        }

        // --- hand the gaze to the scene ------------------------------------------
        {
            const bool escaping = (trial.state() == TrialState::Escape);

            scene.gazeDriven = escaping;
            if (escaping)
            {
                scene.gazeAim        = gaze.aimPoint();
                scene.gazeEye        = gaze.eyeLevel();
                scene.gazeBeamWidth  = gaze.beamWidth();
                scene.gazeBeamLength = gaze.beamLength();
            }

            // Exposure IS the traveller turning to stone: the number that fills
            // the bar also drives the shader, the walk cycle and the colour
            // draining from the screen. applyTrial rewrites petrifyTarget every
            // frame, so this has to come after it.
            scene.petrifyTarget = std::max(scene.petrifyTarget, petrifyHold);
        }

        scene.update(now, deltaTime);

        // Emission follows the energy column, so the plume grows and dies
        // with the blessing rather than being switched on and off.
        if (particlesAvailable)
        {
#ifdef TRIAL_DEBUG_PARTICLES
            // Force full emission so the pool can be checked for a steady
            // state: spawn rate 220/s x mean life 2.25s = about 495 alive.
            particles.setEmitter(scene.djinnEmitterPosition(), 1.0f);
#else
            particles.setEmitter(scene.djinnEmitterPosition(),
                                 useParticles ? scene.columnLevel()
                                                * (1.0f - 0.75f * scene.djinnPresence)
                                              : 0.0f);
#endif
            particles.update(deltaTime, now);

            // --- the Djinn ------------------------------------------------------
            // Out of the lamp as it opens; he points at the treasure as it
            // rises, then pours back into the lamp.
            {
                const TrialState st = trial.state();
                const float t = trial.stateTime();
                const bool present = (st == TrialState::Balanced && t >= 1.2f)
                                  || (st == TrialState::TreasureRevealed && t < 3.4f);

                if (st == TrialState::Waiting) { djinn.hideNow(); }
                djinn.setBase(scene.djinnEmitterPosition());
                if (present != djinn.shown())
                {
                    djinn.show(present);
                    std::cout << (present ? "[Djinn] rises from the lamp in a column of smoke"
                                          : "[Djinn] pours back into the lamp") << std::endl;
                }

                static bool pointed = false;
                const float point = (st == TrialState::TreasureRevealed)
                                  ? Easing::smoothstep01((t - 0.1f) / 0.6f) : 0.0f;
                if (point > 0.0f && !pointed) { std::cout << "[Djinn] points to the treasure" << std::endl; }
                pointed = point > 0.0f;

                djinn.faceTowards(player.position(), deltaTime);
                djinn.setPointing(scene.treasureWorldPosition() + glm::vec3(0.0f, 0.6f, 0.0f), point);
                djinn.update(deltaTime, now);

                if (djinn.visible())
                {
                    for (int i = 0; i < djinn.count(); ++i)
                    {
                        particles.addGlow(djinn.position(i), djinn.size(i), djinn.color(i));
                    }
                    // A warm halo round each eye.
                    for (int e = 0; e < 2; ++e)
                    {
                        particles.addGlow(djinn.eyePosition(e), 0.6f,
                                          { 1.0f, 0.82f, 0.4f, 0.95f * djinn.presence() });
                    }
                }

                scene.djinnHand     = djinn.handPosition();
                scene.djinnChest    = djinn.chestPosition();
                scene.djinnPresence = djinn.presence();
            }

            // --- fireflies ------------------------------------------------------
            // They scatter from the traveller, and panic when the chamber
            // starts to come down.
            fireflies.update(deltaTime, player.position(), collapse.tremor());
            for (int i = 0; i < fireflies.count(); ++i)
            {
                const float glow = fireflies.glow(i);
                const glm::vec3 at = fireflies.position(i);

                // A tight bright core and a wide soft halo round it.
                // Sized to read from the overview camera fifteen units away,
                // not just up close.
                particles.addGlow(at, 0.14f + 0.08f * glow,
                                  { 1.0f, 1.0f, 0.6f, 0.25f + 0.75f * glow });
                particles.addGlow(at, 1.0f,
                                  { 0.55f, 0.95f, 0.20f, 0.65f * glow });
            }

            if (puzzleActive)
            {
                for (int i = 0; i < GatePuzzle::kFlies; ++i)
                {
                    const float glow = puzzle.flyGlow(i);
                    const glm::vec3 at = puzzle.flyPosition(i);
                    // Bigger than the chamber's: they are read against a
                    // brightly lit gate from eight units away.
                    const float star = (i < 9) ? 1.35f : 1.0f;
                    // They drift off into the garden as the gate goes down.
                    const float fade = 1.0f - gardenOpenLevel;
                    particles.addGlow(at, (0.16f + 0.10f * glow) * star,
                                      { 1.0f, 1.0f, 0.75f, (0.35f + 0.65f * glow) * fade });
                    particles.addGlow(at, 1.1f * star,
                                      { 0.6f, 1.0f, 0.3f, 0.65f * glow * fade });
                }
            }

            // --- the souls, rising --------------------------------------------
            {
                int freed = -1;
                while (scene.popFreedStatue(freed))
                {
                    std::cout << "[Garden] a statue wakes - a soul is freed" << std::endl;
                }

                // Which statues have already burst into gold dust this run.
                static bool burstDone[16] = {};
                if (scene.gardenWaveRadius <= 0.0f)
                {
                    for (bool& done : burstDone) { done = false; }
                }

                static float trail = 0.0f;
                trail += deltaTime;
                const bool dropTrail = trail >= 0.08f;
                if (dropTrail) { trail = 0.0f; }

                for (int i = 0; i < scene.soulCount(); ++i)
                {
                    if (!scene.soulVisible(i)) { continue; }
                    const float glow = scene.soulGlow(i);
                    const glm::vec3 at = scene.soulPosition(i);

                    // The moment the stone gives way: a cloud of gold dust.
                    if (i < 16 && !burstDone[i])
                    {
                        burstDone[i] = true;
                        const glm::vec3 chest = scene.statueChest(i);
                        particles.burst(chest, 70, glm::vec3(1.0f, 0.82f, 0.42f), 2.6f);
                        particles.burst(chest - glm::vec3(0.0f, 1.2f, 0.0f), 40,
                                        glm::vec3(0.85f, 0.75f, 0.5f), 1.6f);
                    }

                    // A firefly - but a big one.
                    // Sized for the sky shot, a dozen units away.
                    particles.addGlow(at, 0.45f + 0.15f * glow, { 1.0f, 1.0f, 0.75f, glow });
                    particles.addGlow(at, 2.6f, { 0.65f, 1.0f, 0.35f, 0.65f * glow });
                    if (dropTrail)
                    {
                        particles.burst(at, 2, glm::vec3(1.0f, 0.9f, 0.5f), 0.5f);
                    }
                }
            }

            // The garden's fireflies, gathered round the relics.
            if (gardenOpenLevel > 0.01f)
            {
                for (int s = 0; s < 3; ++s)
                {
                    Fireflies& swarm = gardenFlies[s];
                    swarm.update(deltaTime, player.position(), 0.0f);

                    // Barely there while the garden is dead.
                    const float alive = 0.2f + 0.8f * scene.gardenLifeAt(scene.gardenRelics[s]);
                    for (int i = 0; i < swarm.count(); ++i)
                    {
                        const float glow = swarm.glow(i) * gardenOpenLevel * alive;
                        const glm::vec3 at = swarm.position(i);
                        particles.addGlow(at, 0.12f + 0.07f * glow,
                                          { 1.0f, 1.0f, 0.6f, 0.2f + 0.8f * glow });
                        particles.addGlow(at, 0.8f, { 0.55f, 0.95f, 0.2f, 0.5f * glow });
                    }
                }
            }

            // In the garden the light that wanders is a firefly by the ankh.
            const Fireflies& lighting = (gardenOpenLevel > 0.5f) ? gardenFlies[0] : fireflies;
            const int lit = lighting.lit();
            scene.fireflyLightLevel    = lighting.lightLevel();
            scene.fireflyLightPosition = (lit >= 0) ? lighting.position(lit) : glm::vec3(0.0f);

#ifdef TRIAL_DEBUG_PARTICLES
            {
                static float report = 0.0f;
                report += deltaTime;
                if (report >= 1.0f)
                {
                    report = 0.0f;
                    printf("[particles] alive = %d / 700\n", particles.aliveCount());
                    fflush(stdout);
                }
            }
#endif
        }

        const bool shadowsOn = shadowsAvailable && useShadows;

        // --- pass 1: depth from the light ------------------------------------
        if (shadowsOn)
        {
            shadowMap.setLight(scene.shadowLightPosition(),
                               Scene::shadowLightTarget());

            shadowMap.beginCapture();
            depthShader.use();
            depthShader.setMat4("uLightSpaceMatrix", shadowMap.lightSpaceMatrix());

            // Render back faces into the depth map. The recorded surface is
            // then the far side of each object, which pushes the comparison
            // depth away from the receiver and removes most self-shadowing
            // acne without needing a large bias.
            //
            // Culling is forced on for this pass rather than inherited: with
            // the B key having turned it off, both faces would be written and
            // the acne would come straight back.
            GLboolean wasCulling = GL_FALSE;
            glGetBooleanv(GL_CULL_FACE, &wasCulling);

            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            scene.drawDepth(depthShader);

            glCullFace(GL_BACK);
            if (wasCulling != GL_TRUE)
            {
                glDisable(GL_CULL_FACE);
            }

            shadowMap.endCapture(gWindowWidth, gWindowHeight);
        }

        // --- pass 2: the scene ------------------------------------------------
        const bool postOn = postAvailable && usePost;

        if (postOn)
        {
            // Follow the window, or the off-screen target would be stretched.
            if (post.width() != gWindowWidth || post.height() != gWindowHeight)
            {
                post.resize(gWindowWidth, gWindowHeight);
            }

            const float petrify = scene.petrifyLevel();
            const float column  = scene.columnLevel();
            const float escaped = scene.exitGlow;

            if (escaped > 0.01f)
            {
                // Out alive: the vignette opens up and the picture warms,
                // the exact opposite of the curse.
                post.setEffects(0.0f,
                                0.20f - 0.14f * escaped,
                                glm::vec3(1.06f, 0.98f, 0.82f),
                                0.30f * escaped);
            }
            else
            {
                post.setEffects(
                    // Colour drains out of the whole chamber as the curse
                    // takes hold, not just off the traveller.
                    0.80f * petrify,
                    // A little vignette always, closing in hard during the
                    // curse.
                    0.20f + 0.50f * petrify,
                    // Cold stone-green for the curse, cyan for the blessing.
                    petrify > column ? glm::vec3(0.78f, 0.88f, 0.76f)
                                     : glm::vec3(0.72f, 0.94f, 1.05f),
                    0.35f * std::max(petrify, column * 0.45f));
            }

            post.beginScene();
        }

        glClearColor(0.06f, 0.03f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float aspect = static_cast<float>(gWindowWidth) /
                             static_cast<float>(gWindowHeight > 0 ? gWindowHeight : 1);

        shader.use();
        shader.setMat4("uView", gCamera.view());
        shader.setMat4("uProjection", gCamera.projection(aspect));
        shader.setVec3("uViewPos", gCamera.position());
        shader.setVec3("uAmbient", scene.ambient);
        shader.setInt("uDebugNormals", debugNormals ? 1 : 0);
        shader.setInt("uUseTextures", useTextures ? 1 : 0);
        shader.setInt("uUseNormalMaps", useNormalMaps ? 1 : 0);

        // Texture units: 0 diffuse, 1 specular (both set per material), 2 the
        // shadow map. Bound once here; nothing else touches unit 2, so it
        // stays valid across every material's own binds.
        shader.setInt("uShadowsEnabled", shadowsOn ? 1 : 0);
        shader.setInt("uShadowLightIndex", Scene::kShadowLightIndex);
        shader.setInt("uShadowMap", 2);
        shader.setMat4("uLightSpaceMatrix", shadowMap.lightSpaceMatrix());
        if (shadowsOn)
        {
            shadowMap.bindTexture(2);
        }

        uploadLights(shader, scene.lights);

        scene.draw(shader, gCamera.position());

        debris.draw(shader);
        debris.drawWarnings(shader);

        if (showLightMarkers)
        {
            scene.drawLightMarkers(shader);
        }

        // After all scene geometry so the motes blend over it, but before the
        // HUD, which must sit on top of everything.
        if (particlesAvailable && useParticles)
        {
            particles.render(gCamera.view(), gCamera.projection(aspect));
        }

        // Resolve before the HUD: the overlay is an instrument, not part of
        // the scene, and should stay legible while the image desaturates.
        if (postOn)
        {
            post.endSceneAndResolve();
        }

        if (showHud)
        {
            Hud::Info info;
            info.state        = trial.state();
            info.progress     = trial.progress();
            info.heartWeight  = trial.heartWeight();
            info.wouldBalance = trial.wouldBalance();
            info.time         = now;
            info.escapeActive = (trial.state() == TrialState::Escape);
            info.charm        = trial.charm();
            info.sanctuaryActive = (trial.state() == TrialState::Sanctuary);
            info.sanctuaryLeft   = 1.0f - Easing::clamp01(trial.stateTime()
                                                         / Tuning::kSanctuaryDuration);

            if (info.escapeActive)
            {
                // Measured from where the treasure stands to the way out, so
                // the bar starts empty the moment the chase does.
                const float from = 0.8f;
                info.exitProgress = glm::clamp(
                    (player.position().z - from) / (Tuning::kSanctuaryTriggerZ - from),
                    0.0f, 1.0f);

                info.danger      = chaseDanger;
                info.exposure    = gaze.exposure();
                info.gazeWarning = gaze.warning();
                info.gazeLive    = gaze.beamLive();
            }

            hud.draw(info, gWindowWidth, gWindowHeight);
        }

#ifdef TRIAL_AUTOPLAY
        // Set TRIAL_SHOT_DIR to have the self-playing build save frames at
        // fixed moments of the first run.
        if (const char* shotDir = std::getenv("TRIAL_SHOT_DIR"))
        {
            struct Moment { TrialState state; float at; const char* name; };
            static const Moment moments[] = {
                { TrialState::Placing,   0.5f, "a_chamber" },
                { TrialState::Balanced,  1.9f, "a_djinn1_rise" },
                { TrialState::Balanced,  2.6f, "a_djinn2_rise" },
                { TrialState::Balanced,  3.6f, "a_djinn3_formed" },
                { TrialState::Balanced,  4.3f, "a_djinn4_charm" },
                { TrialState::TreasureRevealed, 1.6f, "a_djinn5_point" },
                { TrialState::TreasureRevealed, 3.0f, "a_djinn6_point" },
                { TrialState::TreasureRevealed, 4.1f, "a_djinn7_gone" },
                { TrialState::Sanctuary, 7.0f, "b_puzzle" },
                { TrialState::Garden,    0.3f, "c_open0" },
                { TrialState::Garden,    1.3f, "c_open1" },
                { TrialState::Garden,    2.1f, "c_open2" },
                { TrialState::Garden,    2.9f, "c_open3" },
                { TrialState::Garden,    3.5f, "c_open4" },
                { TrialState::Garden,    6.0f, "d_walk0" },
                { TrialState::Garden,    9.0f, "d_walk1" },
                { TrialState::Garden,   14.0f, "d_walk2" },
                { TrialState::Garden,   23.0f, "e_wide" },
                { TrialState::Garden,    7.5f, "f_wave0" },
                { TrialState::Garden,    9.0f, "f_wave1" },
                { TrialState::Garden,   10.5f, "f_wave2" },
                { TrialState::Garden,   12.0f, "f_wave3" },
                { TrialState::Garden,   14.0f, "f_wave4" },
                { TrialState::Garden,   17.5f, "f_wave5" },
                { TrialState::Garden,   20.0f, "f_wave6" },
            };
            static bool taken[sizeof(moments) / sizeof(moments[0])] = {};
            for (std::size_t i = 0; i < sizeof(moments) / sizeof(moments[0]); ++i)
            {
                if (!taken[i] && trial.state() == moments[i].state
                    && trial.stateTime() >= moments[i].at)
                {
                    taken[i] = true;
                    saveFrameBmp(std::string(shotDir) + "/" + moments[i].name + ".bmp",
                                 gWindowWidth, gWindowHeight);
                    printf("[shot] %s\n", moments[i].name);
                }
            }
        }
#endif

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    hud.shutdown();
    shadowMap.shutdown();
    particles.shutdown();
    debris.shutdown();
    post.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
