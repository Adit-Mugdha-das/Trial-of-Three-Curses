# The Trial of Three Curses: Escape from the Collapsing Chamber

The project becomes **interactive**. The weighing still plays as a scripted opening, but once
the verdict lands a treasure appears, the player takes control of the traveller, and grabbing
the treasure brings the chamber down. Medusa gives chase along a collapsing corridor while
stones fall from the ceiling. Reach the exit and you live; be caught and you turn to stone.

> This extends [PROJECT_PLAN.md](PROJECT_PLAN.md), it does not replace it. Phases 0–10 and the
> shadow / particle / post-process / normal-map work all stay exactly as they are.

---

## 1. What already exists and carries over unchanged

This matters, because the new feature list looks much bigger than the work actually is.
Roughly **80% of what the escape sequence needs is already built and verified**:

| Need | Already have |
|---|---|
| Moving an object per frame | `SceneNode` position/rotation + `updateWorld()` |
| A traveller with limbs to animate | Hierarchical traveller (torso → head, arms, legs) |
| Medusa aiming at the traveller | `atan2` gaze solve, verified `dot = 1.00000` |
| Turning to stone when caught | Per-fragment petrification height threshold |
| Falling rubble with dust | `ParticleSystem`, instanced, pooled |
| Lit chamber with shadows | Blinn–Phong rig + 1024² shadow map |
| A camera that follows the action | `CameraDirector`, one shot per state |
| On-screen bars | `Hud` with its own screen-space shader |
| Screen draining of colour | `PostProcess` desaturate + vignette |
| Frame-rate independent motion | `deltaTime` everywhere, `Easing` helpers |

**Genuinely new:** player input → movement, a treasure, a corridor, a pursuer, falling stones,
collision checks, and four new states.

---

## 2. Two decisions to make up front

### 2.1 The weighing stays a cutscene; control starts at the treasure

The player does **not** walk around during `Placing` / `Weighing`.

The heart's flight arc is authored in the *scale pan's local space*, which equals world space
only while the beam is level — verified and documented in PROJECT_PLAN §Phase 7. If the
traveller could wander during the weighing, the heart's start point would have to chase him
and that verified geometry would break, for no gameplay gain.

So: **the verdict is a cutscene; the game begins when the treasure appears.** It is also
better pacing — a calm beat before the panic.

### 2.2 The verdict sets difficulty instead of ending the story

Today `Balanced` and `Cursed` are two endings. In the new flow the story continues either way,
so the verdict becomes a **handicap**, which keeps the weighing meaningful:

| Verdict | Effect on the escape |
|---|---|
| **Balanced** (heart is true) | Medusa starts slower and further back; the Djinn light follows you down the corridor; fewer stones fall |
| **Cursed** (heart is false) | Medusa starts at full speed right beside you; the corridor is darker; stones fall more often |

One line of code each, and the weighing suddenly has stakes. "The verdict is not a branch, it
is a difficulty modifier" is a good thing to be able to say in your viva.

---

## 3. The new state machine

```
Waiting
  -> Placing           heart flies to the pan           (2.0 s, cutscene)
  -> Weighing          beam settles to the verdict      (3.0 s, cutscene)
  -> Balanced|Cursed   verdict beat, shortened          (3.0 s, cutscene)
  -> TreasureRevealed  treasure rises; control unlocks  (1.5 s, then open-ended)
  -> Escape            collapse, chase, falling stones  (open-ended)
  -> Escaped | Caught                                   (4.0 s each)
  -> Reset                                              (2.5 s)
  -> Waiting
```

```cpp
enum class TrialState
{
    Waiting, Placing, Weighing,
    Balanced, Cursed,        // now verdict beats, shortened to 3 s
    TreasureRevealed,        // NEW - treasure rises, player gains control
    Escape,                  // NEW - collapse + chase
    Escaped,                 // NEW - win
    Caught,                  // NEW - petrified
    Reset
};
```

**The real structural change:** until now every transition was time-driven. Two of these are
not — they end on a player action. `durationOf()` returns `0.0f` for them, exactly as
`Waiting` already does, and `main` feeds the conditions in each frame.

| From | To | Condition |
|---|---|---|
| `TreasureRevealed` | `Escape` | `distanceXZ(player, treasure) < 1.3` |
| `Escape` | `Escaped` | `player.z > kExitZ` |
| `Escape` | `Caught` | `distanceXZ(player, medusa) < 1.6` |

---

## 4. Layout

The chamber is `x ∈ [-13, 13]`, `z ∈ [-11, 12]`, and the **+Z side is already open** — exactly
where a corridor wants to go. Medusa sits at `(6, 0, -1.5)`, so she starts *beside* you: you
take the treasure right next to her and run past her.

```
   z = -11   back wall, two torches
             +------------------------------+
             |  lamp(-6)   scale(0,-2)      |   Medusa (6, -1.5)
             |         * treasure (0,-2)    |
             |                              |
             |        traveller start       |
   z = +12   +----------+        +----------+   front wall + GATE
                        |        |
                        |        |   corridor: x in [-4, 4]
                        |        |   ceiling y = 7
                        |        |   stones fall here
                        |        |
   z = +70              +--------+   EXIT
```

| Thing | Value |
|---|---|
| Treasure | `(0, 1.4, -2.0)` — floating in front of the scale |
| Traveller start | `(0, 0, 5.5)` (unchanged) |
| Gate | `z = 11.5`, opening `x ∈ [-3.5, 3.5]` |
| Corridor | `z ∈ [11.5, 72]`, walls at `x = ±4`, ceiling `y = 7` |
| Exit line | `z = 70` |
| Total run | ≈ 72 units ≈ 13 s at full speed |

---

## 5. Tuning constants — all in one file

Every number that affects how the game *feels* goes in `src/EscapeTuning.h`, so you can balance
without hunting through code. This is the most useful thing you can do for yourself before
playtesting starts.

```cpp
namespace Tuning
{
    // --- player ---
    constexpr float kPlayerSpeed      = 5.6f;   // units / second
    constexpr float kPlayerTurnRate   = 720.0f; // degrees / second toward heading
    constexpr float kPlayerAccel      = 26.0f;  // so starts and stops are not instant
    constexpr float kPlayerRadius     = 0.45f;

    // --- pursuer ---
    constexpr float kMedusaSpeedStart = 4.8f;
    constexpr float kMedusaSpeedEnd   = 6.6f;   // exceeds the player: she closes late
    constexpr float kMedusaRampTime   = 12.0f;
    constexpr float kCatchRadius      = 1.6f;

    // --- handicap from the verdict ---
    constexpr float kBlessedSpeedBonus = -0.4f;
    constexpr float kBlessedHeadStart  =  6.0f;

    // --- falling stones ---
    constexpr int   kMaxStones       = 24;
    constexpr float kStoneInterval   = 0.55f;
    constexpr float kStoneAheadMin   = 8.0f;    // spawn ahead of the player
    constexpr float kStoneAheadMax   = 22.0f;
    constexpr float kStoneGravity    = 16.0f;
    constexpr float kStoneRadius     = 0.7f;
    constexpr float kStunDuration    = 0.9f;
    constexpr float kStunSpeedFactor = 0.35f;

    // --- treasure ---
    constexpr float kPickupRadius    = 1.3f;
}
```

**Make the player slightly faster than Medusa and let her ramp past.** A chase where the
pursuer is always faster is hopeless; one where she is always slower is boring. The falling
stones are what create the real danger, by stunning you so she closes.

---

## 6. Collision — keep it 2D

The whole game happens on a flat floor, so **every check is 2D in XZ**. No physics engine, no
3D volumes. Say that plainly if asked: it is the simplification that makes this tractable.

```cpp
inline float distanceXZ(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
```

| Check | Method |
|---|---|
| Player ↔ treasure | `distanceXZ < kPickupRadius` |
| Player ↔ Medusa | `distanceXZ < kCatchRadius` |
| Player ↔ falling stone | `distanceXZ < kPlayerRadius + kStoneRadius` **and** stone airborne |
| Player ↔ corridor walls | Clamp `x` to `[-4 + r, 4 - r]` — a straight corridor needs nothing more |
| Player ↔ chamber walls | Clamp `x` and `z` to the room bounds |
| Player ↔ landed stone | Push out along the vector between centres |

Compare **squared** distances wherever the number is not shown to the player, and skip the
`sqrt` entirely.

---

## 7. New files

```
src/
├── EscapeTuning.h     the numbers from section 5
├── Player.h/.cpp      input -> velocity -> position, heading, walk cycle, stun
├── Pursuer.h/.cpp     Medusa's chase: distance-based following + speed ramp
└── Debris.h/.cpp      falling-stone pool: spawn, fall, land, block, fade
```

Everything else is an **edit**:

| File | Change |
|---|---|
| `TrialState.h/.cpp` | 4 new states; two transitions driven by conditions, not timers |
| `Scene.h/.cpp` | `buildCorridor()`, `buildTreasure()`; handles for `treasure`, `gate` |
| `CameraDirector.cpp` | Chase shot for `Escape`, reveal shot for `TreasureRevealed` |
| `Hud.cpp` | Distance-to-exit bar, Medusa-proximity danger bar |
| `main.cpp` | Wire `Player`, `Pursuer`, `Debris`; route WASD to the player |

---

## 8. Build order

Nine steps, each ending in something you can see. **Do not start the next until the current one
works** — a chase that fails because of a camera bug is very hard to diagnose.

### Step A — Player movement (2–3 h)
1. `Player`: position, heading, velocity, `update(input, dt)`.
2. WASD / arrows → a desired direction in world space; accelerate toward it rather than
   snapping. **Normalise diagonals**, or diagonal movement is 1.41× faster.
3. Turn the traveller node toward his heading at a capped rate.
4. Clamp to the chamber bounds.
5. **Resolve the camera conflict:** WASD currently drives free-fly. Make WASD mean "walk" by
   default and give the camera those keys only while `C` free-fly is active.

**Done when:** you can walk around the chamber, he turns to face his direction of travel, and
he cannot leave the room.

### Step B — Walk animation (1 h)
1. Swing `LegL` / `LegR` in opposite phase: `rotation.x = A·sin(t·f + φ)`.
2. Arms swing opposite the legs.
3. **Scale amplitude and frequency with actual speed**, so standing still means standing still.
4. Small torso bob at twice the leg frequency.

**Done when:** he walks when moving, stands when stopped, and does not foot-slide at speed.

### Step C — Treasure and pickup (1–2 h)
1. Build it: a gold jewel on a small pedestal, strong emissive material, its own warm point
   light. **You already have this exact pattern — the heart does it.**
2. Idle: rotate on Y, bob on a sine, pulse the light.
3. `TreasureRevealed`: it rises out of the floor with `easeOutBack` — the lamp lid's curve.
4. Pickup: `distanceXZ < kPickupRadius` → hide it, burst of particles, transition.

**Done when:** walking into it makes it vanish and the console logs the pickup.

### Step D — Corridor and gate (2 h)
1. `Scene::buildCorridor()` — floor, two walls, ceiling, all from `m_cube`. Reuse `kSandstone`
   with a large `uvScale` so it needs no new textures.
2. Front wall across `z = 11.5` with a gap for the gate.
3. `gate`: a slab that slides **up** into the wall when the treasure is taken, animated with
   `smoothstep01` like everything else in the project.
4. Torches along the corridor — but read the light budget note in section 9 first.

**Done when:** you can walk the corridor to the exit and the gate opens on pickup.

### Step E — The new states (1–2 h)
1. Extend `TrialState` and `durationOf()`; open-ended states return `0.0f`.
2. Condition-driven transitions in `update()`, fed from `main`.
3. Shorten `Balanced` / `Cursed` to 3 s and route both into `TreasureRevealed`.
4. Log every transition, as the existing code already does.
5. **Re-run and extend the headless harness** `scratchpad/test_trial.cpp` — drive the new
   conditions and confirm both endings are reachable. This caught a real bug last time.

**Done when:** the full loop runs and both endings appear in the log.

### Step F — Medusa's pursuit (2–3 h)
1. `Pursuer`: move toward the player each frame.
   ```cpp
   glm::vec3 toPlayer = playerPos - position;
   toPlayer.y = 0.0f;
   const float d = glm::length(toPlayer);
   if (d > 0.001f) { position += (toPlayer / d) * speed * dt; }
   ```
   That is the entire algorithm. "Simple distance-based following" really is this simple.
2. Ramp `speed` from `kMedusaSpeedStart` to `kMedusaSpeedEnd` over `kMedusaRampTime`.
3. Apply the verdict handicap to her start position and speed.
4. Face her body along her movement direction — the `atan2` yaw solve already exists.
5. Drive `snakeAgitation` from her speed, so she visibly gets more frantic as she closes.
6. Clamp her to the corridor once she is inside it.

**Done when:** she follows you down the corridor, gains ground over time, and catching you
triggers `Caught`.

### Step G — Collapse and falling stones (3 h)
1. `Debris`: a fixed pool of `kMaxStones`, the same pattern as `ParticleSystem` — never
   allocate mid-game, recycle dead slots.
2. Spawn ahead of the player at a random `x`, at ceiling height, on a timer.
3. Integrate with gravity; on landing emit a dust burst and leave the stone as an obstacle
   that fades after a few seconds.
4. **Draw a warning shadow ring on the floor under each falling stone.** Without it, being hit
   feels unfair rather than tense. This single detail is the difference between a good chase
   and a frustrating one.
5. Airborne stone hits the player → stun: speed × `kStunSpeedFactor` for `kStunDuration`.
6. Screen shake: offset the camera target by a decaying random vector on impact.

**Done when:** stones fall with visible warning, hits slow you, and Medusa noticeably gains
ground when you are hit.

### Step H — The endings (1–2 h)
1. `Caught`: freeze the player, snap Medusa's gaze onto him, ramp `petrifyTarget` 0 → 1, let
   the post-process drain the frame. **All four systems already exist** — this is mostly wiring.
2. `Escaped`: the corridor behind collapses, the Djinn column rises in the doorway, the
   post-process lifts to a warm tone.
3. Hold each 4 s, then `Reset`.

**Done when:** both endings look deliberate and the loop returns to `Waiting`.

### Step I — Camera, HUD, tuning (3 h)
1. `CameraDirector` gains a **chase shot** for `Escape`: behind and above the player, looking
   slightly ahead of him, **easing toward** the target rather than locking to it.
2. `TreasureRevealed` gets a slow push-in on the treasure.
3. HUD: a progress-to-exit bar and a danger bar that fills as Medusa closes, coloured with the
   existing `Hud::colorFor` pattern.
4. Playtest and tune section 5. Aim for a **50–60% win rate** for someone on their second run.

**Done when:** you win some runs and lose others, and losing feels like your own fault.

---

## 9. Traps specific to this update

- **The light budget is 8.** `MAX_LIGHTS` in `phong.frag` is 8 and you already use up to 7
  (2 torches, heart, Djinn, 2 eyes, fill). Corridor torches will silently overflow. Either
  raise `MAX_LIGHTS` **and** `kMaxLights` in `Light.h` together, or — better — select the
  nearest 2–3 corridor torches to the player each frame and upload only those. The second is
  the real technique and worth being able to say you did it.

- **The shadow map only covers the chamber.** `ShadowMap::setLight()` aims a 108° frustum at
  the room. Forty units down a corridor you are outside it — which renders *correctly* (border
  colour = lit) but means no shadows during the chase. Either re-aim the shadow light to follow
  the player, or accept it knowingly.

- **WASD already belongs to the free-fly camera.** Decide who owns it in Step A, not Step I.

- **Do not move the traveller during the weighing** — section 2.1. The heart's arc depends on
  him standing still.

- **Frame-rate independence.** Every new speed is units *per second*, multiplied by `dt`. A
  chase is exactly where a missing `dt` shows up as "impossible on my laptop, trivial in the
  lab".

- **`uPetrifyBaseY` already follows the traveller** — it reads `traveller->worldPosition().y`,
  so the petrification front tracks him as he moves. Nothing to change, but verify it: it is
  the one shader uniform that was written assuming a stationary body.

- **Medusa must not clip through the gate.** Clamp her to the corridor once `z > 11.5`, exactly
  as the player is clamped.

- **Add a debug key that teleports Medusa next to you.** Otherwise you will spend ten minutes
  running badly on purpose every time you want to check the losing ending.

---

## 10. Controls

| Key | Action |
|---|---|
| `W A S D` / arrows | Move the traveller |
| `SPACE` | Begin the trial (during `Waiting`) |
| `↑` / `↓` | Set the heart's weight (during `Waiting` only) |
| `1` / `2` | Force a balanced / cursed verdict |
| `R` | Restart the run |
| `C` | Free-fly camera (takes WASD back while active) |
| `F` | Toggle cinematic / chase camera |
| `G` `J` `U` `V` `Y` | Shadows, particles, post-processing, normal maps, textures |
| `H` `L` `N` `T` `B` | HUD, light markers, normals, wireframe, culling |
| `ESC` | Quit |

Debug keys worth adding: teleport Medusa to the player, skip straight to `Escape`, toggle
invulnerability.

---

## 11. Time budget

| Steps | Work | Estimate |
|---|---|---|
| A–B | Player movement and walk cycle | ~4 h |
| C–D | Treasure and corridor | ~4 h |
| E–F | States and pursuit | ~5 h |
| G | Collapse and falling stones | ~3 h |
| H–I | Endings, camera, HUD, tuning | ~5 h |

**≈ 21 hours** on top of the finished project — low *only* because the rendering, lighting,
animation and state-machine infrastructure is already built and verified.

---

## 12. What to say in the demo

The interactive version shows everything the original did, plus:

1. **Real-time input driving a hierarchical model** — the traveller is player-controlled and
   his limbs animate procedurally from his actual speed.
2. **Condition-driven state transitions** — the earlier version was entirely time-driven;
   `TreasureRevealed` and `Escape` end on player action.
3. **Collision detection** — 2D distance tests in XZ, deliberately chosen over a physics
   engine, using squared distances where the value is never displayed.
4. **Autonomous agent behaviour** — Medusa's distance-based pursuit with a speed ramp.
5. **Dynamic object pools** — falling stones recycle a fixed pool, never allocating mid-game,
   the same pattern as the particle system.
6. **Difficulty derived from narrative** — the verdict is not a dead-end branch; it modifies
   the chase.
7. **A third-person chase camera** that eases toward its target rather than locking to it.

Keep the old cinematic mode on a key. Being able to show the scripted version *and* the
playable one is worth more than either alone.

---

## 13. Medusa's gaze and cover  *(added after the first playable version)*

Until now Medusa caught the traveller only by touching him; her eyes were
lighting, not a weapon. The gaze makes them one.

### The rule

An attack runs **Cooldown -> Telegraph -> Lock -> Sweep**.

| Phase | Length | What the player sees | Can it hurt? |
|---|---|---|---|
| Telegraph | 1.1 s | Eyes brighten, a thin tracer follows him, then winds back toward one wall; HUD caps blink | no |
| Lock | 0.35 s | The aim freezes and the beam goes solid | yes |
| Sweep | 1.9 s | The beam crosses the whole corridor, +-38 degrees | yes |
| Cooldown | 1.8 s | Eyes smoulder | no |

Exposure fills while he is **inside the cone AND the eyes can actually see
him**. Being in the spotlight is not enough: three sight lines (centre and both
shoulders) are tested against the pillars and the chamber's front wall, so a
pillar edge gives *partial* exposure instead of flickering. Full bar = stone.
The bar drains over 3 s once he is clear, and stone slows him by up to 30%.

**Rubble is deliberately not cover** - it is too low to break the line. Only the
seven tall pillars are.

### Where it lives

| File | Role |
|---|---|
| `src/Gaze.h/.cpp` | the whole mechanic; no GL, so it is unit-tested |
| `src/EscapeTuning.h` | every number, including the pillar layout |
| `Scene` | pillars, the visible beam, the glowing eyes, ghosting pillars |
| `Hud` | the petrification bar and the blinking warning caps |

### What the simulation found (40 runs per bot, 60 Hz, real classes)

| Bot | Escapes |
|---|---|
| ignores the gaze | **0%** - stoned on the second attack |
| strafes sideways | **0%** - *worse* than standing still |
| tucks behind a pillar for the ~1 s the beam passes | **100%** |
| same, but 0.7 s late and misses 1 attack in 4 | **97.5%** |

Three things this taught, none of which were obvious in advance:

1. **Strafing cannot work.** A +-38 degree sweep covers an 8-wide corridor from
   any range over about 6 units, and moving *with* the sweep lengthens the time
   in the beam. Cover is the only counter, and the design now says so plainly.
2. **Hiding for the whole attack loses.** A bot that stood behind the pillar from
   the first telegraph to the end of the sweep (~3.4 s) was caught by Medusa
   100% of the time: she closes ~8 units while he waits. Hiding has to be timed
   to the ~1 s the beam is actually passing.
3. **The gaze did nothing at first.** At 0.95 s to fill, one sweep leaves the bar
   at ~65% and it drains before the next attack, so ignoring it cost nothing.
   0.75 s makes the first mistake survivable (~87%, and slowed) and the second
   fatal.

### Two problems only a screenshot could show

- A pillar that hides him from Medusa hides him from the **camera** too, because
  the chase camera sits on her side. Any pillar between the camera and him now
  fades to 25%.
- The camera could cut through a pillar, or sit with a wall torch filling a
  quarter of the screen. Pillars and torches are now camera obstacles.

### Verified, and not

Verified: the mechanic's rules (`test_gaze`), the slowdown (`test_exposure`), the
balance above (`test_gazerun`), and the real game end to end with the
self-playing build - once ignoring the gaze (stoned on the second attack), once
hiding (`visible=0.00` through all three attacks, `exposure=0.00`, escaped).

**Not** verified: how it *feels*. The sloppy bot is a proxy for a person, not a
person. In particular the chase camera looks forward while Medusa is behind it,
so a player cannot see the lane a pillar shades - they work it out from the beam
stopping on the pillar. Playtest that before trusting the 97.5%.

---

## 14. The chamber visibly collapses  *(added after the gaze)*

Before this, the only collapse was stones appearing under a ceiling that never
changed. Now the building itself breaks.

### What happens

Each ceiling section runs **Intact -> Cracking -> Shaking -> Detaching -> Falling -> Down**:

1. **Cracking** - dark cracks grow across its underside and front face; dust trickles out.
2. **Shaking** - it shudders in place; a low rumble goes through the camera.
3. **Detaching** - it tips on the edge it hangs from (a hinge rotation), and chunks break off.
4. **Falling** - it breaks free, keeps turning in the air, and slaps down onto its resting angle.
5. **Down** - a burst of dust, a camera jolt, and it becomes a solid obstacle.

| Where | Sections | When |
|---|---|---|
| Chamber | 3 blocks of a new cornice (a stone ledge round the top of the walls), either side of the doorway | 0, 0.12 and 0.24 s after the treasure is taken |
| Corridor | 3 ceiling beams that split in two; one half falls | when he is 19.5 units short of each beam, so it lands ~4 units ahead of him |

**Torches:** the chamber's two flicker hard while it shakes. In the corridor the
beam at 30.5 puts out the torch across from it, the beam at 44.5 smashes the
torch beside it, and the beam at 58.5 only makes its torch gutter. A dead
torch stops lighting: the light budget goes to the next lit one.

**Reuse:** dust is the particle system (a new `sprinkle` for dust falling *down*);
chunks are the debris pool (`dropChunks`); the shake feeds the same camera
shake as the stones.

### A route always stays open

- The chamber blocks land at least 1.7 units clear of the doorway.
- Every corridor half-beam leaves a 3.95-wide lane (Medusa needs 2.3).
- Fallen sections are registered as obstacles at start-up, switched off, and
  switched on as each lands - for Medusa as well as the traveller.

### Where it lives

| File | Role |
|---|---|
| `src/Collapse.h/.cpp` | stages, poses, events, footprints, torch levels; no GL, unit-tested. `standardLayout()` is the single list the scene, main and the tests all build from |
| `Scene::buildCeilingWork` / `applyCollapse` | cornice, beams, cracks, flames and torch lights |
| `WorldShape::avoidRubble` | steering round fallen blocks |
| `CameraDirector::setLookUp` | the chase camera tilting up to watch the chamber break |

### What testing found

- **The first beam layout broke the game.** Beams alternated sides, and two of them
  landed right behind the pillars the player hides behind from Medusa's gaze. The
  cover-using bot went from escaping every run to escaping none. All three now fall
  on the left, away from the pillar just before each one.
- **A long block met head-on stops anyone dead.** Sliding along a circle does nothing
  when you hit it square on. Medusa now steers round fallen blocks (only fallen
  blocks - the pillars behave as before, so the gaze balance is unchanged).
- **The chamber collapse was out of shot.** The chase camera looks down at the floor,
  and the top of the walls sat just above the frame. It now tilts up to watch the
  ceiling break while he is still in the chamber.
- **A frame of solid brown at the doorway** (an older bug): the camera rode at about
  5 high through a doorway only 5 high, passing through the lintel. It now stays
  below the lintel until it is through.

| Bot (40 runs each) | Collapse off | Collapse on |
|---|---|---|
| ignores the gaze | 0% escape | 0% |
| strafes | 0% | 0% |
| uses cover | 100% | 100% |
| uses cover badly | 97.5% | 85% |

So the collapse is a real hazard, but not an unfair one: it costs an imperfect player
a little, and never lands on anyone who keeps moving.

**Not verified:** how it feels to a person. The chamber blocks are seen cracking and
tipping at the top of the frame, but they land out of shot, because he is at the door
by then. Look back with the free camera (TAB) to see them lying there.
