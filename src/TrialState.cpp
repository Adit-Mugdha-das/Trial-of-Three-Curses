#include "TrialState.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "Easing.h"
#include "EscapeTuning.h"

namespace
{
    // Anything closer to 0.5 than this passes judgement.
    constexpr float kBalanceTolerance = 0.12f;

    // Maximum beam tilt in degrees, at either extreme of weight.
    constexpr float kMaxTilt = 20.0f;
}

float TrialController::durationOf(TrialState state)
{
    switch (state)
    {
        // Three states have no duration at all: they end when the player
        // does something. That is the real structural change here - until
        // now every transition was on a timer.
        case TrialState::Waiting:          return 0.0f;
        case TrialState::TreasureRevealed: return 0.0f;
        case TrialState::Escape:           return 0.0f;
        case TrialState::Sanctuary:        return Tuning::kSanctuaryDuration;
        case TrialState::Garden:           return 0.0f;   // waits for the offering; main ends it

        case TrialState::Placing:          return 2.0f;
        case TrialState::Weighing:         return 3.0f;

        // Shortened from 6 and 8: these are now a verdict beat on the way to
        // the treasure, not the end of the story.
        // Long enough for the lamp to open and the magic to rise, because
        // that is what produces the treasure.
        case TrialState::Balanced:         return 5.6f;

        // She wakes and turns. The petrification itself is the Caught state.
        case TrialState::Cursed:           return 3.5f;

        case TrialState::Escaped:          return 4.0f;

        // Longer than the others: turning to stone should be watched.
        case TrialState::Caught:           return 5.5f;
        case TrialState::Reset:            return 2.5f;
    }
    return 0.0f;
}

const char* TrialController::stateName() const
{
    switch (m_state)
    {
        case TrialState::Waiting:          return "Waiting";
        case TrialState::Placing:          return "Placing";
        case TrialState::Weighing:         return "Weighing";
        case TrialState::Balanced:         return "Balanced";
        case TrialState::Cursed:           return "Cursed";
        case TrialState::TreasureRevealed: return "Treasure";
        case TrialState::Escape:           return "Escape";
        case TrialState::Sanctuary:        return "SANCTUARY";
        case TrialState::Garden:           return "GARDEN";
        case TrialState::Escaped:          return "ESCAPED";
        case TrialState::Caught:           return "CAUGHT";
        case TrialState::Reset:            return "Reset";
    }
    return "?";
}

float TrialController::progress() const
{
    const float duration = durationOf(m_state);
    if (duration <= 0.0f)
    {
        return 0.0f;
    }

    return Easing::clamp01(m_stateTime / duration);
}

bool TrialController::wouldBalance() const
{
    return std::fabs(m_heartWeight - 0.5f) < kBalanceTolerance;
}

float TrialController::verdictAngle() const
{
    // Weight 0.5 -> level. Either extreme -> full tilt.
    const float offset = std::clamp((m_heartWeight - 0.5f) * 2.0f, -1.0f, 1.0f);
    return offset * kMaxTilt;
}

float TrialController::petrification() const
{
    switch (m_state)
    {
        case TrialState::Caught:
            // The gaze lands, then the stone climbs. Cursed no longer
            // petrifies anyone - it is only the moment she wakes.
            return Easing::smoothstep01((progress() - 0.12f) / 0.68f);

        case TrialState::Reset:
            // Only unwind it if he was actually caught.
            return m_wasCaught ? (1.0f - Easing::smoothstep01(progress())) : 0.0f;

        default:
            return 0.0f;
    }
}

bool TrialController::playerHasControl() const
{
    switch (m_state)
    {
        // Free to wander before committing, and again once the treasure is
        // on offer. Never during the weighing: the flight arc of the heart is
        // measured from wherever he is standing.
        case TrialState::Waiting:
        case TrialState::TreasureRevealed:
        case TrialState::Escape:
        case TrialState::Sanctuary:     // free to move about inside the dome
        case TrialState::Garden:        // and to wander the garden
            return true;

        default:
            return false;
    }
}

float TrialController::charm() const
{
    if (!m_balanced)
    {
        return 0.0f;
    }

    switch (m_state)
    {
        case TrialState::Balanced:
            // Only once the Djinn has fully formed (by about 2.9 s) and has
            // reached his hand out to the traveller (3.0 - 3.6 s): then it
            // leaves his hand and flies over, arriving at 5.2 s.
            return Easing::smoothstep01(Easing::clamp01((m_stateTime - 3.7f) / 1.5f));

        case TrialState::TreasureRevealed:
        case TrialState::Escape:
        case TrialState::Sanctuary:
        case TrialState::Garden:
        case TrialState::Escaped:
        case TrialState::Caught:
            return 1.0f;

        default:
            return 0.0f;
    }
}

float TrialController::treasureReveal() const
{
    switch (m_state)
    {
        case TrialState::TreasureRevealed:
            // Rises over the first moment and a half of the state, then waits
            // however long the player takes to walk over.
            return Easing::smoothstep01(m_stateTime / 1.5f);

        case TrialState::Escape:
        case TrialState::Sanctuary:
        case TrialState::Garden:
        case TrialState::Escaped:
        case TrialState::Caught:
            // Only a true heart was ever offered one. Without this test a
            // cursed traveller watches a treasure rise out of the floor
            // while he is being turned to stone.
            return m_balanced ? 1.0f : 0.0f;

        case TrialState::Reset:
            return m_balanced ? (1.0f - Easing::smoothstep01(progress())) : 0.0f;

        default:
            return 0.0f;
    }
}

bool TrialController::trialIsOver() const
{
    return m_state == TrialState::TreasureRevealed
        || m_state == TrialState::Escape
        || m_state == TrialState::Sanctuary
        || m_state == TrialState::Garden
        || m_state == TrialState::Escaped
        || m_state == TrialState::Caught;
}

void TrialController::treasureCollected()
{
    if (m_state == TrialState::TreasureRevealed)
    {
        enter(TrialState::Escape);
    }
}

void TrialController::reachedExit()
{
    if (m_state == TrialState::Escape)
    {
        enter(TrialState::Escaped);
    }
}

void TrialController::reachedSanctuary()
{
    // Only the charm can raise it, and only a true heart was given one.
    if (m_state == TrialState::Escape && hasCharm())
    {
        enter(TrialState::Sanctuary);
    }
}

void TrialController::gateSolved()
{
    if (m_state == TrialState::Sanctuary)
    {
        enter(TrialState::Garden);
    }
}

void TrialController::caught()
{
    if (m_state == TrialState::Escape)
    {
        m_wasCaught = true;
        enter(TrialState::Caught);
    }
}

void TrialController::enter(TrialState next)
{
    m_state     = next;
    m_stateTime = 0.0f;

    std::cout << "[Trial] -> " << stateName();

    if (next == TrialState::Weighing)
    {
        std::cout << "  (weight " << m_heartWeight
                  << ", verdict angle " << verdictAngle() << " deg)";
    }
    else if (next == TrialState::Balanced)
    {
        std::cout << "  the heart is true; the lamp opens";
    }
    else if (next == TrialState::Cursed)
    {
        std::cout << "  the heart is false; the guardian turns her gaze";
    }
    else if (next == TrialState::TreasureRevealed)
    {
        std::cout << "  the Djinn's magic yields a treasure - take it if you dare";
    }
    else if (next == TrialState::Escape)
    {
        std::cout << "  the chamber begins to fall; RUN";
    }
    else if (next == TrialState::Sanctuary)
    {
        std::cout << "  the charm raises a sanctuary; she cannot cross it - yet";
    }
    else if (next == TrialState::Garden)
    {
        std::cout << "  the rings align; the gate opens onto a hidden garden";
    }
    else if (next == TrialState::Escaped)
    {
        std::cout << "  out alive";
    }
    else if (next == TrialState::Caught)
    {
        std::cout << "  her gaze lands; the stone takes hold";
    }

    std::cout << std::endl;
}

void TrialController::begin()
{
    if (m_state != TrialState::Waiting)
    {
        return;
    }

    // The verdict is decided the moment the heart is committed, not when the
    // beam finishes moving. Weighing then animates toward a known outcome.
    m_balanced  = wouldBalance();
    m_wasCaught = false;

    enter(TrialState::Placing);
}

void TrialController::reset()
{
    enter(TrialState::Reset);
}

void TrialController::adjustWeight(float delta)
{
    if (m_state != TrialState::Waiting)
    {
        return;
    }

    m_heartWeight = Easing::clamp01(m_heartWeight + delta);
}

void TrialController::forceOutcome(bool wantBalanced)
{
    if (m_state != TrialState::Waiting)
    {
        return;
    }

    m_heartWeight = wantBalanced ? 0.5f : 0.88f;
    begin();
}

void TrialController::update(float deltaTime)
{
    m_stateTime += deltaTime;

    const float duration = durationOf(m_state);

    // Waiting has no duration and only leaves on input.
    if (duration <= 0.0f || m_stateTime < duration)
    {
        return;
    }

    switch (m_state)
    {
        case TrialState::Placing:
            enter(TrialState::Weighing);
            break;

        case TrialState::Weighing:
            // The branch. It no longer ends the story - it decides how hard
            // the escape is going to be.
            enter(m_balanced ? TrialState::Balanced : TrialState::Cursed);
            break;

        case TrialState::Balanced:
            // The Djinn's magic yields the treasure. Only a true heart ever
            // gets this far.
            enter(TrialState::TreasureRevealed);
            break;

        case TrialState::Cursed:
            // A false heart is not offered anything. Her gaze finishes it.
            m_wasCaught = true;
            enter(TrialState::Caught);
            break;

        case TrialState::Sanctuary:
            // The charm is spent and the dome falls. She is waiting.
            m_wasCaught = true;
            enter(TrialState::Caught);
            break;

        case TrialState::Garden:
        case TrialState::Escaped:
        case TrialState::Caught:
            enter(TrialState::Reset);
            break;

        case TrialState::Reset:
            enter(TrialState::Waiting);
            break;

        // No duration, so control never reaches here: these leave only
        // through treasureCollected(), reachedExit() or caught().
        case TrialState::Waiting:
        case TrialState::TreasureRevealed:
        case TrialState::Escape:
            break;
    }
}
