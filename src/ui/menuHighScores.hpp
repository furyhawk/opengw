#pragma once

// "Top Scores" screen reachable from the title main menu (menuMain). Shows the
// persistent high-score table (highscore::drawTable) over the attract backdrop.
// Any of Confirm / Back / Pause returns to the title main menu. It only ever
// runs while in the credited/title flow (GAMEMODE_CREDITED).
namespace menuHighScores {
void init(); // call when the screen becomes active
void run();  // handle input (call each logic tick while MENU_HIGHSCORES is active)
void draw(); // draw the 2D overlay (call between scene::glEnable2D/glDisable2D)
} // namespace menuHighScores
