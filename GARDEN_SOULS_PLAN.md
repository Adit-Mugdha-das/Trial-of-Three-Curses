# The Garden of Lost Souls - plan

**The story:** the traveller steps into a dead, grey garden full of stone statues -
travellers Medusa caught long ago. He places the treasure on the ankh altar. A wave of
light and colour spreads across the garden; the plants come back to life, and every
statue the wave touches turns back from stone and rises as a firefly soul into the sky.
The fireflies were the souls all along. Then the sun rises.

## Parts (build one, run it, check it, then the next)

| Part | What you will see | Status |
|---|---|---|
| 1. The dead garden | Dry brown lawn, palm fronds hanging limp, bushes shrunk, closed grey flowers, a black still pool, closed lotus buds, withered ivy, dull relics, faint fireflies - and **7 stone statues** of past travellers frozen in different poses. | done |
| 2. The wave of life | Walk up to the ankh: the treasure floats onto it and a glowing ring spreads across the ground. Behind it the colour returns: green grass, fronds lifting, flowers opening, the pool glowing. | done |
| 3. The souls are freed | When the wave reaches a statue it glows gold, crumbles into light, and its soul rises as a firefly into the stars. | done |
| 4. Dawn | The sky turns orange then blue, the moon fades, the sun rises over the back wall, and the camera pulls up for the final shot. | next |

## Part 1 - what was built

- **Colour that changes in the shader.** A `Material` can be *living*: it has a dead
  colour as well as its living one. `phong.frag` receives the wave of life (a centre and a
  radius) and decides per pixel: dead outside the circle, alive inside, with a gold glow
  along its edge. So one big lawn can change colour as a smooth spreading circle. Until
  Part 2 the radius is 0, so everything shows dead.
- **Shape that changes per object** (`Scene::m_living`): each plant has a dead and a living
  position, scale and rotation. Palm fronds hang on hinge pivots at the trunk top (dead:
  hanging down at 155 degrees; alive: spread at 105). Flowers are short closed buds when
  dead, bushes shrunk, ivy shrivelled up the wall, lotus flowers closed.
- **Seven stone statues**, built like the traveller (legs from the hip, torso, arms from the
  shoulder), posed by joint rotations: reaching for the altar, shielding his eyes, kneeling
  in prayer, running, fallen to one knee looking back, turning from the light, and a child
  reaching for the pool. Each stands on a plinth and is solid to walk into.
- The ankh does not turn and the relics do not glow until life reaches them; the relic
  fireflies are faint.

## Part 2 - what was built

- **The offering:** walk within 2.3 of the ankh altar and the treasure's jewel floats out of
  his hands in an arc (1.3 s) and settles, turning and breathing, inside the ankh's loop. It
  carries a warm gold light.
- **The wave of life:** a circle spreading from the altar over 6.5 s to radius 15 (fast at
  first, slowing at the walls). The shader turns everything inside it from dead to alive per
  pixel, with a gold glow along its edge, and gold sparks are thrown up all along that edge.
  As it passes, each plant's shape changes: fronds swing up on their hinges, bushes fill out,
  ivy grows down the walls, flowers grow tall and open, lotus flowers open. The pool glows
  blue, the ankh starts turning, the orb lights, and the relic fireflies brighten.
- **Camera:** rises to look down on the altar so the circle can be watched spreading, then
  pulls back over the living garden.
- **The garden now waits** for the offering instead of ending after 30 s; the run ends 6 s
  after the wave finishes (R still leaves sooner).
- In the garden the chamber's and corridor's torches stop using the 12-light budget; the
  offering's light uses it instead.

## Part 3 - what was built

- When the wave of life reaches a statue (`Scene::m_statues`), it **glows gold from within**
  for 1 s (emissive ramps up, the stone warms toward gold, and it casts real light), then
  **crumbles into light** over 0.8 s: a burst of gold dust and the stone body fading away
  (see-through parts move to the sorted transparent pass by themselves). Only its empty
  plinth is left, as a memorial.
- **Its soul rises**: a big firefly (bright core, wide halo) from the statue's chest, in a
  slow widening spiral, gathering speed, trailing gold sparks, fading into the stars at
  height 24. The three lowest glowing statues or souls light the garden as they go.
- Statues nearest the altar wake first, so the souls go up as a cascade.
- **Camera:** after looking down on the wave, it drops low and looks up past the empty
  plinths and the palms at the souls rising into the night sky.
- A new run puts every statue back, whole and grey.
- The run now lingers 10 s after the wave so the souls can rise (R leaves sooner).
