# The Firefly Constellation Gate - plan

A new ending for the blessed path. The fireflies become clues, and three
rotating stone rings add visible motion and transformations.

## The full sequence

1. **The Djinn grants a protective charm** after a true judgment.
2. You take the treasure and escape the collapsing chamber (as now).
3. **At the final gate the charm raises a sanctuary**: a dome of light that holds
   Medusa and the falling stones back - for a limited time.
4. **Fireflies gather and briefly form a pattern** on the gate. You rotate three
   stone rings to recreate it.
5. **The gate opens into a small hidden garden**, where fireflies gather round
   ancient relics.

## Parts (build one, run it, check it, then the next)

| Part | What you will see | Status |
|---|---|---|
| 1. The charm | After a true verdict a glowing charm rises from the Djinn's column, flies to the traveller and circles him for the rest of the run. A cyan marker on the HUD. Only a balanced heart earns it. | done |
| 2. Gate + sanctuary | A stone gate with three rings closes the end of the corridor. Reaching it, the charm raises a dome: Medusa stops at its edge, the stones stop, a timer runs. If it runs out, she takes him. | done |
| 3. The puzzle | Fireflies fly to the gate and form the pattern for a few seconds (and again on request). Arrow keys select and rotate the rings; they turn smoothly about their centre. Matching all three solves it. | done |
| 4. The garden | The rings lock and glow, the gate opens, and he walks into a small walled garden: grass, plants, relics, fireflies gathered round them. A new ending shot. | done |

## Controls (at the gate)

| Key | Does |
|---|---|
| Up / Down | choose a ring (it glows gold) |
| Left / Right | turn it one step (60 degrees) |
| Space | ask the fireflies to show the pattern again (F was taken by the camera) |
| WASD | still walk; the arrows stop walking while you are at the gate |

## Part 2 - what was built

- New trial state **Sanctuary** between Escape and the ending. It starts when
  he passes z = 65 holding the charm. It lasts 35 s, then the dome falls and he
  is Caught. Until Part 3 the gate cannot be opened, so every run ends that way.
- **The gate:** a slab across the corridor at z = 71.5. On its face are three stone
  discs (outer, middle, inner). Each hangs from its own pivot at the shared
  centre, so turning a ring is one rotation about Z. Each ring has a gold notch
  and carved marks; six sockets sit round the outside for the fireflies.
- **The dome:** a transparent half-sphere (radius 5.5, so a camera inside can see the whole gate) with rune stones round its
  foot. It grows over 0.8 s and shoves Medusa back as it grows. It flares when
  she hits it and flickers in its last 6 s. The charm hangs at the top and
  becomes the dome's light, guarding the edge she presses on.
- **Inside it:** no stones, no gaze (the beam is suppressed), the petrification
  fades, and Medusa cannot catch him. The dome also keeps him in.
- **Camera:** first from the gate looking back at Medusa held at the dome, then
  turned to face the gate.
- **HUD:** the bottom bar turns cyan and drains with the time left, blinking at the end.
- Tested: `test_sanctuary` (11 checks) - including Medusa right on his heels when
  it rises - plus every earlier suite.

## Part 3 - what was built

- `src/GatePuzzle.h/.cpp` (no GL, 25 checks in `test_puzzle`): three rings in 60-degree
  steps, a random pattern each run (never 0, so every ring must turn; never all three
  the same), smooth turning, and a lock once all three match and stop moving.
- **The gate's fireflies** (17): released from the charm when the sanctuary rises,
  they hover round the six sockets. About 4.6 s in (once the camera faces the gate)
  they form the constellation by themselves: a star of three on each ring where its
  notch must point, and lines of fireflies joining the stars. They hold it 4 s, then
  scatter. Space asks again.
- **Feedback:** the selected ring's rim pulses gold and its notch brightens. When all
  three lock, every rim and notch turns cyan, the constellation flares, and the
  fireflies stay in the pattern.
- **Solved:** the dome fades and the run ends Escaped (Part 4 replaces this with the gate
  opening onto the garden).
- **The traveller fades to a ghost** while the camera looks past him at the gate, so
  he never hides a star - he stood right over the bottom of the rings.
- Checked in the real game with a self-playing build that watches the pattern and
  then turns the rings: solved in 6 presses, the rings locked, and it escaped.

## Part 4 - what was built

- New trial state **Garden** (Sanctuary -> Garden when the rings lock; 30 s, then a new
  run, or R sooner). Solving no longer uses the old Escaped ending.
- **The gate opens:** 0.6 s after the lock, the whole gate sinks into the floor over
  2.6 s. The rings spin a last turn in opposite directions, dust bursts along its foot,
  and the camera rumbles. The dome fades. Once it is low enough to step over, the end
  wall is removed and the garden is open.
- **Medusa retreats** down the corridor, away from the light.
- **The garden** (`Scene::buildGarden`, open to the night sky): lawn, sandstone walls with
  ivy, stepping stones, four curved palm trees, bushes, about 30 flowers, a pool with
  lotus flowers, a moon and stars.
- **Three relics:** a golden ankh turning slowly on a stepped pedestal, a crystal orb
  that breathes light on a column, and an obelisk with a gold cap. 22 fireflies gather
  round each. One of them carries the roaming firefly light, and the old exit light
  becomes the garden's moonlight.
- **Walking:** `WorldShape` knows the garden - wider than the corridor, entered through
  the corridor's end, with a front wall either side. The relics, pool and palms are solid.
- **Camera:** watches the gate sink, follows him in, then pulls back high over the garden
  for the last ten seconds.
- Tested: `test_garden` (10 checks) plus all 8 earlier suites. Checked visually by frames
  saved from inside the game (`TRIAL_SHOT_DIR`), because the desktop screen capture had
  started returning plain white for the game window.

## The whole sequence, as built

1. True heart -> the Djinn's lamp opens -> the **charm** flies to him.
2. Take the treasure -> the chamber cracks and falls -> run, hiding from Medusa's gaze.
3. Near the end -> the charm raises the **sanctuary** (35 s) and holds Medusa back.
4. The fireflies show the **constellation** -> turn the three rings to match.
5. The gate sinks -> the **hidden garden**, with fireflies round the relics.
