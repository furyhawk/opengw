#include "ui/menuSettings.hpp"

#include "core/game.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/menuCommon.hpp"
#include "ui/menuMain.hpp"
#include "ui/menuPause.hpp"

#include <cstdio>
#include <cstring>

namespace menuSettings {

namespace {

constexpr int kNumRows = 10;

// Rows
enum Row
{
    ROW_FULLSCREEN = 0,
    ROW_WINDOW,
    ROW_BLOOM,
    ROW_VSYNC,
    ROW_GRID_SMOOTHING,
    ROW_PARTICLE_SMOOTHING,
    ROW_ENEMY_SMOOTHING,
    ROW_PLAYER_SMOOTHING,
    ROW_STAR_SMOOTHING,
    ROW_SOUND_VOLUME
};

const char* const kRowLabels[kNumRows] = {
    "FULLSCREEN",       "WINDOW SIZE",     "BLOOM",
    "VSYNC",            "GRID SMOOTHING",  "PARTICLE SMOOTH",
    "ENEMY SMOOTHING",  "PLAYER SMOOTHING", "STAR SMOOTHING",
    "SOUND VOLUME"
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

constexpr int kVolumeStep = 10;

menuCommon::State sState;

int currentResIndex()
{
    const settings& s = settings::get();
    for (int i = 0; i < kNumRes; ++i) {
        if (kResolutions[i].w == s.displayWidth && kResolutions[i].h == s.displayHeight)
            return i;
    }
    return 0;
}

void applyMasterVolume()
{
    theGame->mSound->setMasterVolume(settings::get().mSoundVolume / 100.0f);
}

// Apply a directional change (+1 = on/next, -1 = off/prev) to a row.
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
    case ROW_VSYNC:
        s.mVsync = on;
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
    case ROW_SOUND_VOLUME: {
        int v = s.mSoundVolume + dir * kVolumeStep;
        if (v < 0)
            v = 0;
        if (v > 100)
            v = 100;
        s.mSoundVolume = v;
        applyMasterVolume();
        break;
    }
    }
    theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
}

// Toggle/advance the selected row (used by the confirm button).
void toggleRow(int row)
{
    settings& s = settings::edit();
    switch (row) {
    case ROW_FULLSCREEN:
        s.mFullscreen = !s.mFullscreen;
        break;
    case ROW_BLOOM:
        s.mEnableGlow = !s.mEnableGlow;
        gfx_set_glow_enabled(s.mEnableGlow);
        break;
    case ROW_VSYNC:
        s.mVsync = !s.mVsync;
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
    case ROW_SOUND_VOLUME:
    case ROW_WINDOW:
        // Stepped/cycled rows advance one step on confirm too.
        applyValue(row, +1);
        return;
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
    case ROW_VSYNC:
        snprintf(out, outSize, "%s", s.mVsync ? "ON" : "OFF");
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
    case ROW_SOUND_VOLUME:
        snprintf(out, outSize, "%d%%", s.mSoundVolume);
        break;
    }
}

} // namespace

void init()
{
    menuCommon::reset(sState);
    applyMasterVolume(); // keep the mixer in sync with the stored setting
}

void run()
{
    // Wait for all inputs to be released after entering the screen.
    if (!sState.ready) {
        if (!menuCommon::anyConfirm() && !menuCommon::anyBack() && !menuCommon::anyPause()
            && menuCommon::vDir() == 0 && menuCommon::hDir() == 0) {
            sState.ready = true;
        }
        menuCommon::snapshot(sState);
        return;
    }

    // --- Move between rows -----------------------------------------------
    if (menuCommon::updateVertical(sState, kNumRows)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
    }

    // --- Change the selected row's value ---------------------------------
    const int hDir = menuCommon::updateHorizontal(sState);
    if (hDir != 0) {
        applyValue(sState.selection, hDir);
    }

    // --- Confirm toggles the selected row --------------------------------
    if (menuCommon::confirmEdge(sState)) {
        toggleRow(sState.selection);
    }

    // --- Back: save and return to wherever we came from ------------------
    if (menuCommon::backEdge(sState)) {
        settings::edit().save();
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        if (game::mPaused) {
            // Came from the pause menu: return to it, still paused.
            menuPause::init();
            game::mMenuScreen = game::MENU_PAUSE;
        } else {
            // Came from the title main menu.
            menuMain::init();
            game::mMenuScreen = game::MENU_TITLE;
        }
        init();
        return;
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    // --- Scrim so the menu reads cleanly over the background -------------
    menuCommon::drawScrim(0.86f, -0.86f);

    // --- Title -----------------------------------------------------------
    {
        vector::pen pen(0.4f, 1.0f, 0.6f, 1.0f, 3);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.03f, 0.0f, 0.72f, &pen, "Settings");
    }

    // --- Rows ------------------------------------------------------------
    constexpr float kScale = 0.0135f;
    constexpr float kAdvance = 1.9f; // font cell width in glyph units
    constexpr float kRowX = -0.66f;
    constexpr float kRowStep = 0.105f;
    constexpr float kFirstY = 0.5f;

    for (int i = 0; i < kNumRows; ++i) {
        char value[32];
        valueText(i, value, sizeof(value));

        const float y = kFirstY - (i * kRowStep);

        const bool selected = (i == sState.selection);
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

    // --- Group caption for the audio section -----------------------------
    {
        vector::pen pen(0.45f, 0.6f, 0.8f, 0.5f, 2);
        const float y = kFirstY - (static_cast<int>(ROW_SOUND_VOLUME) * kRowStep) + 0.045f;
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.009f, kRowX, y, &pen, "AUDIO");
    }

    // --- Help footer ------------------------------------------------------
    {
        vector::pen pen(0.6f, 0.8f, 1.0f, 0.6f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, 0.0f, -0.66f, &pen, "Arrows or WASD Move   Left Right or Space Change");
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, 0.0f, -0.74f, &pen, "Backspace or Back Button Returns");
    }
}

} // namespace menuSettings
