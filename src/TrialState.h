#ifndef TRIALSTATE_H
#define TRIALSTATE_H

// The trial's state machine.
//
//   WAITING -> PLACING -> WEIGHING -> the verdict forks:
//
//     BALANCED -> the Djinn lamp opens and its magic yields a TREASURE
//                 -> ESCAPE -> ESCAPED | CAUGHT
//
//     CURSED   -> the guardian turns her gaze on him -> CAUGHT
//                 (no treasure, no escape - the trial simply ends)
//
//   ... -> RESET -> WAITING
//
// Only a heart that passes judgement is ever offered the treasure. That is
// the whole point of the weighing.
//
// TREASURE and ESCAPE have no duration: they end on a player action.
//
// Nothing here is precomputed: every state exposes a normalised progress value
// and the animation phases derive their motion from it and from elapsed time.
enum class TrialState
{
    Waiting,          // idle; the traveller has not committed yet
    Placing,          // the heart travels onto the left pan
    Weighing,         // the beam settles to the verdict angle
    Balanced,         // blessing: the Djinn lamp opens
    Cursed,           // curse: the guardian stirs
    TreasureRevealed, // the treasure rises; the player takes control
    Escape,           // the chamber collapses and Medusa gives chase
    Sanctuary,        // at the gate: the charm's dome holds her back, for a while
    Garden,           // the rings aligned: the gate opens onto a hidden garden
    Escaped,          // out alive
    Caught,           // petrified
    Reset             // everything eases back to rest
};

class TrialController
{
public:
    void update(float deltaTime);

    // --- input ---
    void begin();                       // SPACE: commit the heart
    void reset();                       // R: abandon and return to Waiting
    void adjustWeight(float delta);     // Up/Down: only meaningful while Waiting
    void forceOutcome(bool wantBalanced); // 1 / 2: jump straight to an outcome

    // --- events ----------------------------------------------------------
    // The first two states below end on a player action rather than a timer,
    // which is the structural change the interactive version brings. main
    // detects the condition and calls these; each one only acts from the
    // state it belongs to, so a stray call cannot derail the machine.
    void treasureCollected();   // TreasureRevealed -> Escape
    void reachedExit();         // Escape -> Escaped
    void reachedSanctuary();    // Escape -> Sanctuary (needs the charm)
    void gateSolved();          // Sanctuary -> Garden: the rings are aligned
    void caught();              // Escape -> Caught

    // Is the traveller under player control right now?
    bool playerHasControl() const;

    // How far out of the floor the treasure has risen, 0..1.
    float treasureReveal() const;

    // True once the verdict has been passed and the treasure has appeared.
    bool trialIsOver() const;

    // The Djinn's protective charm, 0..1. Rises from the energy column late in
    // Balanced and reaches the traveller at 1; he keeps it for the rest of the
    // run. Only a true heart ever earns one.
    float charm() const;
    bool  hasCharm() const { return charm() >= 1.0f; }

    // --- query ---
    TrialState state() const { return m_state; }
    const char* stateName() const;
    float stateTime() const { return m_stateTime; }

    // Would the current weight pass judgement? A live prediction, valid
    // before the heart is committed. Once the trial starts, the latched
    // verdict is what matters - see isBalanced().
    bool wouldBalance() const;

    // 0..1 through the current state's duration. Always 0 for Waiting, which
    // has no fixed length.
    float progress() const;

    float heartWeight() const { return m_heartWeight; }

    // While Waiting this reports the live prediction, so the readout tracks
    // the weight as you dial it in. Once committed it reports the verdict
    // that was latched at begin(), which is what the animation follows.
    bool isBalanced() const
    {
        return (m_state == TrialState::Waiting) ? wouldBalance() : m_balanced;
    }

    // The verdict angle in degrees, derived from the heart's weight. Positive
    // tips the heart's pan down.
    float verdictAngle() const;

    // How far through petrification the traveller is, 0..1. Non-zero only in
    // Cursed and while Reset unwinds it.
    float petrification() const;

    static float durationOf(TrialState state);

private:
    TrialState m_state = TrialState::Waiting;
    float m_stateTime  = 0.0f;

    // 0.5 is a perfectly balanced heart; the extremes are heavy with sin or
    // suspiciously weightless.
    float m_heartWeight = 0.5f;
    bool  m_balanced    = true;

    // Remembered so Reset knows whether to unwind the petrification. The
    // state itself is gone by then.
    bool  m_wasCaught   = false;

    void enter(TrialState next);
};

#endif // TRIALSTATE_H
