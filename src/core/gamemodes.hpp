#pragma once

#include <memory>

class gameplay_mode;

// gameplaymodes -------------------------------------------------------------
// Registry of the match modes the player can pick from (the title menu's
// "GAME MODE" row) and that game::startGame() instantiates for each new
// match. The shell and the menus only ever talk to a mode through its index
// into this registry -- the index stored by game::mModeIndex.
//
// Adding a new selectable mode is:
//   1. implement a gameplay_mode subclass (see classical_mode for the
//      pattern: a name() plus begin/end/update/draw driven via `game&`),
//   2. add one { name, factory } row to the table in gamemodes.cpp.
// No other shell or menu code has to change: the "GAME MODE" row and
// startGame() pick up new modes automatically from count()/name()/create().
namespace gameplaymodes {

// Number of selectable gameplay modes (table size).
int count();

// Display name of the mode at `index` (e.g. "Classical"); "?" out of range.
const char* name(int index);

// Build a fresh gameplay_mode instance for the mode at `index`.
// Returns nullptr when `index` is out of range.
std::unique_ptr<gameplay_mode> create(int index);

} // namespace gameplaymodes
