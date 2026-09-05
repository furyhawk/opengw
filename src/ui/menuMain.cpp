#include "ui/menuMain.hpp"

#include "core/game.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/menuCommon.hpp"
#include "ui/menuSelectGameType.hpp"
#include "ui/menuSettings.hpp"

#include <cstring>

namespace menuMain {

namespace {

constexpr int kNumItems = 3;

enum Item
{
    ITEM_START_GAME = 0,
    ITEM_SETTINGS,
    ITEM_QUIT_GAME
};

const char* const kItemLabels[kNumItems] = {
    "START GAME",
    "SETTINGS",
    "QUIT GAME"
};

menuCommon::State sState;

} // namespace

void init()
{
    menuCommon::reset(sState);
}

void run()
{
    // Wait for all inputs to be released after the menu appears so a held
    // button (e.g. the one that finished the previous match) can't fire.
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

    // --- Back / options button: shortcut to the settings screen ------------
    if (menuCommon::backEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        menuSettings::init();
        game::mMenuScreen = game::MENU_SETTINGS;
        menuCommon::snapshot(sState);
        return;
    }

    // --- Confirm an item ----------------------------------------------------
    if (menuCommon::confirmEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);

        switch (sState.selection) {
        case ITEM_START_GAME: {
            // If a specific player's start button was pressed (1-4 / South on
            // a gamepad), carry that through to the player-selection screen.
            const int startPlayer = menuCommon::pressedStartPlayer();
            menuSelectGameType::init(startPlayer >= 0 ? startPlayer : 0);
            game::mGameMode = game::GAMEMODE_CHOOSE_GAMETYPE;
            game::mMenuScreen = game::MENU_NONE;
            break;
        }
        case ITEM_SETTINGS:
            menuSettings::init();
            game::mMenuScreen = game::MENU_SETTINGS;
            break;
        case ITEM_QUIT_GAME:
            game::mQuitRequested = true;
            break;
        }
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    // A translucent band behind the menu keeps the text readable over the
    // attract marquee while leaving the logo itself visible above it.
    menuCommon::drawScrim(-0.20f, -0.95f);

    constexpr float kScale = 0.022f;
    constexpr float kRowStep = 0.15f;
    constexpr float kFirstY = -0.28f;

    for (int i = 0; i < kNumItems; ++i) {
        const float y = kFirstY - (i * kRowStep);

        const bool selected = (i == sState.selection);
        vector::pen pen = selected ? vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 3)
                                   : vector::pen(0.55f, 0.75f, 0.9f, 0.45f, 3);
        vector::pen markerPen = selected ? vector::pen(1.0f, 0.9f, 0.5f, 1.0f, 3)
                                         : vector::pen(0.5f, 0.5f, 0.5f, 0.3f, 3);

        font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, -0.06f, y, &markerPen, ">");
        font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, 0.0f, y, &pen, "%s", kItemLabels[i]);
    }

    // --- Help footer --------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.5f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.78f, &pen, "Up Down Move   Space or Start Select   Back / O Settings");
    }
}

} // namespace menuMain
