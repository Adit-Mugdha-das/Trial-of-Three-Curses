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
| 4. Dawn | The sky turns orange then blue, the moon fades, the sun rises over the back wall, and the camera pulls up for the final shot. | done |

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

## Part 4 - what was built

- **Dawn** begins 8.5 s after the jewel lands (the souls mostly up) and takes 8 s.
- **The sky** (the clear colour) goes night purple -> sunrise orange -> morning blue.
- **The moon** sinks and fades; **the stars** go out; **clouds** fade in, lit pink-orange at
  sunrise and white by morning, drifting.
- **The sun** rises out of desert dunes on the horizon: an emissive sphere, deep orange
  turning gold-white, with a soft halo drawn as two big additive glows (a sphere for the
  halo had a hard edge). The garden's back wall was lowered to waist height so the garden
  looks out over the dunes.
- **The light warms:** the garden's cold moonlight becomes warm sunlight from the sun's
  side, the directional fill turns warm and stronger, and the ambient rises to daylight.
  The relic fireflies fade with the night.
- **Final shot:** across the living garden - flowers, palms, pool - to the low wall, the
  dunes and the sun rising over them. The run ends 15 s after the wave.
- **Fixed on the way:** the shader was told light #0 casts the shadow map, but in the garden
  the casting torch is skipped (Part 2), so another light was wrongly being shadowed. The
  scene now reports which light casts, or none.

## The whole ending, as built

Dead garden and stone statues -> lay the jewel on the ankh -> a golden circle of life
spreads; plants revive -> each statue glows, crumbles to gold dust, and its soul rises as a
firefly into the stars -> dawn: the sun rises over the dunes on a garden come back to life.

## The twist - the Ascension (built)

He was a soul too. Half a second after dawn has fully come, the run plays a 44 s finale
(`src/Ascension.h/.cpp`, GL-free, tested), then a new traveller begins. No controls; no HUD.

1. **He turns to light (0-5 s):** the charm cracks in a cyan burst; gold climbs his body
   from the feet (the petrify shader front, with a gold palette); he lifts off.
2. **Over the temple, through the clouds (5-14.5 s):** his body fades into a bright orb;
   the camera shows the whole temple from above, then he rises out of a sea of clouds
   (a cloud floor plus 70 puffs at y 76-86) into a golden sky.
3. **The river of souls (14.5-26 s):** a river of 150 lights flows to Ra's sun boat - a
   gold hull with papyrus ends, a cabin, the sun disc and eight rowing oars - sailing toward
   a sun above the clouds. He rides it.
4. **He becomes a star (26-44 s):** night falls and stars come out. Constellations of his
   journey are drawn one by one: the scale, the lamp, the three rings, Medusa's closed
   eye. Last, a figure of himself appears, and he rises to become its heart star.

Camera far plane is 900 during the finale, 200 otherwise.

## Ray-traced pool reflections (built)

The garden pool is a ray-traced mirror (`shaders/phong.frag`, `Scene::uploadRayTracing`).
Each frame the scene sends the garden's visible shapes to the shader: up to 48 boxes and
40 ellipsoids, each with its current colour (alive or dead, texture's mean colour). For
every water pixel the shader:

1. bends the normal with small moving ripples,
2. fires a **reflection ray** and finds the nearest box/ellipsoid it hits (slab test,
   ray-ellipsoid quadratic), or the sky if none,
3. shades that hit with the garden light and a **shadow ray** toward the light,
4. fires a second **shadow ray** from the water, so things standing between it and the
   light shade the pool,
5. mixes reflection over the water by a Fresnel term.

**Z** toggles it (the window title shows "ray tracing ON/OFF"). After dawn the camera
holds a low shot across the pool so the altar, the gold ankh and the orb are seen
reflected, before the ascension begins.

## The sun's ray off the pool (built)

What sets the ascension off. About 6 s after dawn completes, a ray leaves the sun, strikes
the pool and its reflection strikes his chest; a flash, and the gold begins to climb.

- The spot on the water is found with the **mirror law** (angle in = angle out): the ray
  aims at his mirror image under the surface; where that line crosses the water is the
  spot. The reflected ray is `glm::reflect` of the incoming one about the water's normal.
- Timing: sun -> water 1.0 s, water -> chest 0.6 s, the ascension begins 0.35 s after.
- The ray is drawn as glows over everything (`ParticleSystem::addGlowOnTop`), so the low
  wall and the altar never cut it into pieces. The risen sun sits a little higher (16)
  so that the reflection point falls inside the pool.
