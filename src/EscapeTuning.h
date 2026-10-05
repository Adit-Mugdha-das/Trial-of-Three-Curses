#ifndef ESCAPETUNING_H
#define ESCAPETUNING_H

// Every number that decides how the escape *feels*, in one place.
//
// Balancing a chase means changing these over and over, so they are kept out
// of the logic entirely. If you find yourself editing a speed anywhere else,
// it belongs here instead.
namespace Tuning
{
    // --- player ---------------------------------------------------------
    constexpr float kPlayerSpeed    = 5.6f;    // units per second
    constexpr float kPlayerAccel    = 26.0f;   // units per second squared
    constexpr float kPlayerTurnRate = 720.0f;  // degrees per second
    constexpr float kPlayerRadius   = 0.45f;   // for wall and stone collision

    // --- pursuer (Step F) ------------------------------------------------
    // She uncoils and locks on before she moves. Without this beat she is
    // simply moving the instant the treasure leaves its pedestal, which
    // reads as a switch being thrown rather than as a creature reacting.
    constexpr float kMedusaStartDelay  = 1.8f;

    // Set by hand rather than by simulation. Combined with the 1.8 s
    // wind-up these make the escape forgiving - see kStoneInterval below if
    // it wants tightening again.
    constexpr float kMedusaSpeedStart = 5.10f;
    // Above the player's 5.6, so she closes over the run. Found by
    // simulation: one stone hit stays survivable, two are fatal.
    constexpr float kMedusaSpeedEnd   = 6.05f;
    constexpr float kMedusaRampTime   = 9.00f;
    constexpr float kCatchRadius      = 1.6f;

    // --- the collapse: ceiling sections breaking away ------------------------
    // Each one cracks (dust trickles out), shudders, tips about the edge it
    // hangs from, then falls and stays down as an obstacle. The chamber's go
    // quickly - he is only in there for about two seconds - and the
    // corridor's are timed off HIS position, so they come down ahead of him
    // where the chase camera can see them.
    constexpr float kChamberCrackTime    = 0.30f;
    constexpr float kChamberShakeTime    = 0.25f;
    constexpr float kCorridorCrackTime   = 1.10f;
    constexpr float kCorridorShakeTime   = 0.60f;
    constexpr float kSectionDetachTime   = 0.30f;   // tipping on its hinge edge
    constexpr float kSectionDetachAngle  = 24.0f;   // degrees before it breaks free
    constexpr float kSectionGravity      = 16.0f;   // same as the stones
    constexpr float kSectionTriggerAhead = 19.5f;   // corridor: starts this far ahead

    // --- Medusa's gaze -----------------------------------------------------
    // An attack cycle is Cooldown -> Telegraph -> Lock -> Sweep. Only the
    // last two can petrify; the telegraph exists so that nothing ever hurts
    // without having been seen coming.
    constexpr float kGazeFirstDelay = 0.9f;    // after both are in the corridor
    constexpr float kGazeCooldown   = 1.80f;
    constexpr float kGazeTelegraph  = 1.1f;    // eyes brighten, thin tracer follows him
    constexpr float kGazeLock       = 0.35f;   // aim freezes, beam goes solid
    constexpr float kGazeSweep      = 1.9f;    // the beam crosses the corridor
    constexpr float kGazeSweepArc   = 38.0f;   // HALF the arc, in degrees
    constexpr float kGazeConeHalf   = 10.0f;   // half-angle of the beam, degrees
    constexpr float kGazeRange      = 28.0f;
    constexpr float kGazeTargetHeight = 1.5f;  // where on him the beam aims
    constexpr float kGazeTrackRate  = 160.0f;  // degrees/s while she is not locked

    // Seconds of full, unobstructed exposure that turn him to stone, and how
    // long a full bar takes to drain again once the beam is off him.
    //
    // Chosen by simulating bots that ignore the gaze, strafe, use cover and
    // use cover badly. The result is a cliff, not a slope: ignoring it or
    // strafing is fatal on the second attack (a strafe is actually WORSE than
    // standing still, because moving with the sweep lengthens the time in the
    // beam), while hiding for just the ~1 s the beam passes almost always
    // works. At 0.75 a single mistake leaves him alive at ~87% of the bar and
    // slowed; the second kills.
    constexpr float kGazeFillTime   = 0.75f;
    constexpr float kGazeDecayTime  = 3.0f;

    // Stone is heavy: at full exposure he moves this much slower. Without it
    // the stone creeping up his legs would be purely cosmetic.
    constexpr float kExposureSlowdown = 0.30f;

    // --- cover pillars -----------------------------------------------------
    // Tall enough to block the eye-to-chest line, and solid. Rubble is
    // deliberately NOT cover: it is too low to break the line of sight.
    //
    // They alternate sides of the corridor centre, so a straight run down the
    // middle never touches one - using cover is a decision, not an accident.
    constexpr int   kPillarCount       = 7;
    constexpr float kPillarShaftRadius = 0.80f;   // what blocks the gaze
    constexpr float kPillarBaseRadius  = 1.00f;   // what blocks the feet
    constexpr float kPillarHeight      = 7.10f;   // runs up into the ceiling
    constexpr float kPillarX[kPillarCount] = { -1.8f,  1.8f, -1.8f,  1.8f, -1.8f,  1.8f, -1.8f };
    constexpr float kPillarZ[kPillarCount] = { 20.0f, 27.0f, 34.0f, 41.0f, 48.0f, 55.0f, 62.0f };

    // --- handicap from the verdict (Step F) ------------------------------
    constexpr float kBlessedSpeedBonus = -0.4f;
    constexpr float kBlessedHeadStart  =  5.00f;

    // --- falling stones (Step G) -----------------------------------------
    constexpr int   kMaxStones       = 24;
    // Chosen by simulating 20 full runs per candidate. At 0.55 the corridor
    // becomes an obstacle course and even a perfect dodger loses 80% of
    // the time; at 1.10 nothing threatens anyone. At 0.85 a careless run
    // survives 35% of the time and a careful one always does, which is
    // the gradient the warning rings are there to create.
    constexpr float kStoneInterval   = 0.85f;
    // A stone falls for ~0.88 s, in which a running player covers ~4.93
    // units. The spawn band has to STRADDLE that value, or a stone can
    // never be overhead when it lands and the hazard is decorative. The
    // original 8-22 band made a hit literally impossible.
    constexpr float kStoneAheadMin   = 3.5f;
    constexpr float kStoneAheadMax   = 11.0f;
    constexpr float kStoneGravity    = 16.0f;
    constexpr float kStoneRadius     = 0.7f;
    // How long settled rubble blocks the corridor before crumbling. This
    // matters more than the stun does: landed stones are roadblocks, and
    // too many at once turn the corridor into an obstacle course.
    constexpr float kStoneRestDuration = 2.2f;

    constexpr float kStunDuration    = 0.75f;
    constexpr float kStunSpeedFactor = 0.50f;

    // --- treasure (Step C) -----------------------------------------------
    constexpr float kPickupRadius = 1.3f;

    // --- world bounds ----------------------------------------------------
    // The chamber floor spans x [-13, 13] and z [-12, 12]; the walls are 0.6
    // thick, so their inner faces sit at +-12.7 and z = -10.7. These already
    // have the player's radius taken off.
    constexpr float kRoomMinX = -12.25f;
    constexpr float kRoomMaxX =  12.25f;
    // Stops short of the Anubis statue's dais, which fills the back of the
    // chamber from about z = -6.5 rearwards.
    constexpr float kRoomMinZ = -5.20f;
    constexpr float kRoomMaxZ =  11.50f;   // becomes the gate in Step D

    // --- corridor (Step D) -----------------------------------------------
    constexpr float kGateZ      = 11.5f;
    constexpr float kCorridorHalfWidth = 4.0f;
    constexpr float kExitZ      = 70.0f;
}

#endif // ESCAPETUNING_H
