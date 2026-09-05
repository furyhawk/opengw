#pragma once

// Title / main menu shown over the credited screen (GAMEMODE_CREDITED).
// Lets the player start a game, open the settings screen or quit the app.
// Drawn as a 2D (NDC) overlay on top of the attract marquee.
namespace menuMain {
void init(); // call when the menu becomes active
void run();  // handle input (call each logic tick while MENU_TITLE is active)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuMain
