#pragma once

// Short, device-aware control guidance shown at the start of a match. The
// first match of the session walks through Move -> Aim/Fire -> Bomb -> Pause;
// later matches show a compact one-line reminder for a few seconds.
namespace gameplayGuide {

void onMatchStart();
void update();
void draw();

} // namespace gameplayGuide
