#pragma once

#include "math/point3d.hpp"

// Shared helpers for the front-end menus (title main menu, pause menu and the
// combined settings screen). These screens are drawn as 2D (NDC) overlays and
// poll the controls of all four players, following the stick convention used
// everywhere in this codebase:
//   x > 0 = up, x < 0 = down ;  y > 0 = left, y < 0 = right
//
// Each menu keeps a menuCommon::State and drives it with the same sequence:
//   1. state.ready starts false; the menu waits until every input is released
//      (prevents a held button from triggering an action on entry).
//   2. Every frame: updateVertical / updateHorizontal for navigation,
//      confirmEdge / backEdge / pauseEdge for actions, then snapshot().
namespace menuCommon {

struct State
{
    int selection { 0 };
    bool ready { false }; // true once all buttons/sticks have been released
    int prevV { 0 };
    int prevH { 0 };
    int vHold { 0 };
    int hHold { 0 };
    bool prevConfirm { false }; // fire or start held last frame
    bool prevBack { false };
    bool prevPause { false };
};

void reset(State& s);

// --- Raw control polls (any of the four players) -------------------------
Point3d combinedStick();
bool anyFire();  // Space / right trigger
bool anyStart(); // 1..4 / gamepad South
bool anyBack();  // Backspace / O / gamepad East
bool anyPause(); // P / gamepad Start (menu button)
bool anyConfirm();

// First player (0..3) whose start button is held, or -1 if none.
int pressedStartPlayer();

// Vertical (+1 down, -1 up, 0) and horizontal (+1 right, -1 left, 0) stick
// directions for this frame.
int vDir();
int hDir();

// --- Navigation ----------------------------------------------------------
// Move the selection within [0, count). Auto-repeats while held. Returns true
// when the selection actually changed this frame.
bool updateVertical(State& s, int count);

// Horizontal value change with auto-repeat. Returns -1, 0 or +1 for this frame.
int updateHorizontal(State& s);

// --- Edge-triggered actions ----------------------------------------------
bool confirmEdge(const State& s);
bool backEdge(const State& s);
bool pauseEdge(const State& s);

// Record the current frame's inputs as "previous" (call at the end of run()).
void snapshot(State& s);

// --- Drawing -------------------------------------------------------------
// Dark translucent backdrop for a menu panel. Blending is left in the
// additive (SRC_ALPHA, ONE) state used for the glowing font rendering.
void drawScrim(float top, float bottom);

} // namespace menuCommon
