#pragma once

#include <initializer_list>

// Shared, device-aware controller/keyboard prompt rendering.
//
// Every menu supplies the same actions with both input labels; the active
// prompt style in the settings (auto / gamepad / keyboard) decides which one is
// shown. Auto follows the most recently used device, so the display switches as
// soon as the player touches a controller or the keyboard.
namespace hints {

struct Hint
{
    const char* action;
    const char* keyboard;
    const char* gamepad;
};

// True when prompts should use gamepad wording.
bool gamepadMode();

// Draw a centred row of [input] ACTION chips at `y` in 2D (NDC) space.
// Assumes additive blending and the 2D projection are already active.
void bar(std::initializer_list<Hint> hints, float y = -0.92f);

// Draw a single button/key cap with its label. Used by menus and the gameplay
// onboarding overlay.
void cap(float x, float y, const char* label, float scale, const float rgba[4]);

// Width of `text` at `scale`, in the same NDC units the font renderer uses.
float textWidth(const char* text, float scale);

} // namespace hints
