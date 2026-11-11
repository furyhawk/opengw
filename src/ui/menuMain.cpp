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

#include <cmath>
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

// --- Title layout ----------------------------------------------------------
// The marquee logo is drawn by the scene in the left column; everything here
// lives in the right-hand menu column.
constexpr float kMenuCenterX = 0.50f;
constexpr float kRowFirstY = 0.40f;
constexpr float kRowStep = 0.115f;
constexpr float kRowScale = 0.0185f;
constexpr float kSelectedScale = 0.0215f;
constexpr float kBadgeY = -0.58f;

int sAnimTick = 0;

float textWidth(const char* text, float scale)
{
    float width = 0.0f;
    font::AlphanumericsMeasureString(text, scale, &width, nullptr);
    return width;
}

// Soft gradient shading behind the menu column and along the bottom edge so
// the attract fireworks never compete with the text. Leaves blending in the
// additive state the font renderer expects.
void drawMenuBackdrop()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);

    // Gentle global veil: the attract fireworks stay lively but never blow
    // out the logo or the menu text.
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.30f);
    glVertex2d(-1.0f, -1.0f);
    glVertex2d(1.0f, -1.0f);
    glVertex2d(1.0f, 1.0f);
    glVertex2d(-1.0f, 1.0f);
    glEnd();

    // Right column: ramps quickly, then holds.
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.0f);
    glVertex2d(0.02f, -1.0f);
    glColor4f(0, 0, 0, 0.66f);
    glVertex2d(0.34f, -1.0f);
    glVertex2d(0.34f, 1.0f);
    glColor4f(0, 0, 0, 0.0f);
    glVertex2d(0.02f, 1.0f);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.66f);
    glVertex2d(0.34f, -1.0f);
    glColor4f(0, 0, 0, 0.76f);
    glVertex2d(1.0f, -1.0f);
    glVertex2d(1.0f, 1.0f);
    glColor4f(0, 0, 0, 0.66f);
    glVertex2d(0.34f, 1.0f);
    glEnd();

    // Bottom strip for the hint line.
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.0f);
    glVertex2d(-1.0f, -0.62f);
    glVertex2d(1.0f, -0.62f);
    glColor4f(0, 0, 0, 0.55f);
    glVertex2d(1.0f, -1.0f);
    glVertex2d(-1.0f, -1.0f);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

// Thin neon rule separating the logo column from the menu column, fading out
// at both ends so it reads as light rather than a hard border.
void drawColumnDivider()
{
    constexpr float kX = 0.035f;
    constexpr float kTop = 0.80f;
    constexpr float kBottom = -0.62f;

    glLineWidth(2);

    glBegin(GL_LINES);
    glColor4f(0.20f, 0.70f, 1.0f, 0.0f);
    glVertex2d(kX, kTop);
    glColor4f(0.20f, 0.70f, 1.0f, 0.30f);
    glVertex2d(kX, 0.10f);
    glVertex2d(kX, 0.10f);
    glColor4f(0.20f, 0.70f, 1.0f, 0.0f);
    glVertex2d(kX, kBottom);
    glEnd();
}

// Soft light bar behind the highlighted row, plus an underline that anchors
// it. `intensity` lets the bar breathe with the menu animation.
void drawSelectionBar(float y, float halfWidth, float intensity)
{
    constexpr float kHalfHeight = 0.045f;
    const float a = 0.22f * intensity;

    // A single fan: bright centre fading to nothing at all four corners reads
    // as a pool of light rather than a UI box.
    const float x0 = kMenuCenterX - halfWidth;
    const float x1 = kMenuCenterX + halfWidth;
    const float y0 = y - kHalfHeight;
    const float y1 = y + kHalfHeight;

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.12f, 0.55f, 1.0f, a);
    glVertex2d(kMenuCenterX, y);
    glColor4f(0.12f, 0.55f, 1.0f, 0.0f);
    glVertex2d(x0, y0);
    glVertex2d(x1, y0);
    glVertex2d(x1, y1);
    glVertex2d(x0, y1);
    glVertex2d(x0, y0);
    glEnd();

    glLineWidth(2);
    glBegin(GL_LINES);
    glColor4f(1.0f, 0.82f, 0.35f, 0.0f);
    glVertex2d(kMenuCenterX - halfWidth * 0.85f, y - kHalfHeight);
    glColor4f(1.0f, 0.82f, 0.35f, 0.70f * intensity);
    glVertex2d(kMenuCenterX + halfWidth * 0.85f, y - kHalfHeight);
    glEnd();
}

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

    constexpr float kSize = 0.052f;
    constexpr float kStep = 0.118f;
    const int count = achievements::badgeCount();

    for (int i = 0; i < count; ++i) {
        const float x = kMenuCenterX + ((i - (count - 1) * 0.5f) * kStep);
        const bool unlocked = achievements::badgeUnlocked(i);

        vector::pen pen = unlocked ? vector::pen(0.85f, 1.0f, 0.95f, 0.9f, 2)
                                   : vector::pen(0.4f, 0.45f, 0.55f, 0.28f, 2);

        if (sBadgeLoaded[i]) {
            sBadgeTextures[i].draw(x, kBadgeY, kSize, kSize);
            if (!unlocked)
                font::AlphanumericsPrint(font::ALIGN_CENTER, 0.014f, x, kBadgeY, &pen, "-");
        } else {
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, x, kBadgeY, &pen, "%s", unlocked ? "O" : "-");
        }
    }

    vector::pen summaryPen(0.62f, 0.82f, 1.0f, 0.6f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0092f, kMenuCenterX, kBadgeY - 0.085f, &summaryPen,
                             "RANK %d    BADGES %d/%d    COSMETIC %s",
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
    sAnimTick = 0;
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
    ++sAnimTick;

    drawMenuBackdrop();
    drawColumnDivider();

    const float pulse = 0.5f + 0.5f * sinf(sAnimTick * 0.09f);
    // Short entrance: rows fade in and settle upward over the first frames.
    const float intro = (sAnimTick < 20) ? (static_cast<float>(sAnimTick) / 20.0f) : 1.0f;

    for (int i = 0; i < kNumItems; ++i) {
        const bool selected = (i == sState.selection);
        const float y = kRowFirstY - (i * kRowStep) + (1.0f - intro) * 0.035f;
        const float scale = selected ? kSelectedScale : kRowScale;

        // The GAME MODE row shows the current mode; the footer swaps in a
        // Left/Right hint while this row is selected.
        char label[64];
        char modeName[32] = "";
        if (i == ITEM_GAME_MODE) {
            snprintf(modeName, sizeof(modeName), "%s", gameplaymodes::name(theGame->mModeIndex));
            snprintf(label, sizeof(label), "GAME MODE   %s", modeName);
        } else {
            snprintf(label, sizeof(label), "%s", kItemLabels[i]);
        }

        const float width = textWidth(label, scale);

        if (selected) {
            drawSelectionBar(y, width * 0.5f + 0.06f, (0.85f + 0.15f * pulse) * intro);

            vector::pen bracketPen(1.0f, 0.82f, 0.35f, (0.75f + 0.25f * pulse) * intro, 3);
            font::AlphanumericsPrint(font::ALIGN_CENTER, kRowScale,
                                     kMenuCenterX - width * 0.5f - 0.045f, y, &bracketPen, "[");
            font::AlphanumericsPrint(font::ALIGN_CENTER, kRowScale,
                                     kMenuCenterX + width * 0.5f + 0.045f, y, &bracketPen, "]");

            // Faint halo behind the highlighted label.
            vector::pen haloPen(0.30f, 0.80f, 1.0f, (0.10f + 0.05f * pulse) * intro, 7);
            font::AlphanumericsPrint(font::ALIGN_CENTER, scale, kMenuCenterX, y, &haloPen, "%s", label);
        }

        vector::pen pen = selected ? vector::pen(1.0f, 1.0f, 1.0f, intro, 4)
                                   : vector::pen(0.55f, 0.75f, 0.9f, 0.62f * intro, 3);
        font::AlphanumericsPrint(font::ALIGN_CENTER, scale, kMenuCenterX, y, &pen, "%s", label);

        // Accent the mode name so the row reads as a selector. The value is
        // redrawn over its copy in the label, so it must land on the same
        // glyph cells (the font lays text out from each cell's centre).
        if (i == ITEM_GAME_MODE) {
            const char* value = strstr(label, modeName);
            const size_t prefixLen = value ? static_cast<size_t>(value - label) : 0;
            char prefix[64];
            memcpy(prefix, label, prefixLen);
            prefix[prefixLen] = '\0';
            const float valueX = kMenuCenterX - width * 0.5f
                + textWidth(prefix, scale) + textWidth(modeName, scale) * 0.5f;
            vector::pen valuePen = selected ? vector::pen(1.0f, 0.72f, 0.25f, intro, 3)
                                            : vector::pen(0.40f, 0.85f, 0.70f, 0.70f * intro, 3);
            font::AlphanumericsPrint(font::ALIGN_CENTER, scale, valueX, y, &valuePen, "%s", modeName);
        }
    }

    drawAchievementBadges();

    // --- Bottom row ---------------------------------------------------------
    if (settings::get().mCreditsPerGame == 0) {
        vector::pen freePlayPen(0.35f, 0.85f, 0.75f, 0.55f * intro, 2);
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.010f, -0.94f, -0.925f, &freePlayPen, "FREE PLAY");
    }

    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.65f * intro, 2);
        if (sState.selection == ITEM_GAME_MODE) {
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0092f, 0.0f, -0.925f, &pen,
                                     "LEFT RIGHT CHANGE MODE      ENTER OR START  SELECT      BACK OR O  OPTIONS");
        } else {
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0092f, 0.0f, -0.925f, &pen,
                                     "ARROWS OR STICK  MOVE      ENTER OR START  SELECT      BACK OR O  OPTIONS");
        }
    }
}

} // namespace menuMain
