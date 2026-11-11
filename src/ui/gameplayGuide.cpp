#include "ui/gameplayGuide.hpp"

#include "core/controls.hpp"
#include "core/game.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "ui/hints.hpp"

#include <cmath>

namespace gameplayGuide {

namespace {

struct Step
{
    const char* action;
    const char* keyboard;
    const char* gamepad;
    const char* detail;
};

const Step kSteps[] = {
    { "MOVE", "W A S D", "L STICK / D-PAD", "MOVE YOUR SHIP TO SURVIVE" },
    { "AIM + FIRE", "ARROW KEYS", "R STICK", "PUSH THE AIM STICK TO FIRE" },
    { "BOMB", "SPACE", "RT", "CLEARS ENEMIES AROUND YOU" },
    { "PAUSE", "P", "START", "PAUSE OR OPEN THE CONTROLS SCREEN" },
};

constexpr int kStepCount = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));
constexpr int kStepFrames = 170;
constexpr int kMinimumReadFrames = 45;
constexpr int kCompactFrames = 300;

bool sActive = false;
bool sCompact = false;
bool sShownFullIntro = false;
int sStep = 0;
int sFrame = 0;
int sStepTimer = 0;

bool anyStickPastThreshold(bool left)
{
    for (int p = 0; p < 4; ++p) {
        const Point3d stick = left ? theGame->mControls->getLeftStick(p)
                                   : theGame->mControls->getRightStick(p);
        if (std::fabs(stick.x) > 0.15f || std::fabs(stick.y) > 0.15f)
            return true;
    }
    return false;
}

bool anyTrigger()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getTriggerButton(p))
            return true;
    }
    return false;
}

bool stepActionSeen()
{
    switch (sStep) {
    case 0:
        return anyStickPastThreshold(true);
    case 1:
        return anyStickPastThreshold(false);
    case 2:
        return anyTrigger();
    default:
        // Pause is handled by the shell, so this step simply times out.
        return false;
    }
}

void drawPanel(float top, float bottom)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.62f);
    glBegin(GL_QUADS);
    glVertex2d(-0.58f, bottom);
    glVertex2d(0.58f, bottom);
    glVertex2d(0.58f, top);
    glVertex2d(-0.58f, top);
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

} // namespace

void onMatchStart()
{
    sActive = true;
    sCompact = sShownFullIntro;
    sShownFullIntro = true;
    sStep = 0;
    sFrame = 0;
    sStepTimer = 0;
}

void update()
{
    if (!sActive)
        return;

    ++sFrame;
    ++sStepTimer;

    if (sCompact) {
        if (sFrame >= kCompactFrames)
            sActive = false;
        return;
    }

    const bool timedOut = sStepTimer >= kStepFrames;
    const bool actionSeen = sStepTimer >= kMinimumReadFrames && stepActionSeen();

    if (timedOut || actionSeen) {
        ++sStep;
        sStepTimer = 0;
        if (sStep >= kStepCount)
            sActive = false;
    }
}

void draw()
{
    if (!sActive)
        return;

    if (sCompact) {
        hints::bar({
            { "MOVE", "W A S D", "L STICK" },
            { "AIM + FIRE", "ARROW KEYS", "R STICK" },
            { "BOMB", "SPACE", "RT" },
            { "PAUSE", "P", "START" },
        }, -0.90f);
        return;
    }

    const Step& step = kSteps[sStep];

    drawPanel(-0.46f, -0.76f);

    vector::pen counterPen(0.48f, 0.72f, 0.95f, 0.62f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0095f, 0.0f, -0.50f, &counterPen,
                             "CONTROLLER GUIDE %d/%d", sStep + 1, kStepCount);

    hints::bar({
        { step.action, step.keyboard, step.gamepad },
    }, -0.61f);

    vector::pen detailPen(0.55f, 0.82f, 0.95f, 0.62f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.0100f, 0.0f, -0.70f, &detailPen,
                             "%s", step.detail);
}

} // namespace gameplayGuide
