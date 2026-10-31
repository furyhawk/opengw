#include "ui/menuMain.hpp"

#include "core/game.hpp"
#include "core/gamemodes.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "render/texture.hpp"
#include "ui/menuCommon.hpp"
#include "ui/menuHighScores.hpp"
#include "ui/menuSelectGameType.hpp"
#include "ui/menuSettings.hpp"

#include <cstdio>
#include <cstring>

namespace menuMain {

namespace {

constexpr int kNumItems = 5;

enum Item
{
    ITEM_START_GAME = 0,
    ITEM_GAME_MODE, // cycles the gameplay mode (gameplaymodes registry)
    ITEM_TOP_SCORES,
    ITEM_SETTINGS,
    ITEM_QUIT_GAME
};

const char* const kItemLabels[kNumItems] = {
    "START GAME",
    "GAME MODE",
    "TOP SCORES",
    "SETTINGS",
    "QUIT GAME"
};

menuCommon::State sState;

texture sIconTrophy;
texture sIconStar;
texture sIconLightning;
texture sIconCrown;
texture sIconMedal;
texture sIconGem;
bool sBadgeTexturesAttempted = false;

void ensureBadgeTexturesLoaded()
{
    if (sBadgeTexturesAttempted)
        return;

    sIconTrophy.load("assets/images/achievements/icon_trophy.png");
    sIconStar.load("assets/images/achievements/icon_star.png");
    sIconLightning.load("assets/images/achievements/icon_lightning.png");
    sIconCrown.load("assets/images/achievements/icon_crown.png");
    sIconMedal.load("assets/images/achievements/icon_medal.png");
    sIconGem.load("assets/images/achievements/icon_gem.png");

    sBadgeTexturesAttempted = true;
}

void drawBadgeIcon(texture& icon, float x, float y, float size, const char* fallback, vector::pen pen)
{
    if (icon.mLoaded) {
        icon.draw(x, y, size, size);
        return;
    }

    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, x, y, &pen, "%s", fallback);
}

void drawAchievementBadges()
{
    ensureBadgeTexturesLoaded();

    const settings& s = settings::get();
    const int modeCompletions =
        (s.mModeCompleteClassical ? 1 : 0)
        + (s.mModeCompleteEndless ? 1 : 0)
        + (s.mModeCompletePressureLadder ? 1 : 0);
    const bool allModes = modeCompletions >= 3;
    const int lifetimeRank = std::min(99, 1 + (s.mLifetimeAchievementPoints / 5000));

    vector::pen labelPen(0.78f, 0.9f, 1.0f, 0.62f, 2);
    vector::pen valuePen(1.0f, 1.0f, 1.0f, 0.9f, 2);

    // MODE completion track: trophy + star + lightning + crown (all-modes).
    const float modeY = -0.84f;
    drawBadgeIcon(sIconTrophy, -0.60f, modeY, 0.06f, "TR", valuePen);
    drawBadgeIcon(sIconStar, -0.53f, modeY, 0.06f, "ST", valuePen);
    drawBadgeIcon(sIconLightning, -0.46f, modeY, 0.06f, "LT", valuePen);
    drawBadgeIcon(sIconCrown, -0.39f, modeY, 0.06f, "CR", valuePen);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, -0.50f, -0.77f, &labelPen, "MODE TRACK");
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, -0.50f, -0.91f, &valuePen, "%d/3  CROWN %s", modeCompletions, allModes ? "ON" : "OFF");

    // PERFORMANCE track: medal + running milestone count.
    const float perfY = -0.84f;
    drawBadgeIcon(sIconMedal, 0.04f, perfY, 0.06f, "MD", valuePen);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, 0.04f, -0.77f, &labelPen, "PERFORMANCE");
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, 0.04f, -0.91f, &valuePen, "MILESTONES %d", s.mPerformanceMilestones);

    // LIFELONG track: gem rank/points across sessions.
    const float lifeY = -0.84f;
    drawBadgeIcon(sIconGem, 0.58f, lifeY, 0.06f, "GM", valuePen);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, 0.58f, -0.77f, &labelPen, "LIFELONG");
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.01f, 0.58f, -0.91f, &valuePen, "LV %d  PTS %d", lifetimeRank, s.mLifetimeAchievementPoints);
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
    constexpr float kRowStep = 0.115f;
    constexpr float kFirstY = -0.22f;

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
