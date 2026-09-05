#pragma once

// Combined settings screen (graphics + audio). Rendered as a 2D (NDC) overlay.
// It is reached from the title main menu (menuMain) or the in-game pause menu
// (menuPause). Every change is applied live (window/fullscreen/vsync are picked
// up by the host each frame; bloom/volume take effect immediately) and the
// whole set is persisted to settings.cfg when the player leaves with Back.
namespace menuSettings {
void init(); // call when entering the screen
void run();  // handle input, apply + persist changes (call each logic tick)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuSettings
