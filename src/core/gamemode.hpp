#pragma once

class game;

// gameplay_mode -------------------------------------------------------------
// Abstract contract for a playable match mode -- "how a round is played".
//
// The original OpenGW match rules (escalating spawner/wave pacing, enemy mix,
// scoring/multiplier/weapon/life/bomb rules, lives & shared co-op pools) are
// encapsulated by classical_mode. The game shell owns exactly one active
// gameplay_mode at a time and dispatches lifecycle (begin/end) and per-frame
// (update/draw) calls to it, so the shell stays free of mode-specific rules
// and new modes can be added without touching the shell.
//
// A mode is "active" from when a match starts (game::startGame) until the
// game returns to attract mode. While active, the shell routes the fixed
// 60 Hz logic tick and the world render to the mode.
class gameplay_mode
{
  public:
    virtual ~gameplay_mode() = default;

    // Display name used by menus / HUD / logs (e.g. "Classical").
    virtual const char* name() const = 0;

    // A new match is about to run (players already joined). The mode resets
    // its own rules/state and prepares the arena: players, spawner, the
    // co-op shared life/bomb pool and the match music.
    virtual void begin_match(game& owner) = 0;

    // The last life has been lost: tear the match down (stop players, clear
    // enemies/attractors, settle sounds). The shell remains in the game-over
    // flow afterwards so the mode's arena can keep being drawn while it fades.
    virtual void end_match(game& owner) = 0;

    // Advance an in-progress match by one fixed logic tick.
    virtual void update(game& owner) = 0;

    // Draw the mode's arena/world for the given render pass. Called while the
    // match is on screen (playing and the game-over fade-out).
    virtual void draw(game& owner, int pass) = 0;

    // ---- Rules queried by gameplay systems --------------------------------
    // Co-op "shared pool" values. A mode that keeps lives/bombs per player
    // leaves these at their defaults; classical_mode shares a single pool
    // whenever more than one player is in the match.
    virtual int shared_lives() const { return 0; }
    virtual int shared_bombs() const { return 0; }
    virtual void add_shared_life() {}
    virtual void take_shared_life() {}
    virtual void add_shared_bomb() {}
    virtual void take_shared_bomb() {}
};
