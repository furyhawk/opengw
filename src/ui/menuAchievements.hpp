#pragma once

// Achievements screen reachable from the title main menu.
//
// Shows the three reward tracks (Mode Mastery / Performance / Lifelong) with
// their neon hex badges, plus the cosmetic ship-palette selector. Left/Right
// cycles the equipped cosmetic among the unlocked ones; Back/Confirm/Pause
// returns to the title menu.
namespace menuAchievements {
void init(); // call when the screen becomes active
void run();  // handle input (call each logic tick while MENU_ACHIEVEMENTS is active)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuAchievements
