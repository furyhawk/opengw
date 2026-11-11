#include "ui/hints.hpp"

#include "core/controls.hpp"
#include "core/game.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"

namespace hints {

namespace {

constexpr float kTextScale = 0.0105f;

} // namespace

bool gamepadMode()
{
    if (settings::get().mPromptStyle == 1)
        return true;
    if (settings::get().mPromptStyle == 2)
        return false;

    if (!theGame || !theGame->mControls)
        return false;

    const controls* c = theGame->mControls.get();
    if (c->lastInput() == controls::InputDevice::Gamepad)
        return c->gamepadCount() > 0;
    if (c->hasInput())
        return false;

    // No input yet this session: prefer gamepad prompts when a pad is present.
    return c->gamepadCount() > 0;
}

float textWidth(const char* text, float scale)
{
    if (!text)
        return 0.0f;

    float width = 0.0f;
    font::AlphanumericsMeasureString(text, scale, &width, nullptr);
    return width;
}

void cap(float x, float y, const char* label, float scale, const float rgba[4])
{
    if (!label || !*label)
        return;

    const float width = textWidth(label, scale);
    const float halfW = width * 0.5f + 0.014f;
    const float halfH = 0.018f;

    glLineWidth(2.0f);
    glColor4f(rgba[0], rgba[1], rgba[2], rgba[3] * 0.16f);
    glBegin(GL_QUADS);
    glVertex2d(x - halfW, y - halfH);
    glVertex2d(x + halfW, y - halfH);
    glVertex2d(x + halfW, y + halfH);
    glVertex2d(x - halfW, y + halfH);
    glEnd();

    glColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
    glBegin(GL_LINE_LOOP);
    glVertex2d(x - halfW, y - halfH);
    glVertex2d(x + halfW, y - halfH);
    glVertex2d(x + halfW, y + halfH);
    glVertex2d(x - halfW, y + halfH);
    glEnd();

    vector::pen pen(rgba[0], rgba[1], rgba[2], rgba[3], 2.5f);
    font::AlphanumericsPrint(font::ALIGN_CENTER, scale, x, y, &pen, "%s", label);
}

void bar(std::initializer_list<Hint> hintList, float y)
{
    const bool gamepad = gamepadMode();

    const int count = static_cast<int>(hintList.size());
    if (count <= 0)
        return;

    constexpr float kGap = 0.045f;
    constexpr float kCapPad = 0.014f;
    constexpr float kLabelGap = 0.026f;

    float scale = kTextScale;
    float total = 0.0f;

    for (const Hint& hint: hintList) {
        const char* input = gamepad ? hint.gamepad : hint.keyboard;
        total += textWidth(input ? input : "", scale) + (kCapPad * 2.0f);
        total += kLabelGap + textWidth(hint.action ? hint.action : "", scale);
    }
    total += kGap * static_cast<float>(count - 1);

    // Keep the bar inside the screen on narrow layouts.
    if (total > 1.86f) {
        scale *= 1.86f / total;
        total = 0.0f;
        for (const Hint& hint: hintList) {
            const char* input = gamepad ? hint.gamepad : hint.keyboard;
            total += textWidth(input ? input : "", scale) + (kCapPad * 2.0f);
            total += kLabelGap + textWidth(hint.action ? hint.action : "", scale);
        }
        total += kGap * static_cast<float>(count - 1);
    }

    float x = -total * 0.5f;
    const float capColor[4] = { 1.0f, 0.82f, 0.35f, 0.95f };
    vector::pen actionPen(0.62f, 0.84f, 1.0f, 0.70f, 2.0f);

    for (const Hint& hint: hintList) {
        const char* input = gamepad ? hint.gamepad : hint.keyboard;
        if (!input)
            input = "";

        const float capWidth = textWidth(input, scale) + (kCapPad * 2.0f);
        cap(x + capWidth * 0.5f, y, input, scale, capColor);

        x += capWidth + kLabelGap;
        if (hint.action && *hint.action)
            font::AlphanumericsPrint(font::ALIGN_LEFT, scale, x, y, &actionPen, "%s", hint.action);
        x += textWidth(hint.action ? hint.action : "", scale) + kGap;
    }
}

} // namespace hints
