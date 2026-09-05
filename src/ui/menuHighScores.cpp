#include "ui/menuHighScores.hpp"

#include "core/game.hpp"
#include "core/highscore.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/menuCommon.hpp"
#include "ui/menuMain.hpp"

namespace menuHighScores {

namespace {

menuCommon::State sState;

} // namespace

void init()
{
    menuCommon::reset(sState);
}

void run()
{
    // Wait for all inputs to be released after the screen appears (we arrive
    // here with the confirm button that picked "TOP SCORES" possibly held).
    if (!sState.ready) {
        if (!menuCommon::anyConfirm() && !menuCommon::anyBack() && !menuCommon::anyPause()
            && menuCommon::vDir() == 0 && menuCommon::hDir() == 0) {
            sState.ready = true;
        }
        menuCommon::snapshot(sState);
        return;
    }

    // Confirm / Back / Pause all return to the title main menu.
    if (menuCommon::confirmEdge(sState) || menuCommon::backEdge(sState) || menuCommon::pauseEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        menuMain::init();
        game::mMenuScreen = game::MENU_TITLE;
        menuCommon::snapshot(sState);
        return;
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    // Full-height translucent panel so the table reads cleanly over the
    // attract-mode fireworks behind it.
    menuCommon::drawScrim(0.9f, -0.9f);

    theGame->mHighscore->drawTable();

    // --- Help footer --------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.5f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.92f, &pen,
                                 "Back or Start Return");
    }
}

} // namespace menuHighScores
