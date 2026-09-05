#pragma once

// Pause menu shown over the frozen match (reached from GAMEMODE_PLAYING by
// pressing the pause button). Offers Resume, the combined Settings screen,
// quitting to the title screen, or quitting the application.
namespace menuPause {
void init(); // call when the pause menu becomes active
void run();  // handle input (call each logic tick while MENU_PAUSE is active)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuPause
