#pragma once

// Graphics options screen. Rendered as a 2D (NDC) overlay while the game is in
// GAMEMODE_OPTIONS. Toggles are applied live and persisted to settings.cfg.
namespace menuGraphicsOptions
{
void init(); // call when entering the screen
void run();  // handle input, apply + persist changes (call each logic tick)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuGraphicsOptions
