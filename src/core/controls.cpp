#include "core/controls.hpp"

#include "core/settings.hpp"

#include <SDL3/SDL_joystick.h>

#include <cmath>
#include <cstdio>
#include <cstring>

extern const bool* keyboardState;

// CONTROLLER VALUES
constexpr float AXIS_MAX = 32768.f;
constexpr float TRIGGER_THRESHOLD = .25f;
// Radial deadzone applied to the analog sticks. Rescaling after the deadzone
// gives a continuous 0..1 magnitude so the ship can move at any speed between
// the deadzone and full deflection.
constexpr float STICK_DEADZONE = .20f;

namespace {

Point3d applyRadialDeadzone(float x, float y)
{
    const float magnitude = std::sqrt(x * x + y * y);
    if (magnitude <= STICK_DEADZONE)
        return Point3d(0, 0, 0);

    float scaled = (magnitude - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
    if (scaled > 1.0f)
        scaled = 1.0f;

    return Point3d((x / magnitude) * scaled, (y / magnitude) * scaled, 0);
}

Point3d clampStickMagnitude(Point3d v)
{
    const float magnitude = std::sqrt(v.x * v.x + v.y * v.y);
    if (magnitude > 1.0f) {
        v.x /= magnitude;
        v.y /= magnitude;
    }
    return v;
}

Uint16 rumbleAmount(float value)
{
    if (value <= 0.0f)
        return 0;
    if (value >= 1.0f)
        return 65535;
    return static_cast<Uint16>(value * 65535.0f);
}

} // namespace

controls::controls()
{
    printf("Initing controls\n");

    // Scan for initially connected gamepads
    scanForGamepads();
}

controls::~controls()
{
    for (auto& pad: mGamepads) {
        if (pad) {
            SDL_RumbleGamepad(pad, 0, 0, 0);
            SDL_CloseGamepad(pad);
            pad = nullptr;
        }
    }
}

void controls::scanForGamepads()
{
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);

    if (!joysticks) {
        printf("SDL_GetJoysticks() failed: %s\n", SDL_GetError());
    }

    printf("Scanning for gamepads... Found %d joysticks\n", count);

    // Open any available gamepads
    for (int j = 0; j < count && mNumGamepads < MAX_GAMEPADS; j++) {
        // SDL_IsGamepad()/SDL_OpenGamepad() take a joystick instance ID, not
        // an index into the list returned by SDL_GetJoysticks(). Passing the
        // raw loop index only worked for a pad whose ID happened to equal its
        // array position (e.g. the very first device); Bluetooth controllers
        // usually get non-zero/higher IDs and were silently never opened.
        SDL_JoystickID instanceId = joysticks[j];

        if (SDL_IsGamepad(instanceId)) {
            // Check if this device is already opened
            bool alreadyOpen = false;

            for (auto& pad: mGamepads) {
                if (pad) {
                    SDL_Joystick* joy = SDL_GetGamepadJoystick(pad);
                    if (SDL_GetJoystickID(joy) == instanceId) {
                        alreadyOpen = true;
                        break;
                    }
                }
            }

            if (!alreadyOpen) {
                // Find first free slot
                for (int i = 0; i < MAX_GAMEPADS; i++) {
                    if (!mGamepads[i]) {
                        mGamepads[i] = SDL_OpenGamepad(instanceId);
                        if (mGamepads[i]) {
                            printf("Gamepad %d: %s\n", i,
                                   SDL_GetGamepadName(mGamepads[i]));
                            mNumGamepads++;
                        } else {
                            printf("Failed to open gamepad (id=%u): %s\n",
                                   (unsigned)instanceId, SDL_GetError());
                        }
                        break;
                    }
                }
            }
        }
    }

    SDL_free(joysticks);

    printf("Total gamepads opened: %d\n", mNumGamepads);
}

void controls::handleGamepadAdded(SDL_JoystickID instanceId)
{
    if (!SDL_IsGamepad(instanceId))
        return;

    // Find first available slot
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (!mGamepads[i]) {
            mGamepads[i] = SDL_OpenGamepad(instanceId);
            if (mGamepads[i]) {
                const char* name = SDL_GetGamepadName(mGamepads[i]);
                printf("Gamepad connected in slot %d: %s\n", i,
                       name ? name : "Controller");
                mNumGamepads++;
                queueDeviceEvent(DeviceEvent::Type::Added, i,
                                 name ? name : "Controller");
                noteGamepad();
            } else {
                printf("Failed to open gamepad %d: %s\n", instanceId, SDL_GetError());
            }
            break;
        }
    }
}

void controls::handleGamepadRemoved(SDL_JoystickID instanceId)
{
    // Find and remove the disconnected gamepad
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (mGamepads[i]) {
            SDL_Joystick* joy = SDL_GetGamepadJoystick(mGamepads[i]);
            if (SDL_GetJoystickID(joy) == instanceId) {
                const char* name = SDL_GetGamepadName(mGamepads[i]);
                char copiedName[64];
                snprintf(copiedName, sizeof(copiedName), "%s",
                         name ? name : "Controller");
                printf("Gamepad disconnected from slot %d\n", i);
                SDL_RumbleGamepad(mGamepads[i], 0, 0, 0);
                SDL_CloseGamepad(mGamepads[i]);
                mGamepads[i] = nullptr;
                mNumGamepads--;
                queueDeviceEvent(DeviceEvent::Type::Removed, i, copiedName);
                break;
            }
        }
    }
}

Point3d controls::getLeftStick(int player)
{
    const Point3d keyboard = readKeyboardLeftStick(player);
    const Point3d gamepad = readGamepadLeftStick(player);

    if (keyboard.x != 0 || keyboard.y != 0)
        noteKeyboard();
    if (gamepad.x != 0 || gamepad.y != 0)
        noteGamepad();

    return keyboard + gamepad;
}

Point3d controls::getRightStick(int player)
{
    const Point3d keyboard = readKeyboardRightStick(player);
    const Point3d gamepad = readGamepadRightStick(player);

    if (keyboard.x != 0 || keyboard.y != 0)
        noteKeyboard();
    if (gamepad.x != 0 || gamepad.y != 0)
        noteGamepad();

    return keyboard + gamepad;
}

bool controls::getTriggerButton(int player)
{
    const bool keyboard = readKeyboardTrigger(player);
    const bool gamepad = readGamepadTrigger(player);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getStartButton(int player)
{
    const bool keyboard = readKeyboardStart(player);
    const bool gamepad = readGamepadStart(player);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getBackButton(int player)
{
    const bool keyboard = readKeyboardBack(player);
    const bool gamepad = readGamepadBack(player);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getPauseButton(int player)
{
    const bool keyboard = readKeyboardPause(player);
    const bool gamepad = readGamepadPause(player);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getOptionsButton(int player)
{
    const bool keyboard = readKeyboardOptions(player);
    const bool gamepad = readGamepadOptions(player);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getConfirmButton(int player)
{
    // The keyboard is a single device: only player 1 should react to it, so
    // player-select cannot join all four slots from one Space/Enter press.
    const bool keyboard = (player == 0) && readKeyboardConfirm(player);
    const bool gamepad = readGamepadButton(player, SDL_GAMEPAD_BUTTON_SOUTH);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::getCancelButton(int player)
{
    const bool keyboard = readKeyboardCancel(player);
    const bool gamepad = readGamepadButton(player, SDL_GAMEPAD_BUTTON_EAST);

    if (keyboard)
        noteKeyboard();
    if (gamepad)
        noteGamepad();

    return keyboard || gamepad;
}

bool controls::gamepadConnected(int player) const
{
    return player >= 0 && player < MAX_GAMEPADS && mGamepads[player] != nullptr;
}

const char* controls::gamepadName(int player) const
{
    if (!gamepadConnected(player))
        return nullptr;

    return SDL_GetGamepadName(mGamepads[player]);
}

void controls::rumble(int player, float low, float high, int ms)
{
    if (!settings::get().mVibration)
        return;
    if (!gamepadConnected(player))
        return;

    SDL_RumbleGamepad(mGamepads[player], rumbleAmount(low), rumbleAmount(high), static_cast<Uint32>(ms));
}

void controls::stopRumble(int player)
{
    if (!gamepadConnected(player))
        return;

    SDL_RumbleGamepad(mGamepads[player], 0, 0, 0);
}

bool controls::pollDeviceEvent(DeviceEvent& event)
{
    if (mDeviceEventCount <= 0)
        return false;

    event = mDeviceEvents[mDeviceEventHead];
    mDeviceEventHead = (mDeviceEventHead + 1) % MAX_DEVICE_EVENTS;
    --mDeviceEventCount;
    return true;
}

void controls::noteKeyboard()
{
    mLastInput = InputDevice::Keyboard;
    mHasInput = true;
}

void controls::noteGamepad()
{
    mLastInput = InputDevice::Gamepad;
    mHasInput = true;
}

void controls::queueDeviceEvent(DeviceEvent::Type type, int slot, const char* name)
{
    if (mDeviceEventCount >= MAX_DEVICE_EVENTS) {
        // Drop the oldest event rather than stalling the SDL event pump.
        mDeviceEventHead = (mDeviceEventHead + 1) % MAX_DEVICE_EVENTS;
        --mDeviceEventCount;
    }

    const int index = (mDeviceEventHead + mDeviceEventCount) % MAX_DEVICE_EVENTS;
    mDeviceEvents[index].type = type;
    mDeviceEvents[index].slot = slot;
    snprintf(mDeviceEvents[index].name, sizeof(mDeviceEvents[index].name), "%s",
             name ? name : "Controller");
    ++mDeviceEventCount;
}

//
// Keyboard controller
//
Point3d controls::readKeyboardLeftStick(int /*player*/)
{
    bool up = keyboardState[SDL_SCANCODE_W];
    bool down = keyboardState[SDL_SCANCODE_S];
    bool left = keyboardState[SDL_SCANCODE_A];
    bool right = keyboardState[SDL_SCANCODE_D];

    int x = 0;
    int y = 0;

    x += up ? 1 : 0;
    x -= down ? 1 : 0;

    y += left ? 1 : 0;
    y -= right ? 1 : 0;

    return Point3d(x, y, 0);
}

Point3d controls::readKeyboardRightStick(int /*player*/)
{
    bool up = keyboardState[SDL_SCANCODE_UP];
    bool down = keyboardState[SDL_SCANCODE_DOWN];
    bool left = keyboardState[SDL_SCANCODE_LEFT];
    bool right = keyboardState[SDL_SCANCODE_RIGHT];

    int x = 0;
    int y = 0;

    x += up ? 1 : 0;
    x -= down ? 1 : 0;

    y += left ? 1 : 0;
    y -= right ? 1 : 0;

    return Point3d(x, y, 0);
}

bool controls::readKeyboardTrigger(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_SPACE];
}

bool controls::readKeyboardStart(int player)
{
    switch (player) {
    case 0:
        return keyboardState[SDL_SCANCODE_1];
    case 1:
        return keyboardState[SDL_SCANCODE_2];
    case 2:
        return keyboardState[SDL_SCANCODE_3];
    case 3:
        return keyboardState[SDL_SCANCODE_4];
    }
    return false;
}

bool controls::readKeyboardBack(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_BACKSPACE];
}

bool controls::readKeyboardPause(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_P];
}

bool controls::readKeyboardOptions(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_O];
}

bool controls::readKeyboardConfirm(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_RETURN] || keyboardState[SDL_SCANCODE_SPACE];
}

bool controls::readKeyboardCancel(int /*player*/)
{
    return keyboardState[SDL_SCANCODE_ESCAPE] || keyboardState[SDL_SCANCODE_BACKSPACE];
}

//
// Modern SDL GameController API
//

Point3d controls::readGamepadLeftStick(int player)
{
    if (!mGamepads[player])
        return Point3d(0, 0, 0);

    // Note: Y axis is inverted for standard game controls
    const float x = -(SDL_GetGamepadAxis(mGamepads[player], SDL_GAMEPAD_AXIS_LEFTY)) / AXIS_MAX;
    const float y = -(SDL_GetGamepadAxis(mGamepads[player], SDL_GAMEPAD_AXIS_LEFTX)) / AXIS_MAX;

    // The D-pad shares the left stick's axis layout, so the menus (which poll
    // the sticks for navigation) and in-game movement both react to it.
    return clampStickMagnitude(applyRadialDeadzone(x, y) + readGamepadDpad(player));
}

Point3d controls::readGamepadRightStick(int player)
{
    if (!mGamepads[player])
        return Point3d(0, 0, 0);

    const float x = -(SDL_GetGamepadAxis(mGamepads[player], SDL_GAMEPAD_AXIS_RIGHTY)) / AXIS_MAX;
    const float y = -(SDL_GetGamepadAxis(mGamepads[player], SDL_GAMEPAD_AXIS_RIGHTX)) / AXIS_MAX;

    return applyRadialDeadzone(x, y);
}

Point3d controls::readGamepadDpad(int player)
{
    if (!mGamepads[player])
        return Point3d(0, 0, 0);

    Point3d vector;
    // The D-pad is a digital 4-way control; map each direction onto the same
    // axis convention used by the analog sticks (x up/down, y left/right).
    if (SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_DPAD_UP))
        vector.x += 1;
    if (SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_DPAD_DOWN))
        vector.x -= 1;
    if (SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_DPAD_LEFT))
        vector.y += 1;
    if (SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
        vector.y -= 1;

    return vector;
}

bool controls::readGamepadTrigger(int player)
{
    if (!mGamepads[player])
        return false;

    // Right trigger
    float triggerVal = (SDL_GetGamepadAxis(mGamepads[player], SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) / AXIS_MAX;

    return triggerVal > TRIGGER_THRESHOLD;
}

bool controls::readGamepadStart(int player)
{
    if (!mGamepads[player])
        return false;

    return SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_START);
}

bool controls::readGamepadBack(int player)
{
    if (!mGamepads[player])
        return false;

    return SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_BACK);
}

bool controls::readGamepadPause(int player)
{
    if (!mGamepads[player])
        return false;

    return SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_START);
}

bool controls::readGamepadOptions(int player)
{
    if (!mGamepads[player])
        return false;

    // Standard gamepad UI uses the Back button for options/cancel; the Start
    // button is reserved for launching and pausing.
    return SDL_GetGamepadButton(mGamepads[player], SDL_GAMEPAD_BUTTON_BACK);
}

bool controls::readGamepadButton(int player, SDL_GamepadButton button)
{
    if (!mGamepads[player])
        return false;

    return SDL_GetGamepadButton(mGamepads[player], button);
}
