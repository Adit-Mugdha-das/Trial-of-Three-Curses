#include <algorithm>
#include <cstdio>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Camera.h"
#include "CameraDirector.h"
#include "Debris.h"
#include "Hud.h"
#include "EscapeTuning.h"
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
    bool cameraPositionIsInside(const glm::vec3& p, const glm::vec3& subject,
                                bool allowThroughDoorway)
    {
        if (p.y < 0.7f) { return false; }              // through the floor

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
            // fine; only the walls and floor constrain it.
            return std::fabs(p.x) <= 12.2f && p.z >= -10.2f;
        }

        // The corridor is enclosed, so the ceiling matters here.
        return std::fabs(p.x) <= 3.9f && p.y <= 6.8f && p.z <= 73.0f;
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

    // Where she rests between runs, captured before anything moves her.
    const glm::vec3 medusaHome =
        (scene.medusaRoot != nullptr) ? scene.medusaRoot->position
                                      : glm::vec3(6.0f, 0.0f, -1.5f);
    const float medusaHomeHeading =
        (scene.medusaRoot != nullptr) ? scene.medusaRoot->rotation.y : -35.0f;

    Pursuer pursuer;
    pursuer.setWorld(world);
    pursuer.reset(medusaHome, medusaHomeHeading, true);

    Debris debris;
    debris.init(Tuning::kMaxStones);
    debris.setCorridor(Tuning::kGateZ, Tuning::kCorridorHalfWidth,
                       Tuning::kExitZ, 6.8f);
    debris.setMaterial(scene.rubbleMaterial);

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
        float inputForward = 0.0f;
        float inputStrafe  = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    { inputForward += 1.0f; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  { inputForward -= 1.0f; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { inputStrafe  += 1.0f; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  { inputStrafe  -= 1.0f; }

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
                                55, glm::vec3(0.62f, 0.55f, 0.45f), 2.6f);
            }

            cameraShake = std::max(cameraShake, debris.shake());
        }

        cameraShake = std::max(0.0f, cameraShake - deltaTime * 2.2f);

        // --- the chase ---------------------------------------------------------
        pursuer.setGateOpen(scene.gateLevel() > 0.85f);
        pursuer.update(player.position(), deltaTime);

        if (scene.medusaRoot != nullptr)
        {
            pursuer.applyTo(*scene.medusaRoot);
        }

        if (trial.state() == TrialState::Escape && pursuer.hasCaught(player.position()))
        {
            trial.caught();
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

                debris.reset();
                debris.setIntensity(1.0f);
                debris.setActive(true);

                std::cout << "[Medusa] gives chase" << std::endl;
            }
            else if (from == TrialState::Escape)
            {
                pursuer.setActive(false);

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
                std::cout << "[auto] pressing 2 (cursed)" << std::endl;
                trial.forceOutcome(false);
            }

            if (trial.state() == TrialState::TreasureRevealed)
            {
                const glm::vec3 d = scene.treasureWorldPosition() - player.position();
                walkDirection = glm::vec3(d.x, 0.0f, d.z);
            }
            else if (trial.state() == TrialState::Escape)
            {
                walkDirection = glm::vec3(0.0f, 0.0f, 1.0f);
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
                fflush(stdout);
            }
        }
#endif

        // Always updated, even with no input, so he decelerates rather than
        // stopping dead the instant control is taken away.
        player.update(walkDirection, deltaTime);

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
            if (pressed(window, GLFW_KEY_SPACE, beginHeld))      { trial.begin(); }
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
                                 useParticles ? scene.columnLevel() : 0.0f);
#endif
            particles.update(deltaTime, now);

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

            if (info.escapeActive)
            {
                // Measured from where the treasure stands to the way out, so
                // the bar starts empty the moment the chase does.
                const float from = 0.8f;
                info.exitProgress = glm::clamp(
                    (player.position().z - from) / (Tuning::kExitZ - from),
                    0.0f, 1.0f);

                info.danger = chaseDanger;
            }

            hud.draw(info, gWindowWidth, gWindowHeight);
        }

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
