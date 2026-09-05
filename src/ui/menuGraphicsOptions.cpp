#include "ui/menuGraphicsOptions.hpp"

#include "core/game.hpp"
#include "core/controls.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace menuGraphicsOptions {

namespace {

constexpr int kNumRows = 9;

// Rows
enum Row
{
    ROW_FULLSCREEN = 0,
    ROW_WINDOW,
    ROW_BLOOM,
    ROW_GRID_SMOOTHING,
    ROW_PARTICLE_SMOOTHING,
    ROW_ENEMY_SMOOTHING,
    ROW_PLAYER_SMOOTHING,
    ROW_STAR_SMOOTHING,
    ROW_VSYNC
};

const char* const kRowLabels[kNumRows] = {
    "FULLSCREEN",      "WINDOW SIZE",   "BLOOM",
    "GRID SMOOTHING",  "PARTICLE SMOOTH", "ENEMY SMOOTHING",
    "PLAYER SMOOTHING", "STAR SMOOTHING", "VSYNC"
};

struct Res
{
    int w;
    int h;
};

const Res kResolutions[] = {
    { 800, 600 },  { 1024, 768 }, { 1280, 720 },
    { 1280, 800 }, { 1600, 900 }, { 1920, 1080 }
};
constexpr int kNumRes = static_cast<int>(sizeof(kResolutions) / sizeof(kResolutions[0]));

// Stick convention used everywhere in this codebase:
//   x > 0 = up,  x < 0 = down ;  y > 0 = left, y < 0 = right
int sSelection = 0;
bool sReady = false; // wait for all input to be released after entering
int sPrevV = 0;
int sPrevH = 0;
int sVHold = 0;
int sHHold = 0;
bool sPrevFire = false;

Point3d combinedStick()
{
    Point3d v;
    for (int p = 0; p < 4; ++p) {
        v += theGame->mControls->getLeftStick(p) + theGame->mControls->getRightStick(p);
    }
    return v;
}

bool anyBack()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getBackButton(p) || theGame->mControls->getOptionsButton(p))
            return true;
    }
    return false;
}

bool anyFire()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getTriggerButton(p) || theGame->mControls->getStartButton(p))
            return true;
    }
    return false;
}

int currentResIndex()
{
    const settings& s = settings::get();
    for (int i = 0; i < kNumRes; ++i) {
        if (kResolutions[i].w == s.displayWidth && kResolutions[i].h == s.displayHeight)
            return i;
    }
    return 0;
}

// Set a boolean-style setting from a direction (+1 = on/next, -1 = off/prev).
void applyValue(int row, int dir)
{
    settings& s = settings::edit();
    const bool on = dir > 0;

    switch (row) {
    case ROW_FULLSCREEN:
        s.mFullscreen = on;
        break;
    case ROW_WINDOW: {
        int idx = currentResIndex();
        idx += (dir > 0) ? 1 : -1;
        idx = (idx + kNumRes) % kNumRes;
        s.displayWidth = kResolutions[idx].w;
        s.displayHeight = kResolutions[idx].h;
        break;
    }
    case ROW_BLOOM:
        s.mEnableGlow = on;
        gfx_set_glow_enabled(on);
        break;
    case ROW_GRID_SMOOTHING:
        s.mGridSmoothing = on;
        break;
    case ROW_PARTICLE_SMOOTHING:
        s.mParticleSmoothing = on;
        break;
    case ROW_ENEMY_SMOOTHING:
        s.mEnemySmoothing = on;
        break;
    case ROW_PLAYER_SMOOTHING:
        s.mPlayerSmoothing = on;
        break;
    case ROW_STAR_SMOOTHING:
        s.mStarSmoothing = on;
        break;
    case ROW_VSYNC:
        s.mVsync = on;
        break;
    }
    theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
}

// Toggle/advance the selected row (used by the fire button).
void toggleRow(int row)
{
    settings& s = settings::edit();
    switch (row) {
    case ROW_FULLSCREEN:
        s.mFullscreen = !s.mFullscreen;
        break;
    case ROW_WINDOW:
        applyValue(row, +1);
        return;
    case ROW_BLOOM:
        s.mEnableGlow = !s.mEnableGlow;
        gfx_set_glow_enabled(s.mEnableGlow);
        break;
    case ROW_GRID_SMOOTHING:
        s.mGridSmoothing = !s.mGridSmoothing;
        break;
    case ROW_PARTICLE_SMOOTHING:
        s.mParticleSmoothing = !s.mParticleSmoothing;
        break;
    case ROW_ENEMY_SMOOTHING:
        s.mEnemySmoothing = !s.mEnemySmoothing;
        break;
    case ROW_PLAYER_SMOOTHING:
        s.mPlayerSmoothing = !s.mPlayerSmoothing;
        break;
    case ROW_STAR_SMOOTHING:
        s.mStarSmoothing = !s.mStarSmoothing;
        break;
    case ROW_VSYNC:
        s.mVsync = !s.mVsync;
        break;
    }
    theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
}

void valueText(int row, char* out, std::size_t outSize)
{
    const settings& s = settings::get();
    switch (row) {
    case ROW_FULLSCREEN:
        snprintf(out, outSize, "%s", s.mFullscreen ? "ON" : "OFF");
        break;
    case ROW_WINDOW:
        snprintf(out, outSize, "%dx%d", s.displayWidth, s.displayHeight);
        break;
    case ROW_BLOOM:
        snprintf(out, outSize, "%s", s.mEnableGlow ? "ON" : "OFF");
        break;
    case ROW_GRID_SMOOTHING:
        snprintf(out, outSize, "%s", s.mGridSmoothing ? "ON" : "OFF");
        break;
    case ROW_PARTICLE_SMOOTHING:
        snprintf(out, outSize, "%s", s.mParticleSmoothing ? "ON" : "OFF");
        break;
    case ROW_ENEMY_SMOOTHING:
        snprintf(out, outSize, "%s", s.mEnemySmoothing ? "ON" : "OFF");
        break;
    case ROW_PLAYER_SMOOTHING:
        snprintf(out, outSize, "%s", s.mPlayerSmoothing ? "ON" : "OFF");
        break;
    case ROW_STAR_SMOOTHING:
        snprintf(out, outSize, "%s", s.mStarSmoothing ? "ON" : "OFF");
        break;
    case ROW_VSYNC:
        snprintf(out, outSize, "%s", s.mVsync ? "ON" : "OFF");
        break;
    }
}

} // namespace

void init()
{
    sSelection = 0;
    sReady = false;
    sPrevV = 0;
    sPrevH = 0;
    sVHold = 0;
    sHHold = 0;
    sPrevFire = false;
}

void run()
{
    const Point3d stick = combinedStick();

    int vDir = 0;
    if (stick.x > 0.5f)
        vDir = -1; // up
    else if (stick.x < -0.5f)
        vDir = 1; // down

    int hDir = 0;
    if (stick.y > 0.5f)
        hDir = -1; // left
    else if (stick.y < -0.5f)
        hDir = 1; // right

    const bool back = anyBack();
    const bool fire = anyFire();

    // Wait for everything to be released after entering the screen so a held
    // button can't immediately trigger an action.
    if (!sReady) {
        if (vDir == 0 && hDir == 0 && !back && !fire) {
            sReady = true;
        }
        sPrevV = vDir;
        sPrevH = hDir;
        sPrevFire = fire;
        return;
    }

    // Back / options button: save settings and return to the title screen.
    if (back) {
        settings::edit().save();
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        game::mGameMode = game::GAMEMODE_ATTRACT;
        init();
        return;
    }

    // --- Move between rows (with auto-repeat while held) ----------------
    bool moved = false;
    if (vDir != 0) {
        if (vDir != sPrevV) {
            moved = true;
            sVHold = 0;
        } else if (++sVHold > 20) {
            moved = true;
            sVHold = 18;
        }
    } else {
        sVHold = 0;
    }
    if (moved) {
        sSelection += (vDir > 0) ? 1 : -1;
        if (sSelection < 0)
            sSelection = kNumRows - 1;
        if (sSelection >= kNumRows)
            sSelection = 0;
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
    }

    // --- Change the selected row's value --------------------------------
    bool changed = false;
    if (hDir != 0) {
        if (hDir != sPrevH) {
            changed = true;
            sHHold = 0;
        } else if (++sHHold > 20) {
            changed = true;
            sHHold = 18;
        }
    } else {
        sHHold = 0;
    }
    if (changed) {
        applyValue(sSelection, (hDir > 0) ? 1 : -1);
    }

    // --- Fire / start toggles the selected row --------------------------
    if (fire && !sPrevFire) {
        toggleRow(sSelection);
    }

    sPrevV = vDir;
    sPrevH = hDir;
    sPrevFire = fire;
}

void draw()
{
    // --- Scrim so the menu reads cleanly over the attract background -----
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.8f);
    glBegin(GL_QUADS);
    glVertex2d(-1.0, -0.80);
    glVertex2d(1.0, -0.80);
    glVertex2d(1.0, 0.80);
    glVertex2d(-1.0, 0.80);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // --- Title -----------------------------------------------------------
    {
        vector::pen pen(0.4f, 1.0f, 0.6f, 1.0f, 3);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.03f, 0.0f, 0.72f, &pen, "Graphics Options");
    }

    // --- Rows -------------------------------------------------------------
    constexpr float kScale = 0.0135f;
    constexpr float kAdvance = 1.9f; // font cell width in glyph units
    constexpr float kRowX = -0.62f;
    constexpr float kRowStep = 0.115f;
    constexpr float kFirstY = 0.48f;

    for (int i = 0; i < kNumRows; ++i) {
        char value[32];
        valueText(i, value, sizeof(value));

        const float y = kFirstY - (i * kRowStep);

        const bool selected = (i == sSelection);
        vector::pen labelPen = selected ? vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 3)
                                        : vector::pen(0.55f, 0.75f, 0.9f, 0.5f, 3);
        vector::pen valuePen = selected ? vector::pen(1.0f, 0.9f, 0.5f, 1.0f, 3)
                                        : vector::pen(0.7f, 0.7f, 0.7f, 0.5f, 3);

        if (selected) {
            // Selection marker
            font::AlphanumericsPrint(font::ALIGN_LEFT, kScale, kRowX - 0.07f, y, &valuePen, ">");
        }

        font::AlphanumericsPrint(font::ALIGN_LEFT, kScale, kRowX, y, &labelPen, "%s", kRowLabels[i]);

        const float labelWidth = static_cast<float>(strlen(kRowLabels[i])) * kAdvance * kScale;
        font::AlphanumericsPrint(font::ALIGN_LEFT, kScale, kRowX + labelWidth + 0.10f, y, &valuePen, "%s", value);
    }

    // --- Help footer -------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.6f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, 0.0f, -0.58f, &pen, "Arrows or WASD Move   Left Right or Space Change");
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, 0.0f, -0.66f, &pen, "Backspace or Back Button Returns");
    }
}

} // namespace menuGraphicsOptions
