#include "ui/menuSettings.hpp"

#include "core/controls.hpp"
#include "core/game.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/hints.hpp"
#include "ui/menuCommon.hpp"
#include "ui/menuMain.hpp"
#include "ui/menuPause.hpp"

#include <cstdio>
#include <cstring>

namespace menuSettings {

namespace {

constexpr int kNumRows = 12;

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
    ROW_SOUND_VOLUME,
    ROW_VIBRATION,
    ROW_PROMPTS
};

const char* const kRowLabels[kNumRows] = {
    "FULLSCREEN",       "WINDOW SIZE",     "BLOOM",
    "VSYNC",            "GRID SMOOTHING",  "PARTICLE SMOOTH",
    "ENEMY SMOOTHING",  "PLAYER SMOOTHING", "STAR SMOOTHING",
    "SOUND VOLUME",     "VIBRATION",       "PROMPTS"
};

const char* const kPromptStyles[] = { "AUTO", "GAMEPAD", "KEYBOARD" };

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

void pulseVibration()
{
    for (int p = 0; p < 4; ++p) {
        if (!theGame->mControls->gamepadConnected(p))
            continue;
        theGame->mControls->rumble(p, 0.2f, 0.4f, 120);
        break;
    }
}

void stopVibration()
{
    for (int p = 0; p < 4; ++p)
        theGame->mControls->stopRumble(p);
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
    case ROW_VIBRATION:
        s.mVibration = on;
        if (on)
            pulseVibration();
        else
            stopVibration();
        break;
    case ROW_PROMPTS:
        s.mPromptStyle = (s.mPromptStyle + (dir > 0 ? 1 : 2)) % 3;
        break;
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
    case ROW_VIBRATION:
        s.mVibration = !s.mVibration;
        if (s.mVibration)
            pulseVibration();
        else
            stopVibration();
        break;
    case ROW_PROMPTS:
        s.mPromptStyle = (s.mPromptStyle + 1) % 3;
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
    case ROW_VIBRATION:
        snprintf(out, outSize, "%s", s.mVibration ? "ON" : "OFF");
        break;
    case ROW_PROMPTS:
        snprintf(out, outSize, "%s", kPromptStyles[s.mPromptStyle]);
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
    constexpr float kRowStep = 0.095f;
    constexpr float kFirstY = 0.52f;

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

    // --- Group captions --------------------------------------------------
    {
        vector::pen pen(0.45f, 0.6f, 0.8f, 0.5f, 2);
        const float audioY = kFirstY - (static_cast<int>(ROW_SOUND_VOLUME) * kRowStep) + 0.042f;
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.009f, kRowX, audioY, &pen, "AUDIO");

        const float controllerY = kFirstY - (static_cast<int>(ROW_VIBRATION) * kRowStep) + 0.042f;
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.009f, kRowX, controllerY, &pen, "CONTROLLER");
    }

    // --- Connected controller status -------------------------------------
    {
        vector::pen pen(0.45f, 0.65f, 0.85f, 0.55f, 2);
        char status[96];
        if (theGame->mControls->gamepadCount() > 0) {
            status[0] = '\0';
            for (int p = 0; p < 4; ++p) {
                if (!theGame->mControls->gamepadConnected(p))
                    continue;
                const char* name = theGame->mControls->gamepadName(p);
                snprintf(status, sizeof(status), "PAD %d: %s", p + 1,
                         (name && *name) ? name : "CONTROLLER");
                break;
            }
        } else {
            snprintf(status, sizeof(status), "NO CONTROLLER CONNECTED");
        }
        font::AlphanumericsPrint(font::ALIGN_RIGHT, 0.009f, 0.88f, 0.62f, &pen, "%s", status);
    }

    // --- Help footer ------------------------------------------------------
    hints::bar({
        { "MOVE", "UP DOWN / WASD", "L STICK / D-PAD" },
        { "CHANGE", "LEFT RIGHT / SPACE", "L STICK / A" },
        { "BACK", "BACKSPACE", "B / BACK" },
    }, -0.70f);
}

} // namespace menuSettings
