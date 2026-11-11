#pragma once

#include "math/point3d.hpp"

#include <SDL3/SDL.h>

class controls
{
  public:
    // Which device most recently produced input. Menus use this (together
    // with settings::mPromptStyle) to choose keyboard or gamepad prompts.
    enum class InputDevice
    {
        Keyboard,
        Gamepad
    };

    // Hot-plug notification queued by the SDL event path and consumed by the
    // game shell so it can show a short on-screen notice.
    struct DeviceEvent
    {
        enum class Type
        {
            Added,
            Removed
        };

        Type type;
        int slot;
        char name[64];
    };

    controls();
    ~controls();

    Point3d getLeftStick(int player);
    Point3d getRightStick(int player);
    bool getTriggerButton(int player);
    bool getStartButton(int player);
    bool getBackButton(int player);
    bool getPauseButton(int player);
    bool getOptionsButton(int player);

    // Standard gamepad menu buttons: South/A confirms, East/B cancels. The
    // keyboard equivalents (Enter/Space and Escape/Backspace) are included so
    // every screen can share one code path.
    bool getConfirmButton(int player);
    bool getCancelButton(int player);

    InputDevice lastInput() const { return mLastInput; }
    bool hasInput() const { return mHasInput; }
    int gamepadCount() const { return mNumGamepads; }
    bool gamepadConnected(int player) const;
    const char* gamepadName(int player) const;

    // Rumble the pad assigned to `player`. Values are 0..1; ignored when the
    // vibration setting is off. `ms` is the duration in milliseconds.
    void rumble(int player, float low, float high, int ms);
    void stopRumble(int player);

    // Hot-plugging support
    void handleGamepadAdded(SDL_JoystickID instanceId);
    void handleGamepadRemoved(SDL_JoystickID instanceId);
    void scanForGamepads();

    // Pop the next hot-plug event. Returns false when the queue is empty.
    bool pollDeviceEvent(DeviceEvent& event);

  private:
    // Keyboard
    Point3d readKeyboardLeftStick(int player);
    Point3d readKeyboardRightStick(int player);
    bool readKeyboardTrigger(int player);
    bool readKeyboardStart(int player);
    bool readKeyboardBack(int player);
    bool readKeyboardPause(int player);
    bool readKeyboardOptions(int player);
    bool readKeyboardConfirm(int player);
    bool readKeyboardCancel(int player);

    // Gamepads (modern API)
    Point3d readGamepadLeftStick(int player);
    Point3d readGamepadRightStick(int player);

    // D-pad read as a directional vector in the same layout as the left stick
    // (x = up/down, y = left/right), so both movement and menu navigation
    // (which poll the sticks) respond to it.
    Point3d readGamepadDpad(int player);

    bool readGamepadTrigger(int player);
    bool readGamepadStart(int player);
    bool readGamepadBack(int player);
    bool readGamepadPause(int player);
    bool readGamepadOptions(int player);
    bool readGamepadButton(int player, SDL_GamepadButton button);

    void noteKeyboard();
    void noteGamepad();
    void queueDeviceEvent(DeviceEvent::Type type, int slot, const char* name);

    static constexpr int MAX_GAMEPADS { 4 };
    static constexpr int MAX_DEVICE_EVENTS { 8 };

    SDL_Gamepad* mGamepads[MAX_GAMEPADS] {};
    int mNumGamepads { 0 };

    InputDevice mLastInput { InputDevice::Keyboard };
    bool mHasInput { false };

    DeviceEvent mDeviceEvents[MAX_DEVICE_EVENTS] {};
    int mDeviceEventHead { 0 };
    int mDeviceEventCount { 0 };
};
