#include "CameraDirector.h"

#include <cmath>

#include "Easing.h"
#include "EscapeTuning.h"

namespace
{
    float easeAngle(float current, float target, float t)
    {
        return current + Easing::shortestAngleDelta(current, target) * t;
    }
}

CameraDirector::Shot CameraDirector::shotFor(TrialState state) const
{
    switch (state)
    {
        case TrialState::Waiting:
            // Wide establishing shot, from above the doorway looking back
            // into the chamber. The distance and pitch are chosen so the
            // camera lands INSIDE the room: before Step D added a front wall
            // this shot sat at z = 17.8, six units outside it, and the view
            // opened up buried in stone.
            return { { 0.0f, 2.6f, -2.0f }, 15.0f, 85.0f, 32.0f };

        case TrialState::Placing:
            // Pull in on the scale as the heart travels to it.
            return { { -0.6f, 3.4f, 0.6f }, 11.0f, 74.0f, 18.0f };

        case TrialState::Weighing:
            // Close on the beam - this is the moment the story turns.
            return { { -1.4f, 3.9f, -1.2f }, 9.0f, 82.0f, 8.0f };

        case TrialState::Balanced:
            // Swing left to the lamp and tilt up to follow the energy column.
            return { { -5.4f, 3.4f, 1.0f }, 12.5f, 30.0f, 14.0f };

        case TrialState::Cursed:
            // Frame Medusa and the traveller together, so the gaze reads as
            // travelling between them.
            // From the right-front corner of the CHAMBER. The old framing put
            // the camera at z = 14.6, which is past the front wall and
            // therefore inside the corridor, looking back through stone.
            return { { 3.0f, 2.4f, 1.5f }, 11.0f, 55.0f, 20.0f };

        case TrialState::TreasureRevealed:
            // First, while the Djinn points: him and the rising treasure in
            // one frame, from the front of the chamber.
            if (m_stateTime < 4.0f)
            {
                return { { -3.0f, 3.9f, 1.2f }, 9.5f, 70.0f, 8.0f };
            }
            // Push in on the treasure, then keep the traveller in frame as
            // he walks to it.
            return { { m_follow.x * 0.35f, 2.4f, m_follow.z * 0.35f + 0.5f },
                     11.0f, 100.0f, 28.0f };

        case TrialState::Escape:
        {
            // Chase cam: behind him and looking slightly ahead, so the
            // corridor and whatever is falling in it are both visible.
            // yaw 270 puts the camera on the -Z side of its target, which is
            // behind a traveller running toward +Z.
            Shot chase;

            // Medusa closes to within 1.6 units of the traveller, while the
            // camera sits several units further back again - so as she
            // catches up she ends up directly between the two, filling the
            // screen at exactly the moment the corridor ahead matters most.
            //
            // The answer is to climb and tilt down as she closes, looking
            // OVER her at the floor ahead. She stays visible low in frame,
            // the warning rings stay readable, and nothing has to be hidden.
            const float d = m_danger;

            // Lead further ahead the faster he moves - but less so when she
            // is close, or the camera slides forward past him.
            chase.target = m_follow + glm::vec3(
                0.0f,
                1.80f - 0.53f * d,
                3.0f + 2.2f * m_followSpeed * (1.0f - 0.6f * d));

            // Pulling in as she closes keeps the climb under the corridor
            // ceiling at y = 7.2.
            chase.distance = 9.0f + 1.8f * m_followSpeed - 2.8f * d;
            chase.yaw      = 270.0f;
            chase.pitch    = 18.0f + 20.0f * d;

            // Tilting up for the falling cornice: looking up 10 degrees from
            // about head height, so the top of the front wall fills the upper
            // part of the frame and he runs along the bottom of it.
            chase.pitch    += (-10.0f - chase.pitch) * m_lookUp;
            chase.target.y += 2.6f * m_lookUp;
            return chase;
        }

        case TrialState::Sanctuary:
            // First from in front of the gate, looking back down the corridor:
            // the dome going up and Medusa thrown back against it. Then round
            // to face the gate itself, where the puzzle will be.
            if (m_stateTime < 4.5f)
            {
                return { { 0.0f, 2.6f, Tuning::kSanctuaryCentreZ - 5.0f }, 6.0f, 90.0f, 10.0f };
            }
            // Far enough back for the whole gate - rings and sockets - and
            // high enough to look over his head at it.
            return { { 0.0f, 3.3f, Tuning::kGateSlabZ - 0.7f }, 8.7f, 270.0f, 14.0f };

        case TrialState::Garden:
            // Watch the gate sink, from where the puzzle was seen...
            if (m_stateTime < 3.6f)
            {
                return { { 0.0f, 3.3f, Tuning::kGateSlabZ - 0.7f }, 8.7f, 270.0f, 14.0f };
            }
            // ...follow him in until he makes the offering...
            if (m_offerTime < 0.0f)
            {
                return { m_follow + glm::vec3(0.0f, 1.6f, 2.0f), 7.5f, 270.0f, 18.0f };
            }
            // ...rise to look down on the altar as the wave of life spreads...
            if (m_offerTime < Tuning::kOfferFlight + 4.2f)
            {
                return { { 0.0f, 1.0f, Tuning::kGardenFrontZ + 7.5f }, 9.0f, 270.0f, 45.0f };
            }
            // ...then low, looking up past the empty plinths at the souls
            // rising into the stars...
            if (m_offerTime < Tuning::kOfferFlight + 9.0f)
            {
                return { { 0.0f, 5.5f, Tuning::kGardenFrontZ + 9.0f }, 10.0f, 270.0f, -12.0f };
            }
            // ...then across the living garden to the sun rising over the
            // back wall: garden below, the low wall and the dunes across the
            // middle, the sky and the rising sun above...
            if (m_offerTime < Tuning::kOfferFlight + Tuning::kDawnStart + Tuning::kDawnTime - 1.0f)
            {
                return { { 0.0f, 4.0f, Tuning::kGardenBackZ - 4.0f }, 11.0f, 270.0f, 15.0f };
            }
            // ...and last, low across the pool, so the sun, the palms and the
            // wall shine in its ray-traced reflection.
            {
                const glm::vec3 eye(1.6f, 1.5f, Tuning::kGardenBackZ - 0.9f);
                const glm::vec3 look(-0.3f, 0.6f, Tuning::kGardenBackZ - 7.5f);
                const glm::vec3 d = eye - look;
                const float distance = glm::length(d);
                return { look, distance, glm::degrees(std::atan2(d.z, d.x)),
                         glm::degrees(std::asin(d.y / distance)) };
            }

        case TrialState::Escaped:
            return { m_follow + glm::vec3(0.0f, 1.6f, 0.0f), 9.0f, 250.0f, 14.0f };

        case TrialState::Caught:
        {
            // Swing round in front of him to watch the stone take hold.
            //
            // The angle has to come from his heading rather than being fixed:
            // he can be caught anywhere, facing anywhere. A fixed yaw of 75
            // put the camera at z = 12.16 when he was petrified at his
            // starting spot - two thirds of a unit inside the front wall.
            //
            // A camera in front of a subject facing `h` sits along
            // (sin h, 0, cos h), and orientationVector() lays that out as
            // (cos yaw, 0, sin yaw), so yaw = 90 - h.
            Shot look;
            look.target   = m_follow + glm::vec3(0.0f, 1.7f, 0.0f);
            look.distance = 6.5f;
            look.yaw      = 90.0f - m_followHeading;
            look.pitch    = 10.0f;

            // Caught at the gate, facing it: in front of his face is behind
            // the gate. Watch from behind and to one side instead, with the
            // gate he could not open in the background.
            if (m_follow.z > Tuning::kSanctuaryTriggerZ - 1.0f)
            {
                look.distance = 5.5f;
                look.yaw      = 235.0f;
                look.pitch    = 14.0f;
            }
            return look;
        }

        case TrialState::Reset:
            // Back out to the establishing angle, and like it, kept inside.
            return { { 0.0f, 2.6f, -2.0f }, 15.0f, 85.0f, 36.0f };
    }

    return { { 0.0f, 2.8f, 0.0f }, 20.0f, 60.0f, 16.0f };
}

void CameraDirector::update(Camera& camera, TrialState state,
                            float deltaTime, float time)
{
    if (!m_enabled || camera.mode() != CameraMode::Orbit)
    {
        return;
    }

    Shot shot = shotFor(state);

    // A slow drift while nothing is happening, so the establishing shot is
    // never completely static.
    if (state == TrialState::Waiting)
    {
        shot.yaw += 9.0f * std::sin(time * 0.09f);
    }

    // Frame-rate independent exponential ease. Deliberately slow for the
    // cinematic shots; the chase has to keep up with a running player, so it
    // tracks far harder.
    const float rate = (state == TrialState::Escape)    ? 6.0f
                     : (state == TrialState::Sanctuary) ? 3.0f   // two shots in a few seconds
                                                        : 1.6f;
    const float t = std::min(1.0f, deltaTime * rate);

    camera.setTarget(camera.target() + (shot.target - camera.target()) * t);
    camera.setDistance(camera.distance() +
                       (shot.distance - camera.distance()) * t);
    camera.setYaw(easeAngle(camera.yaw(), shot.yaw, t));
    camera.setPitch(easeAngle(camera.pitch(), shot.pitch, t));
}
