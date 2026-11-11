#include "ui/menuControls.hpp"

#include "core/controls.hpp"
#include "core/game.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/hints.hpp"
#include "ui/menuCommon.hpp"
#include "ui/menuMain.hpp"
#include "ui/menuPause.hpp"

namespace menuControls {

namespace {

menuCommon::State sState;

struct Row
{
    const char* action;
    const char* keyboard;
    const char* gamepad;
};

const Row kRows[] = {
    { "MOVE", "W A S D", "L STICK / D-PAD" },
    { "AIM + FIRE", "ARROW KEYS", "R STICK" },
    { "BOMB", "SPACE", "RT" },
    { "PAUSE", "P", "START" },
    { "CONFIRM", "ENTER / SPACE", "A" },
    { "CANCEL", "BACKSPACE", "B" },
    { "OPTIONS / HELP", "O", "BACK" },
    { "JOIN PLAYER", "1 2 3 4", "START OR A" },
};

constexpr int kRowCount = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));

void returnToOrigin()
{
    if (game::mPaused) {
        menuPause::init();
        game::mMenuScreen = game::MENU_PAUSE;
    } else {
        menuMain::init();
        game::mMenuScreen = game::MENU_TITLE;
    }
}

} // namespace

void init()
{
    menuCommon::reset(sState);
}

void run()
{
    // Wait for all inputs to be released after the screen opens.
    if (!sState.ready) {
        if (!menuCommon::anyConfirm() && !menuCommon::anyBack() && !menuCommon::anyPause()
            && menuCommon::vDir() == 0 && menuCommon::hDir() == 0) {
            sState.ready = true;
        }
        menuCommon::snapshot(sState);
        return;
    }

    if (menuCommon::backEdge(sState) || menuCommon::confirmEdge(sState)
        || menuCommon::pauseEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        returnToOrigin();
        return;
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    // The reference is text-heavy, so hide the attract-mode marquee/fireworks
    // completely rather than letting the bright title art show through.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColor4f(0, 0, 0, 1);
    glBegin(GL_QUADS);
    glVertex2d(-1.0, -1.0);
    glVertex2d(1.0, -1.0);
    glVertex2d(1.0, 1.0);
    glVertex2d(-1.0, 1.0);
    glEnd();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    const bool gamepad = hints::gamepadMode();

    vector::pen titlePen(0.75f, 0.95f, 1.0f, 0.95f, 3);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.028f, 0.0f, 0.78f, &titlePen, "CONTROLS");

    vector::pen subPen(0.55f, 0.72f, 0.86f, 0.55f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0105f, 0.0f, 0.70f, &subPen,
                             "DEVICE PROMPTS FOLLOW THE SETTING IN SETTINGS > CONTROLLER");

    constexpr float kActionX = -0.90f;
    constexpr float kKeyboardX = -0.30f;
    constexpr float kGamepadX = 0.34f;
    constexpr float kFirstY = 0.52f;
    constexpr float kStep = 0.095f;

    vector::pen active(0.95f, 0.98f, 1.0f, 0.92f, 2.5f);
    vector::pen inactive(0.50f, 0.66f, 0.82f, 0.42f, 2.0f);
    vector::pen actionPen(0.98f, 0.82f, 0.38f, 0.82f, 2.5f);

    vector::pen keyboardPen = gamepad ? inactive : active;
    vector::pen gamepadPen = gamepad ? active : inactive;

    font::AlphanumericsPrint(font::ALIGN_LEFT, 0.0115f, kKeyboardX, 0.61f,
                             &keyboardPen, "KEYBOARD");
    font::AlphanumericsPrint(font::ALIGN_LEFT, 0.0115f, kGamepadX, 0.61f,
                             &gamepadPen, "GAMEPAD");

    for (int i = 0; i < kRowCount; ++i) {
        const float y = kFirstY - (i * kStep);
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.0115f, kActionX, y, &actionPen, "%s", kRows[i].action);
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.0115f, kKeyboardX, y,
                                 &keyboardPen, "%s", kRows[i].keyboard);
        font::AlphanumericsPrint(font::ALIGN_LEFT, 0.0115f, kGamepadX, y,
                                 &gamepadPen, "%s", kRows[i].gamepad);
    }

    // --- Connected controllers -------------------------------------------
    vector::pen statusPen(0.45f, 0.85f, 0.75f, 0.70f, 2);
    const int padCount = theGame->mControls->gamepadCount();
    if (padCount <= 0) {
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0105f, 0.0f, -0.38f, &statusPen,
                                 "NO CONTROLLER DETECTED - CONNECT A PAD OR USE THE KEYBOARD");
    } else {
        for (int p = 0; p < 4; ++p) {
            if (!theGame->mControls->gamepadConnected(p))
                continue;

            const char* name = theGame->mControls->gamepadName(p);
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0100f, 0.0f,
                                     -0.36f - (p * 0.055f), &statusPen,
                                     "PAD %d  %s", p + 1, (name && *name) ? name : "CONTROLLER");
        }
    }

    // --- Footer ------------------------------------------------------------
    hints::bar({
        { "RETURN", "ENTER / BACKSPACE", "A / B / START" },
    });
}

} // namespace menuControls
