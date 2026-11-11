#include "ui/menuMain.hpp"

#include "core/achievements.hpp"
#include "core/game.hpp"
#include "core/gamemodes.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "render/texture.hpp"
#include "ui/menuAchievements.hpp"
#include "ui/menuCommon.hpp"
#include "ui/menuHighScores.hpp"
#include "ui/menuSelectGameType.hpp"
#include "ui/menuSettings.hpp"

#include <cstdio>
#include <cstring>

namespace menuMain {

namespace {

constexpr int kNumItems = 6;

enum Item
{
    ITEM_START_GAME = 0,
    ITEM_GAME_MODE, // cycles the gameplay mode (gameplaymodes registry)
    ITEM_TOP_SCORES,
    ITEM_ACHIEVEMENTS, // tracks + cosmetic selector
    ITEM_SETTINGS,
    ITEM_QUIT_GAME
};

const char* const kItemLabels[kNumItems] = {
    "START GAME",
    "GAME MODE",
    "TOP SCORES",
    "ACHIEVEMENTS",
    "SETTINGS",
    "QUIT GAME"
};

menuCommon::State sState;

// Neon hex badge textures, loaded lazily from the achievements catalog.
texture sBadgeTextures[achievements::BADGE_COUNT];
bool sBadgeTexturesAttempted = false;
bool sBadgeLoaded[achievements::BADGE_COUNT] = { false };

void ensureBadgeTexturesLoaded()
{
    if (sBadgeTexturesAttempted)
        return;

    for (int i = 0; i < achievements::BADGE_COUNT; ++i) {
        sBadgeTextures[i].load(achievements::badge(i).icon);
        sBadgeLoaded[i] = sBadgeTextures[i].mLoaded;
    }

    sBadgeTexturesAttempted = true;
}

// A single row of all six neon hex badges with locked ones dimmed, followed by
// a compact lifetime summary. Kept deliberately short so it never spans the
// playfield the way the old text block did.
void drawAchievementBadges()
{
    ensureBadgeTexturesLoaded();

    constexpr float kSize = 0.062f;
    constexpr float kStep = 0.15f;
    constexpr float kY = -0.80f;
    const int count = achievements::badgeCount();

    for (int i = 0; i < count; ++i) {
        const float x = ((i - (count - 1) * 0.5f) * kStep);
        const bool unlocked = achievements::badgeUnlocked(i);

        vector::pen pen = unlocked ? vector::pen(0.85f, 1.0f, 0.95f, 0.9f, 2)
                                   : vector::pen(0.4f, 0.45f, 0.55f, 0.28f, 2);

        if (sBadgeLoaded[i]) {
            sBadgeTextures[i].draw(x, kY, kSize, kSize);
            if (!unlocked)
                font::AlphanumericsPrint(font::ALIGN_CENTER, 0.014f, x, kY, &pen, "-");
        } else {
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, x, kY, &pen, "%s", unlocked ? "O" : "-");
        }
    }

    vector::pen summaryPen(0.62f, 0.82f, 1.0f, 0.6f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.885f, &summaryPen,
                             "RANK %d    BADGES %d/%d    COSMETIC: %s",
                             achievements::lifetimeRank(),
                             achievements::badgesUnlocked(),
                             achievements::badgeCount(),
                             achievements::cosmetic(achievements::equippedCosmetic()).name);
}

// Advance the selected gameplay mode by `dir` (+1 next, -1 previous) within
// the modes available in the gameplaymodes registry. Ignored while there is
// only one mode to choose from.
void advanceGameMode(int dir)
{
    const int n = gameplaymodes::count();
    if (n < 2)
        return;
    theGame->mModeIndex = (theGame->mModeIndex + dir + n) % n;
    theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
}

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

    // --- Change the gameplay mode on the GAME MODE row --------------------
    if (sState.selection == ITEM_GAME_MODE) {
        const int hDir = menuCommon::updateHorizontal(sState);
        if (hDir != 0) {
            advanceGameMode(hDir);
        }
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
        case ITEM_GAME_MODE:
            // Confirm cycles the mode, matching the stepped rows elsewhere.
            advanceGameMode(+1);
            break;
        case ITEM_TOP_SCORES:
            menuHighScores::init();
            game::mMenuScreen = game::MENU_HIGHSCORES;
            break;
        case ITEM_ACHIEVEMENTS:
            menuAchievements::init();
            game::mMenuScreen = game::MENU_ACHIEVEMENTS;
            break;
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

    constexpr float kScale = 0.02f;
    constexpr float kRowStep = 0.10f;
    constexpr float kFirstY = -0.18f;

    for (int i = 0; i < kNumItems; ++i) {
        const float y = kFirstY - (i * kRowStep);

        const bool selected = (i == sState.selection);
        vector::pen pen = selected ? vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 3)
                                   : vector::pen(0.55f, 0.75f, 0.9f, 0.45f, 3);
        vector::pen markerPen = selected ? vector::pen(1.0f, 0.9f, 0.5f, 1.0f, 3)
                                         : vector::pen(0.5f, 0.5f, 0.5f, 0.3f, 3);

        font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, -0.06f, y, &markerPen, ">");

        // The GAME MODE row shows the currently selected mode so changing it
        // with Left/Right (or confirm) has visible feedback.
        if (i == ITEM_GAME_MODE) {
            char label[64];
            snprintf(label, sizeof(label), "%s  %s", kItemLabels[i], gameplaymodes::name(theGame->mModeIndex));
            font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, 0.0f, y, &pen, "%s", label);
        } else {
            font::AlphanumericsPrint(font::ALIGN_CENTER, kScale, 0.0f, y, &pen, "%s", kItemLabels[i]);
        }
    }

    drawAchievementBadges();

    // --- Help footer --------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.5f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.010f, 0.0f, -0.96f, &pen, "Up Down Move   Space or Start Select   Back / O Settings");
    }
}

} // namespace menuMain
