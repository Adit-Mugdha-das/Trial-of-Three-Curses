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
