# The Trial of Three Curses — Project Plan

An OpenGL 3.3 (core profile) real-time animated scene: a traveller enters a mystical chamber,
places a glowing heart on **Anubis's soul-balance scale**, and the verdict either opens a
**Djinn lamp** (blessing) or wakes the **Medusa guardian** (petrification curse).

Everything is procedurally animated from elapsed time inside the render loop — nothing baked.

> **Update:** this scene is now the opening act of an interactive game. See
> [ESCAPE_PLAN.md](ESCAPE_PLAN.md) for the player-controlled escape sequence. Everything
> documented below stays as it is.

---

## 1. Current status

| Item | State |
|---|---|
| `src/main.cpp` | Window, GLAD, depth test, delta time, FPS title, F5 shader reload ✅ |
| `include/glad`, `src/glad.c` | GLAD for GL 3.3 core ✅ |
| GLFW | installed at `C:/msys64/ucrt64` ✅ |
| GLM (math) | installed at `C:/msys64/ucrt64/include/glm` ✅ |
| `build.ps1` | Builds and runs ✅ (Phase 0 done) |
| `Shader` class + `shaders/phong.*` | Compile, link, uniform cache, hot reload ✅ (Phase 1 done) |
| `Mesh` + `Primitives` + `Camera` | 7 generators, orbit/free-fly camera ✅ (Phase 2 done) |
| `SceneNode` + `Scene` | Full chamber hierarchy, culling on ✅ (Phase 3 done) |
| `Light` + `Material` + Blinn–Phong | Full light rig, §5 material table ✅ (Phase 4 done) |
| `Texture` + procedural maps | 6 diffuse + 3 specular, all tileable ✅ (Phase 5 done) |
| `stb_image.h` | v2.30 in `include/stb/` ✅ |
| `TrialController` + `Easing` | Full state machine, verified headlessly ✅ (Phase 6 done) |
| Scale animation | Heart arc, damped beam settle ✅ (Phase 7 done) |
| Blessing branch | Hinged lid, energy column, sorted transparency ✅ (Phase 8 done) |
| Curse branch | `atan2` aim, roused snakes, per-fragment petrification ✅ (Phase 9 done) |
| Polish | Cinematic camera, HUD ✅ (Phase 10 done) |
| Assimp (model loading) | **missing** — optional, Phase 11 |

**The project is complete and every requirement in the brief is met.** Everything from
Phase 11 on is optional extra credit — shadow mapping has the biggest visual payoff.

### Confirmed GPU

```
OpenGL version: 3.3.0 - Build 20.19.15.4568
Renderer:       Intel(R) HD Graphics 520
```

Integrated Intel graphics reporting **exactly 3.3** — so targeting GL 3.3 is not a
conservative choice here, it is the ceiling. Two consequences:

- Nothing above 3.3 is available. No compute shaders, no SSBOs, no `glTextureStorage`.
  Every technique in this plan stays within 3.3 core, so this is fine.
- Fill rate is limited. Keep the window at 1280×720, shadow maps at 1024×1024, and particle
  counts in the low thousands. Watch the frame-time readout in the title bar as you add
  each stretch goal, and check it in **wireframe (`T`)** if a phase suddenly gets slow —
  that usually means overdraw, not vertex count.

---

## 2. Toolchain and dependencies

- **Compiler:** `C:\msys64\ucrt64\bin\g++.exe` (GCC 16.2, `x86_64-w64-mingw32`), C++17

> ⚠️ **Do not use the bare `g++` on your PATH.** It resolves to `C:\MinGW\bin\g++.exe`
> — MinGW.org GCC 6.3.0, target `mingw32` (**32-bit**). Your GLFW and GLM are 64-bit UCRT64
> builds, so that compiler cannot link them. `build.ps1` pins the full UCRT64 path for this
> reason. If you ever compile by hand, use the full path.

- **Window/Input:** GLFW 3
- **GL loader:** GLAD (already vendored)
- **Math:** GLM (header-only)
- **Image loading:** `stb_image.h` (single header — drop into `include/stb/`)
- **Models (optional):** Assimp — `pacman -S mingw-w64-ucrt-x86_64-assimp`

### Building

`make` is not installed on this machine, so the build is a PowerShell script:

```powershell
.\build.ps1           # compile
.\build.ps1 -Run      # compile, then launch
.\build.ps1 -Clean    # delete app.exe
```

It globs every `src/*.cpp` and `src/*.c`, so new source files are picked up automatically —
no edits needed as the project grows. It links `-static-libgcc -static-libstdc++`, so
`app.exe` runs without `C:\msys64\ucrt64\bin` on `PATH`.

**If the build fails with `cannot open output file ... Permission denied`,** a previous
`app.exe` is still running and holding the lock. Close its window, or:
`Stop-Process -Name app -Force`.

Always launch from the project root — shader paths are relative (`shaders/phong.vert`).

If you would rather use `make`, install it first with
`pacman -S mingw-w64-ucrt-x86_64-make`, then write a `Makefile` mirroring the flags in
`build.ps1`.

---

## 3. Target file layout

```
Trial of Three curses/
├── build.ps1
├── PROJECT_PLAN.md
├── include/
│   ├── glad/  KHR/            (existing)
│   └── stb/stb_image.h        (Phase 5)
├── shaders/
│   ├── phong.vert / phong.frag        core lit shader
│   ├── emissive.vert / emissive.frag  glowing heart, energy, light markers
│   ├── depth.vert  / depth.frag       shadow map pass      (stretch)
│   ├── particle.vert / particle.frag  Djinn smoke          (stretch)
│   └── post.vert   / post.frag        screen-space effects (stretch)
├── assets/
│   ├── textures/   (gold, brass, stone, sandstone, snake scales, normal maps)
│   └── models/     (optional .obj imports)
└── src/
    ├── main.cpp            window, loop, wiring
    ├── glad.c
    ├── Shader.h/.cpp       compile/link/uniform helpers
    ├── Camera.h/.cpp       orbit + free-fly camera
    ├── Mesh.h/.cpp         VAO/VBO/EBO wrapper, draw()
    ├── Primitives.h/.cpp   cube, sphere, cylinder, cone, torus, plane, disk
    ├── Texture.h/.cpp      2D texture loading
    ├── Material.h          ka, kd, ks, shininess, texture handles
    ├── Light.h             point / spot / directional structs + uniform upload
    ├── SceneNode.h/.cpp    hierarchical transforms
    ├── Scene.h/.cpp        builds the whole chamber tree
    └── TrialState.h/.cpp   the state machine + all animation curves
```

---

## 4. Architecture

### 4.1 Scene graph (hierarchical transforms)

Every node holds a local TRS transform; `world = parent.world * local`. Children inherit
automatically — this is what satisfies the "hierarchical transformation" requirement.

```
Root
├── Chamber
│   ├── Floor, Walls, Ceiling
│   └── TorchL, TorchR              (each = bracket + flame quad + point light)
├── AnubisScale
│   └── Pillar
│       └── Beam            ← ROTATES about Z (the verdict tilt)
│           ├── ChainL → PanL       ← pans inherit beam rotation
│           │   └── Heart           ← lands here after translating in
│           └── ChainR → PanR
│               └── Counterweight (Feather of Ma'at)
├── DjinnLamp
│   ├── Body
│   ├── Spout, Handle
│   ├── Lid                 ← COMPOSITE: translate to hinge, rotate, translate back
│   └── EnergyColumn        ← rises on success
│       ├── Ring0..Ring4    ← scale + rise, inherit column transform
│       └── DjinnLight      (blue/green point light, attenuated)
├── Medusa
│   ├── Body / Coiled tail
│   └── Head                ← ROTATES to face traveller
│       ├── Snake0..SnakeN  ← each snake = 4–5 chained segments, inherit head rotation
│       └── EyeL/EyeR       ← spotlights aimed down head's forward axis
└── Traveller
    ├── Torso → Head, ArmL, ArmR, LegL, LegR
    └── PetrifyMeter        ← scales 0→1 as stone spreads
```

Snake chains are the strongest hierarchy demo: `Head → Seg0 → Seg1 → Seg2 → Seg3`, each
segment with a small local rotation that oscillates with a phase offset, so the whole snake
whips while still following the head.

### 4.2 State machine

One enum drives the entire animation. `t` = seconds since the current state began.

| State | Duration | What animates | Exit condition |
|---|---|---|---|
| `WAITING` | until keypress | Idle bob of heart, torch flicker, slow snake sway | user presses `SPACE` |
| `PLACING` | 2.0 s | Heart translates along an eased arc onto the left pan; heart pulses (scale) | `t > 2.0` |
| `WEIGHING` | 3.0 s | Beam rotates toward final angle with damped-spring settle; pans counter-hang | `t > 3.0` |
| `BALANCED` | 6.0 s | Lid rotates open about hinge; energy column rises + scales; rings expand and fade; Djinn light brightens | `t > 6.0` → `RESET` |
| `CURSED` | 8.0 s | Medusa head yaws toward traveller, eye spotlights switch on, snakes agitate, traveller's material lerps to stone bottom-up | `t > 8.0` → `RESET` |
| `RESET` | 2.0 s | Everything eases back to rest, heart returns | `t > 2.0` → `WAITING` |

Branch rule: the user picks a heart weight `w ∈ [0, 1]` with `↑`/`↓` before pressing `SPACE`.

```
balanced = |w - 0.5| < 0.12
beamAngle = clamp((w - 0.5) * 2.0, -1, 1) * MAX_TILT   // MAX_TILT ≈ 20°
```

`WEIGHING` → `BALANCED` if `balanced`, else `CURSED`.

Use an easing helper everywhere so motion never looks linear:

```cpp
float smoothstep01(float x);                    // ease in/out
float easeOutBack(float x);                     // lid pop
float damped(float t, float freq, float decay); // beam settle wobble
```

### 4.3 Transformation requirement checklist

| Requirement | Where it lives | Verify by |
|---|---|---|
| **Translation** | Heart glides onto pan; energy column rises from lamp | Watch `PLACING` and `BALANCED` |
| **Rotation** | Beam tilt; lid opening; Medusa head yaw; snake segments | `WEIGHING`, `BALANCED`, `CURSED` |
| **Scaling** | Heart pulse `1 + 0.08·sin(6t)`; rings expand 0.2→3.0; petrify meter grows | All states |
| **Composite** | Lid = `T(hinge) · R(θ) · T(-hinge)` applied after the lid is positioned on the body | Lid must swing on its rim, not spin through the lamp |
| **Hierarchical** | Pans follow beam; snakes follow head; rings follow column; limbs follow torso | Rotate a parent — children must move with it |

---

## 5. Lighting and materials

Per-fragment **Blinn–Phong** in `phong.frag`. Support at least 4 point lights + 2 spotlights
via uniform arrays (`#define MAX_POINT_LIGHTS 4`).

```glsl
struct Light {
    int   type;               // 0 = directional, 1 = point, 2 = spot
    vec3  position, direction, color;
    float intensity;
    float constant, linear, quadratic;   // attenuation
    float cutOff, outerCutOff;           // spot cone (cosines)
};
struct Material {
    vec3  ka, kd, ks;
    float shininess;
    sampler2D diffuseMap, specularMap, normalMap;
    int   useDiffuseMap, useSpecularMap, useNormalMap;
};
```

Final colour = `Σ over lights of (ambient + diffuse + specular) · attenuation · spotFactor`.

### Light rig

| Light | Type | Colour | Notes |
|---|---|---|---|
| Torch L / R | Point | warm `(1.0, 0.55, 0.2)` | `intensity = base + 0.12·noise(t)` flicker; quadratic attenuation |
| Djinn energy | Point | cyan/green `(0.2, 0.9, 1.0)` | intensity ramps 0→1 during `BALANCED`; strong attenuation so falloff is visible |
| Medusa eyes | Spot ×2 | pale green | `cutOff = cos(12°)`, `outerCutOff = cos(18°)`; only active in `CURSED`, aimed at traveller |
| Fill | Directional | dim blue | optional, keeps back walls from going pure black |

### Material table (demonstrates varying kₐ, k_d, k_s, n_s)

| Object | ka | kd | ks | shininess |
|---|---|---|---|---|
| Gold scale beam/pans | 0.24, 0.20, 0.07 | 0.75, 0.61, 0.23 | 0.63, 0.56, 0.37 | 51.2 |
| Brass lamp | 0.33, 0.22, 0.03 | 0.78, 0.57, 0.11 | 0.99, 0.94, 0.81 | 27.9 |
| Stone guardian | 0.10, 0.10, 0.10 | 0.42, 0.42, 0.44 | 0.06, 0.06, 0.06 | 4.0 |
| Snake scales | 0.05, 0.10, 0.05 | 0.20, 0.45, 0.22 | 0.35, 0.45, 0.35 | 24.0 |
| Traveller (flesh) | 0.15, 0.12, 0.11 | 0.66, 0.52, 0.45 | 0.30, 0.30, 0.30 | 22.0 |
| Traveller (petrified) | 0.10, 0.10, 0.10 | 0.44, 0.44, 0.46 | 0.05, 0.05, 0.05 | 3.0 |
| Sandstone chamber | 0.12, 0.10, 0.08 | 0.55, 0.45, 0.33 | 0.04, 0.04, 0.04 | 2.0 |

**Petrification** is a material lerp driven by a height threshold, so stone visibly creeps
upward instead of cross-fading the whole body at once:

```glsl
float stone = clamp((petrifyLevel * bodyHeight - vWorldPos.y + 0.15) / 0.3, 0.0, 1.0);
vec3  kd    = mix(fleshKd, stoneKd, stone);
float ns    = mix(22.0, 3.0, stone);
```

---

## 6. Step-by-step build plan

Work in order. Each phase ends with a checkpoint you can see on screen — never move on
without it, because debugging two broken systems at once is where these projects die.

### ✅ Phase 0 — Build system — DONE
1. ~~`build.ps1` compiling every `src/*.cpp` and `src/*.c` with the UCRT64 compiler.~~
2. ~~Fixed the "OpenGL 4.6" message; window is now 1280×720 with a framebuffer-size callback.~~
3. ~~`glEnable(GL_DEPTH_TEST)` + clearing `GL_DEPTH_BUFFER_BIT`.~~
4. ~~`deltaTime` timer and live FPS/frame-time in the title bar.~~

### ✅ Phase 1 — Shader class + first triangle — DONE
1. ~~`Shader` class ([src/Shader.h](src/Shader.h), [src/Shader.cpp](src/Shader.cpp)): compile,
   link, full info-log printing on failure, uniform-location cache, and `setInt/setFloat/
   setVec2..4/setMat3/setMat4` helpers.~~
2. ~~`shaders/phong.vert` / `.frag` as a pass-through with a `uTime` pulse, proving the uniform
   path works end to end.~~
3. ~~Hard-coded gold/cyan/stone triangle proving the VAO→VBO→draw pipeline.~~
4. ~~**F5 hot-reloads shaders in place.** A failed recompile keeps the last working program, so a
   typo never blanks the screen mid-demo.~~

### ✅ Phase 2 — Mesh + primitives + camera — DONE
1. ~~`Mesh` ([src/Mesh.h](src/Mesh.h)): `Vertex { position, normal, uv }`, `MeshData` split from
   the GL object so geometry can be built without a context, VAO/VBO/EBO, move-only ownership.~~
2. ~~`Primitives` ([src/Primitives.cpp](src/Primitives.cpp)): `cube`, `sphere`, `cylinder`,
   `cone`, `torus`, `plane`, `disk` — all with derived outward normals and UVs.~~
3. ~~`Camera` ([src/Camera.h](src/Camera.h)): orbit + free-fly with **seamless mode switching**
   (no viewpoint jump on `C`), drag-look, scroll zoom, perspective from the live aspect ratio.~~
4. ~~`uNormalMatrix` (inverse-transpose) uploaded per draw, so the Phase 7 pulsing heart and
   Phase 8 expanding rings will light correctly under non-uniform scale.~~
5. ~~Debug views: **`N`** renders normals as RGB, **`T`** toggles wireframe.~~

Notes for later phases:
- **Back-face culling is deliberately still off.** Turn it on at the start of Phase 3
  (`glEnable(GL_CULL_FACE)`) — it is the cheapest way to catch a winding mistake, because any
  face wound backwards simply vanishes. Do it while the test row is still on screen.
- The cone's side normal is `normalize(height·cosθ, radius, height·sinθ)`, not `(cosθ, 0, sinθ)`.
  The surface leans inward, so the naive normal puts the highlight in the wrong place.

### ✅ Phase 3 — Scene graph — DONE
1. ~~`SceneNode` ([src/SceneNode.h](src/SceneNode.h)): position / rotation / **pivot** / scale,
   owned children, `localMatrix()`, `updateWorld()`, recursive `draw()`, `find()` by name,
   and `worldForward()` for aiming Medusa's spotlights in Phase 9.~~
2. ~~`Scene` ([src/Scene.cpp](src/Scene.cpp)): the full §4.1 tree — chamber, torches, Anubis's
   scale with Anubis's head, the Djinn lamp, Medusa with 7 four-segment snakes, the traveller.~~
3. ~~Back-face culling enabled, with **`B`** to toggle it off for comparison.~~
4. ~~`Scene::update()` sways the beam so the hierarchy is demonstrably working before the
   state machine exists.~~

**The pivot is the composite transform.** `local = T(position)·T(pivot)·R·T(-pivot)·S`, so
setting `lampLid->pivot = {-0.32, 0, 0}` makes the lid swing on its rim in Phase 8 with no
special-case matrix code — the requirement is satisfied by the node type itself.

**Undoing a parent's scale.** Because scale cascades to children, any node whose parent is
non-uniformly scaled sets the reciprocal on itself (see `unbeam` and `untorso` in
`Scene.cpp`). Without this, the scale pans would inherit the beam's 4.6× stretch. Worth
knowing if you add nodes: place children under an unscaled pivot node, or divide it out.

Animation handles are public on `Scene` (`beam`, `panLeft`, `heart`, `lampLid`,
`energyColumn`, `energyRings`, `medusaHead`, `snakeSegments`, `petrifyMeter`). Phases 7–9
drive the whole trial by writing to these — no phase after this needs to know the tree shape.

### ✅ Phase 4 — Blinn–Phong lighting — DONE
1. ~~`shaders/phong.frag` rewritten: per-fragment Blinn–Phong over a mixed rig, with
   attenuation, soft-edged spot cones, tone mapping and gamma.~~
2. ~~`Light` ([src/Light.h](src/Light.h)) + `uploadLights()` — directional / point / spot in one
   struct, angles stored in degrees and converted to cosines on upload.~~
3. ~~`Material` ([src/Material.h](src/Material.h)) with the full §5 table, plus an `emissive`
   term and `Materials::lerp()` ready for Phase 9's petrification.~~
4. ~~The complete light rig: 2 flickering torches, the Djinn point light, Medusa's two eye
   spotlights, and a dim directional fill.~~
5. ~~**`L`** draws emissive markers at every light position.~~

Notes:
- **Ambient is applied once, not per light** (`uAmbient * ka`). Summing it per light makes the
  ambient term scale with light count and washes the scene out as you add lights.
- **Specular is zeroed when `N·L <= 0`.** Without that guard, surfaces facing away from a light
  still catch a highlight and glow along their dark edge.
- Lights are rebuilt every frame in `Scene::updateLights()`, *after* `updateWorld()`, so a light
  anchored to a node follows it automatically. Order matters — build them before the transform
  flush and they lag a frame behind.
- The torch flicker sums two sines at unrelated frequencies (8.3 Hz and 19.7 Hz). A single sine
  reads as a mechanical pulse rather than fire.
- Blinn–Phong, not Phong: the half-vector holds the highlight's shape at grazing angles, where
  Phong's reflection vector cuts it off abruptly.

### ✅ Phase 5 — Textures — DONE
1. ~~`stb_image.h` v2.30 downloaded to `include/stb/`, implementation compiled in exactly one
   translation unit ([src/Texture.cpp](src/Texture.cpp)).~~
2. ~~`Texture` ([src/Texture.h](src/Texture.h)): mipmaps, `GL_REPEAT`,
   `GL_LINEAR_MIPMAP_LINEAR`, vertical flip on file load, move-only ownership.~~
3. ~~**All textures are generated procedurally** ([src/ProceduralTexture.cpp](src/ProceduralTexture.cpp))
   — sandstone, flagstones, brushed gold, brass with verdigris, granite, snake scales, plus
   three specular maps. No binary assets required.~~
4. ~~Shader branches on `useDiffuseMap` / `useSpecularMap`; `uvScale` per material handles
   tiling; **`Y`** toggles all textures off for comparison.~~

**Why procedural rather than image files:** the project stays self-contained and diffable, and
every map is guaranteed tileable. To use a real image instead, call
`Texture::loadFromFile("assets/textures/wall.jpg")` in `Scene::buildTextures()` — nothing else
changes.

Notes:
- **Tileability is in the noise, not the filter.** The value-noise lattice wraps modulo its
  period, and each fbm octave doubles that period, so every octave wraps too. Non-wrapping
  noise gives a visible seam wherever the wall repeats.
- **Diffuse and specular maps must share their noise.** `brassPatina` and `brassSpecular` use
  the same seed and threshold, so the highlight dies exactly where the verdigris appears. Get
  this wrong and the shine floats independently of the visible corrosion.
- **Ambient is multiplied by the diffuse map too.** Otherwise unlit regions show a flat
  silhouette with no surface detail.
- **An empty sampler slot gets a 1×1 white texture** (`Texture::white()`), not nothing. Some
  drivers misbehave when a sampler points at an unbound unit even behind a uniform branch, and
  white is the identity for the multiply.
- Textures are uploaded as plain `GL_RGB` with the shader's existing tone curve, rather than
  `GL_SRGB8`. Fine for a hand-tuned look; revisit if colours seem washed out after Phase 10.

### ✅ Phase 6 — State machine — DONE
1. ~~`TrialState` + `TrialController` ([src/TrialState.h](src/TrialState.h)) with `state`,
   `stateTime`, `progress()`, `heartWeight`, latched verdict and `petrification()`.~~
2. ~~`update(dt)` advances and transitions exactly per §4.2.~~
3. ~~Input: `SPACE` begins, `↑`/`↓` sweep the weight, `R` resets, `1`/`2` force an outcome,
   `I` switches between trial control and manual inspection.~~
4. ~~Transitions log to the console with the weight and verdict angle.~~
5. ~~`Easing` ([src/Easing.h](src/Easing.h)): `smoothstep01`, `easeOutBack`, `dampedSettle` and
   friends — Phases 7–9 draw all their motion curves from here.~~
6. ~~`Scene::applyTrial()` maps state → animation targets, so the full story loop already
   runs end to end.~~

**Verified headlessly.** The controller has no GL dependency, so it was compiled standalone
and stepped at 60 Hz: Placing 2.02 s, Weighing 3.02 s, Balanced 6.00 s, Cursed 8.00 s,
Reset 2.02 s, and the branch flips between weight 0.61 (passes) and 0.63 (fails). Worth
re-running if you change any timing — the harness is
`scratchpad/test_trial.cpp`, compiled against `TrialState.cpp` alone.

Notes:
- **The verdict latches at `begin()`, not when the beam stops.** `Weighing` then animates
  toward an outcome already known, which is what lets the beam overshoot and settle without
  the result ever being in doubt.
- `isBalanced()` reports a *live prediction* while Waiting and the *latched verdict* after,
  so the readout tracks the weight as you dial it in but cannot contradict the animation
  once committed. (The first version latched only on commit and cheerfully displayed
  "balanced" for a heart weighing 0.90.)
- **`applyTrial()` restates every target each frame** rather than only the ones the current
  state changes. A state can then never leak a stale value into the next.

### ✅ Phase 7 — Scale animation — DONE
1. ~~The heart travels an eased arc from the traveller's hands onto the pan during `Placing`,
   turning over once (`easeOutCubic`) as it flies, and returns home during `Reset`.~~
2. ~~Heart pulse scaling, beating faster as it is judged, and deliberately non-uniform.~~
3. ~~Beam rotation to the verdict angle with `dampedSettle` — it overshoots and wobbles in.~~
4. ~~Pans counter-rotate so they hang level. (Done in Phase 3.)~~
5. ~~The heart carries its own red point light along the arc, brightest in flight.~~

**The heart never reparents.** It is a child of the left pan for its whole life, so it
inherits the tilt the instant it lands with no reparenting code. The arc is therefore
authored in the *pan's local space*, which is legitimate only because that space equals world
space up to a translation while the beam is level — and `Placing` always runs level.

**Verified numerically**, not by eye. Building with `-DTRIAL_DEBUG_PLACEMENT` prints the pan's
world frame; its basis vectors must come back as exact identity, confirming the assumption
above. Measured: pan at `(-1.893, 3.172, -2.0)`, heart at home `(0.197, 2.11, 4.60)` — in
front of the traveller at `(0,0,5.5)` — and the arc apex at y≈4.47, z≈1.3, clear of the beam.
Re-run that if you ever move the scale or change the beam's scale.

- **The travel is eased before the arc is added**, not after. Easing the summed position
  instead would drag the hump's peak off-centre.
- The vertical hump matters: a straight lerp slides the heart through the pan's rim on the
  way in.

### ✅ Phase 8 — Blessing branch — DONE
1. ~~The lid swings open on its hinge pivot with `easeOutBack`, overshooting to 1.100 before
   settling — a physical "pop" rather than a glide.~~
2. ~~The energy column rises out of the lamp mouth as it grows, translucent at 0.42 alpha.~~
3. ~~Five rings on staggered phase offsets, expanding as they rise, fading in *and* out via
   `sin(cycle·π)`, cooling from cyan to deep blue as they lose energy.~~
4. ~~Djinn point light intensity ramps with the column.~~
5. ~~**A sorted transparent pass**: `Material::opacity`, `SceneNode::collectTransparent()`,
   and a back-to-front sort in `Scene::draw()`.~~

**The transparency pass, in order of what breaks without it:**
- **Depth writes off, depth test ON.** Test off would draw rings through the walls; writes on
  would make overlapping rings occlude each other instead of blending.
- **Sorted back-to-front.** Alpha blending is order-dependent — a near surface drawn first
  blends against the background rather than against what is actually behind it.
- **Culling disabled for the pass.** The rings are thin shells; culling back faces drops half
  of each and they read as broken arcs. The previous culling state is queried and restored,
  not assumed, so the `B` key still works.

**`directDrive` matters.** Trial-driven targets are already shaped curves and are applied
verbatim; manual inspection toggles are step inputs and get eased. Easing an already-eased
curve smooths the lid's overshoot straight back out — the pop simply disappears.

**Easing verified numerically** (`scratchpad/test_easing.cpp`). This caught a real bug:
`dampedSettle` originally had a `* 0.25` frequency factor that limited it to 0.65 of a cycle,
so it overshot once and decayed straight back — a soft landing, not a wobble. Now it
overshoots to 1.293, swings back to 0.922, and settles: the beam reads as having mass. Both
curves are exact at their endpoints (0 → 0, 1 → 1).

### ✅ Phase 9 — Curse branch — DONE
1. ~~Medusa's head aims at the traveller with a real `atan2` solve, yaw *and* pitch.~~
2. ~~Eye spotlights follow the head's world forward vector. (Rig built in Phase 4.)~~
3. ~~Snakes rouse: amplitude *and* frequency both rise with `snakeAgitation`, starting just
   ahead of the head finishing its turn so the threat registers before the gaze lands.~~
4. ~~Petrification as a per-fragment height threshold in `phong.frag`, blending ka, kd, ks
   *and* shininess — moderately shiny flesh into rough stone.~~
5. ~~The petrify meter grows alongside it.~~

**Petrification is per fragment, not per node.** The front is a world-Y threshold, so a single
limb is part flesh and part stone while the boundary passes through it. Blending whole nodes
would make the traveller change in visible chunks. A faint green glow rides the leading edge
(`stone·(1-stone)·4`, peaking mid-transition) so the front is visible as it climbs.

**Aiming in Medusa's local space, not world space.** Her body is posed at −35°, so a
world-space angle would be wrong by exactly that much. Pulling the target through
`inverse(medusaRoot->world())` makes her body's pose irrelevant. The solve inverts the exact
composition `localMatrix()` builds — `R = Ry·Rx·Rz` gives a forward of
`(sinY·cosX, −sinX, cosY·cosX)`, hence `yaw = atan2(d.x, d.z)` and
`pitch = atan2(−d.y, √(d.x²+d.z²))`.

**Verified numerically.** Building with `-DTRIAL_DEBUG_AIM` forces a full turn and compares
the head's actual world forward against the true direction to the traveller:

```
[aim] want dir  = -0.642 -0.161  0.749
[aim] got  dir  = -0.642 -0.161  0.749
[aim] dot       =  1.00000
```

- `uPetrifyEnabled` is set per node in `drawSelf()`, and cleared by hand in
  `drawLightMarkers()` — those bypass `drawSelf()`, and the last node drawn is often part of
  the traveller.
- `snakeAgitation` unwinds during `Reset` **only if the curse actually roused them**; a
  blessing must leave them undisturbed rather than calming from a peak they never reached.

### ✅ Phase 10 — Polish — DONE
1. ~~`Reset` eases every animated value back to rest, staggered into separate beats.~~
2. ~~`CameraDirector` ([src/CameraDirector.cpp](src/CameraDirector.cpp)) frames each state:
   wide establishing shot, in on the scale for the verdict, left to the lamp for a blessing,
   round to Medusa and the traveller for a curse. A slow drift keeps `Waiting` alive.~~
3. ~~`Hud` ([src/Hud.cpp](src/Hud.cpp)) with its own screen-space shader: a state-coloured
   progress bar and a weight track showing the tolerance window and the current marker.~~
4. ~~**`F`** toggles the director, **`H`** the overlay. Dragging the mouse cancels the
   director automatically.~~

**Angle interpolation is wrapped, not linear.** `shortestAngleDelta` folds the difference into
[-180, 180], so a shot crossing the ±180 boundary does not send the camera the long way round.
This is the classic bug in per-state camera framing.

**The director writes into the Camera rather than replacing it**, so handing control back is
just a flag — the view never jumps. Dragging the mouse switches it off mid-shot, because
fighting a director for the camera is maddening.

**The HUD is not a text renderer.** A bitmap font is a project of its own; colour and position
carry this much state perfectly well. Bar sizes are computed in pixels and converted to NDC,
so the overlay keeps a constant thickness at any window size.

- HUD init failure is non-fatal — the scene is still watchable, and the title bar carries the
  same information.
- Both the HUD and the transparent pass **query and restore** the culling state rather than
  assuming it, so the `B` key keeps working.

### Phase 11+ — Stretch goals, in value order

#### ✅ 1. Shadow mapping — DONE
- ~~`ShadowMap` ([src/ShadowMap.cpp](src/ShadowMap.cpp)): 1024² depth-only FBO, `depth.vert` /
  `depth.frag`, 3×3 PCF in `phong.frag`. **`G`** toggles it.~~
- The left torch casts (`lights[0]`). Point lights strictly need a cube map for
  omnidirectional shadows; one perspective frustum aimed at the chamber is far cheaper and,
  for a wall-mounted torch facing inward, visually equivalent.

Four things that each cause a specific, recognisable artefact if omitted:
- **`GL_CLAMP_TO_BORDER` with a white border.** Outside the light's frustum there is no data;
  with `GL_REPEAT` the map tiles and stamps phantom shadows across the chamber.
- **Front-face culling during the depth pass.** Recording the *far* side of each object pushes
  the comparison depth away from the receiver, removing nearly all self-shadowing acne
  without a large bias. Culling is forced on for that pass rather than inherited, or the `B`
  key would silently bring the acne back.
- **Slope-scaled bias**, `max(0.004·(1−N·L), 0.0009)`. A surface edge-on to the light spans
  many depth values inside one texel and needs far more bias than one facing it square on. A
  single constant either leaves acne on slopes or makes flat surfaces float free of their
  own shadow (peter-panning).
- **Ambient is not shadowed.** Only the direct terms are attenuated; killing ambient too would
  make shadowed regions pure black rather than dimly lit.

The transparent energy rings are excluded from the depth pass — they are light itself and
must not cast a solid silhouette.

#### ✅ 2. Particles for Djinn smoke — DONE
- ~~`ParticleSystem` ([src/ParticleSystem.cpp](src/ParticleSystem.cpp)) with
  `particle.vert` / `particle.frag`. **`J`** toggles it.~~
- ~~Emission strength follows the energy column, so the plume grows and dies with the blessing
  rather than switching on and off.~~

**This is the instanced-rendering requirement too.** One four-vertex quad lives in the buffer;
`glVertexAttribDivisor` makes centre, size and colour advance once per *instance*, and
`glDrawArraysInstanced` draws the whole plume of ~500 motes in a **single draw call**. Without
the divisors every particle reads instance 0's data and the plume collapses to one mote.

- **Additive blending needs no depth sort.** Addition is commutative, so overlapping motes just
  accumulate into brighter cores — unlike the alpha-blended rings, which do need sorting.
- **Depth test on, depth writes off.** Motes correctly hide behind the lamp but never occlude
  each other.
- **Billboards use the view matrix's first two columns** as the camera's right and up axes. The
  view matrix is the authority on what "right" means on screen.
- **The buffer is orphaned before each upload** (`glBufferData(..., nullptr)` then
  `glBufferSubData`), so the driver hands back fresh storage instead of stalling until last
  frame's draw has finished reading the old contents.
- **Fractional spawns carry across frames**, or the rate rounds down to zero every frame at
  high frame rates.

**Verified numerically** with `-DTRIAL_DEBUG_PARTICLES`, which forces full emission and reports
the live count. It ramps to a steady **~495 / 700**, matching the predicted 220/s × 2.25 s mean
life. Steady rather than climbing to the cap is the proof that nothing leaks and the fixed pool
recycles correctly.

#### ✅ 3. Post-processing FBO — DONE
- ~~`PostProcess` ([src/PostProcess.cpp](src/PostProcess.cpp)) with `post.vert` / `post.frag`:
  the scene renders into a colour texture, then a full-screen pass desaturates, tints and
  vignettes it. **`U`** toggles it.~~
- ~~Driven by `petrifyLevel` — colour drains out of the *whole chamber* as the curse takes
  hold, with the corners closing in. The blessing gets a slight cyan lift instead.~~

**This is the effect that has to be post-process.** No per-object shader can desaturate the
finished frame, because it only exists once everything has been composed.

- **A single oversized triangle, not two triangles.** It covers the screen with no diagonal
  seam, and shades each pixel once rather than twice along a quad's shared edge.
- **Rec. 709 luma weights** `(0.2126, 0.7152, 0.0722)`, not a flat channel average. The eye is
  far more sensitive to green; averaging makes distinct reds and blues collapse to the same
  muddy grey.
- **The tint is applied against luma**, so it reads as a light the scene is lit by rather than
  a filter laid over the top.
- **Depth is a renderbuffer, not a texture** — nothing samples it, and renderbuffers are
  cheaper when you only need to render to them.
- **The target follows the window size.** A stale target would be stretched; a minimised
  window reports zero, which would make the framebuffer incomplete, so sizes clamp to 1.
- **The HUD is drawn after the resolve.** It is an instrument, not part of the scene, and must
  stay legible while the image desaturates.

#### ✅ 4. Normal mapping — DONE
- ~~`Vertex` gained a tangent; `Primitives::computeTangents()` derives one for every generator
  from positions and UVs. **`V`** toggles the effect.~~
- ~~`Procedural::normalFromLuminance()` differentiates each diffuse map into a tangent-space
  normal map with a Sobel filter. Applied to sandstone, granite and snake scales.~~
- ~~TBN built in `phong.vert`, applied per fragment in `phong.frag`.~~

**Deriving the normal maps from the diffuse luminance is deliberate.** For these textures the
shading *is* the relief — the dome across each scale, the grain in sandstone — so the diffuse
doubles as a height field and the two register perfectly. No separate authoring, no
misalignment.

- **Sobel, not a two-tap difference.** It weights the diagonals too, which matters a great deal
  when the height field is fractal noise; a two-tap gradient turns it into static.
- **The height lookup wraps.** Clamping would leave a seam of flat normals along every edge,
  breaking the tileability the diffuse maps were carefully built to have.
- **Gram-Schmidt in the vertex shader.** Interpolating tangents across a triangle tilts them
  off the surface; a skewed frame shears the mapped normals.
- **`vTangent`/`vBitangent` as separate varyings**, not a `mat3` — some drivers handle matrix
  varyings poorly.
- **The debug normals view (`N`) samples *after* the perturbation**, so it shows the normals
  actually used for lighting.

**Verified numerically** (`scratchpad/test_tangents.cpp`). All seven primitives produce
unit-length tangents perpendicular to their normals to float precision (`|len−1|` and `|N·T|`
both ≤ 1.2e-7), with no degenerate frames, and the sphere's tangent matches the analytic
∂P/∂u exactly (dot = 1.00000).

#### 5. Imported 3D model — Assimp; swap a primitive-built prop for a real mesh.

#### 6. Instanced rubble — a field of scattered debris in one draw call.
(Instancing is already demonstrated by the particle system.)
3. **Normal mapping** — TBN in the vertex shader; makes sandstone and snake scales pop.
4. **Post-processing FBO** — render to texture, then a screen quad; desaturate + vignette that
   ramps with `petrifyLevel` during `CURSED`. Very strong storytelling beat.
5. **Assimp model import** — swap a primitive-built prop for a real mesh.
6. **Instanced rendering** — a field of scattered rubble or many rings in one draw call.

---

## 7. Controls (document these for the demo)

| Key | Action |
|---|---|
| `SPACE` | Place the heart / begin the trial |
| `↑` / `↓` | Increase / decrease heart weight |
| `1` / `2` | Force balanced / cursed outcome |
| `R` | Reset to `WAITING` |
| Left-drag | Orbit / look ✅ |
| `C` | Toggle Orbit / FreeFly camera ✅ |
| `W A S D` + `Q E` | Free-fly movement ✅ |
| Scroll | Zoom — orbit distance, or FOV in free-fly ✅ |
| `N` | Debug: render normals as RGB ✅ |
| `T` | Toggle wireframe ✅ |
| `B` | Toggle back-face culling ✅ |
| `L` | Toggle light position markers ✅ |
| `Y` | Toggle textures on / off ✅ |
| `F` | Toggle the cinematic camera ✅ |
| `H` | Toggle the on-screen bars ✅ |
| `I` | Switch between trial and manual inspection ✅ |
| `[` `]` | Tilt the beam by hand ✅ |
| `O` | Open / shut the lamp lid ✅ |
| `M` | Turn Medusa's head (arms her spotlights) ✅ |
| `K` | Show / hide the Djinn energy column ✅ |
| `P` | Grow / shrink the petrification meter ✅ |
| `X` | Reset all inspection state ✅ |
| `F5` | Hot-reload shaders ✅ |
| `ESC` | Quit ✅ |

---

## 8. Time budget

| Phases | Work | Estimate |
|---|---|---|
| 0–2 | Foundation (build, shaders, meshes, camera) | ~7 h |
| 3–5 | Scene graph, lighting, textures | ~10 h |
| 6–9 | Animation and the two branches | ~12 h |
| 10 | Polish | ~3 h |
| 11+ | Stretch goals | 4–6 h each |

**~32 hours to a complete, submittable project**, before stretch goals.

---

## 9. Traps to avoid

- **Normal matrix.** Non-uniform scaling breaks lighting unless you use
  `mat3(transpose(inverse(model)))`. The pulsing heart and growing rings will expose this.
- **Degrees vs radians.** GLM takes radians. Wrap every angle in `glm::radians()`.
- **Transform order.** `model = T · R · S`, applied right-to-left. Scaling after rotating skews things.
- **Hinge rotation.** Rotating the lid without the translate-to-hinge sandwich makes it orbit the
  lamp's centre and clip through the body.
- **Depth vs blending.** Draw all opaque geometry first, then transparent rings sorted back-to-front,
  with `glDepthMask(GL_FALSE)` during the transparent pass.
- **Uniform arrays.** Set `numLights` explicitly; a stale count reads garbage and produces black or
  blown-out frames.
- **Shader hot-reload.** A key that recompiles shaders at runtime pays for itself in Phase 4 alone.

---

## 10. Demo talking points

When presenting, name each requirement as you show it:

1. Orbit the camera → perspective projection, view matrix, depth testing.
2. Point at gold vs stone under the same torch → different kₐ/k_d/k_s/n_s in Blinn–Phong.
3. Press `SPACE` → translation (heart), scaling (pulse), rotation (beam).
4. Point at the pans staying level → hierarchical transforms with child compensation.
5. Balanced outcome → composite hinge rotation on the lid, attenuated coloured point light,
   alpha-blended expanding rings.
6. Cursed outcome → spotlight cones with inner/outer cutoff, chained snake hierarchy,
   per-fragment material interpolation driving petrification.
7. Note that every value is computed from elapsed time in the render loop — no precomputed frames.
