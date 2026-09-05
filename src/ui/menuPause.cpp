#include "ui/menuPause.hpp"

#include "core/game.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/menuCommon.hpp"
#include "ui/menuSettings.hpp"

namespace menuPause {

namespace {

constexpr int kNumItems = 4;

enum Item
{
    ITEM_RESUME = 0,
    ITEM_SETTINGS,
    ITEM_QUIT_TO_TITLE,
    ITEM_QUIT_GAME
};

const char* const kItemLabels[kNumItems] = {
    "RESUME",
    "SETTINGS",
    "QUIT TO TITLE",
    "QUIT GAME"
};

menuCommon::State sState;

void resumeGame()
{
    theGame->mSound->unpauseAllTracks();
    game::mPaused = false;
    game::mMenuScreen = game::MENU_NONE;
}

} // namespace

void init()
{
    menuCommon::reset(sState);
}

void run()
{
    // Wait for all inputs to be released after the menu appears.
    if (!sState.ready) {
        if (!menuCommon::anyConfirm() && !menuCommon::anyBack() && !menuCommon::anyPause()
            && menuCommon::vDir() == 0 && menuCommon::hDir() == 0) {
            sState.ready = true;
        }
        menuCommon::snapshot(sState);
        return;
    }

    // --- Move between items ------------------------------------------------
    if (menuCommon::updateVertical(sState, kNumItems)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
    }

    // --- Pause / back button resumes the game -------------------------------
    if (menuCommon::pauseEdge(sState) || menuCommon::backEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        resumeGame();
        menuCommon::snapshot(sState);
        return;
    }

    // --- Confirm an item ----------------------------------------------------
    if (menuCommon::confirmEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);

        switch (sState.selection) {
        case ITEM_RESUME:
            resumeGame();
            break;
        case ITEM_SETTINGS:
            menuSettings::init();
            game::mMenuScreen = game::MENU_SETTINGS;
            break;
        case ITEM_QUIT_TO_TITLE:
            theGame->abandonMatch();
            break;
        case ITEM_QUIT_GAME:
            // Exit the application (settings are persisted by the host).
            resumeGame();
            game::mQuitRequested = true;
            break;
        }
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    // A translucent band keeps the menu readable over the frozen arena.
    menuCommon::drawScrim(0.68f, -0.68f);

    // --- Title --------------------------------------------------------------
    {
        vector::pen pen(0.4f, 1.0f, 0.6f, 1.0f, 3);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.032f, 0.0f, 0.5f, &pen, "Paused");
    }

    // --- Items --------------------------------------------------------------
    constexpr float kScale = 0.019f;
    constexpr float kRowStep = 0.15f;
    constexpr float kFirstY = 0.22f;

    for (int i = 0; i < kNumItems; ++i) {
        const float y = kFirstY - (i * kRowStep);

        const bool selected = (i == sState.selection);
        vector::pen pen = selected ? vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 3)
                                   : vector::pen(0.55f, 0.75f, 0.9f, 0.5f, 3);
        vector::pen markerPen = selected ? vector::pen(1.0f, 0.9f, 0.5f, 1.0f, 3)
                                         : vector::pen(0.5f, 0.5f, 0.5f, 0.3f, 3);

        font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, -0.13f, y, &markerPen, ">");
        font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, 0.0f, y, &pen, "%s", kItemLabels[i]);
    }

    // --- Help footer --------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.5f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.6f, &pen, "Up Down Move   Space Select   P / Back Resume");
    }
}

} // namespace menuPause
