#include "evdev_to_keycode.h"
#include <array>
#include <linux/input-event-codes.h>

constexpr auto evdev_keyboard_to_keycode = []() {
    std::array<Keycode, 256> arr{Keycode::None};

    // Letters
    arr[KEY_A] = Keycode::A;
    arr[KEY_B] = Keycode::B;
    arr[KEY_C] = Keycode::C;
    arr[KEY_D] = Keycode::D;
    arr[KEY_E] = Keycode::E;
    arr[KEY_F] = Keycode::F;
    arr[KEY_G] = Keycode::G;
    arr[KEY_H] = Keycode::H;
    arr[KEY_I] = Keycode::I;
    arr[KEY_J] = Keycode::J;
    arr[KEY_K] = Keycode::K;
    arr[KEY_L] = Keycode::L;
    arr[KEY_M] = Keycode::M;
    arr[KEY_N] = Keycode::N;
    arr[KEY_O] = Keycode::O;
    arr[KEY_P] = Keycode::P;
    arr[KEY_Q] = Keycode::Q;
    arr[KEY_R] = Keycode::R;
    arr[KEY_S] = Keycode::S;
    arr[KEY_T] = Keycode::T;
    arr[KEY_U] = Keycode::U;
    arr[KEY_V] = Keycode::V;
    arr[KEY_W] = Keycode::W;
    arr[KEY_X] = Keycode::X;
    arr[KEY_Y] = Keycode::Y;
    arr[KEY_Z] = Keycode::Z;

    // Numbers
    arr[KEY_1] = Keycode::Num1;
    arr[KEY_2] = Keycode::Num2;
    arr[KEY_3] = Keycode::Num3;
    arr[KEY_4] = Keycode::Num4;
    arr[KEY_5] = Keycode::Num5;
    arr[KEY_6] = Keycode::Num6;
    arr[KEY_7] = Keycode::Num7;
    arr[KEY_8] = Keycode::Num8;
    arr[KEY_9] = Keycode::Num9;
    arr[KEY_0] = Keycode::Num0;

    // Function keys
    arr[KEY_F1] = Keycode::F1;
    arr[KEY_F2] = Keycode::F2;
    arr[KEY_F3] = Keycode::F3;
    arr[KEY_F4] = Keycode::F4;
    arr[KEY_F5] = Keycode::F5;
    arr[KEY_F6] = Keycode::F6;
    arr[KEY_F7] = Keycode::F7;
    arr[KEY_F8] = Keycode::F8;
    arr[KEY_F9] = Keycode::F9;
    arr[KEY_F10] = Keycode::F10;
    arr[KEY_F11] = Keycode::F11;
    arr[KEY_F12] = Keycode::F12;

    // Common controls
    arr[KEY_ESC] = Keycode::Escape;
    arr[KEY_TAB] = Keycode::Tab;
    arr[KEY_ENTER] = Keycode::Enter;
    arr[KEY_SPACE] = Keycode::Space;
    arr[KEY_BACKSPACE] = Keycode::Backspace;
    arr[KEY_CAPSLOCK] = Keycode::CapsLock;
    arr[KEY_LEFTSHIFT] = Keycode::LeftShift;
    arr[KEY_RIGHTSHIFT] = Keycode::RightShift;
    arr[KEY_LEFTCTRL] = Keycode::LeftControl;
    arr[KEY_RIGHTCTRL] = Keycode::RightControl;
    arr[KEY_LEFTALT] = Keycode::LeftAlt;
    arr[KEY_RIGHTALT] = Keycode::RightAlt;
    arr[KEY_LEFTMETA] = Keycode::LeftGUI;
    arr[KEY_RIGHTMETA] = Keycode::RightGUI;
    arr[KEY_MENU] = Keycode::Menu;

    // Symbols
    arr[KEY_MINUS] = Keycode::Minus;
    arr[KEY_EQUAL] = Keycode::Equal;
    arr[KEY_LEFTBRACE] = Keycode::LeftBracket;
    arr[KEY_RIGHTBRACE] = Keycode::RightBracket;
    arr[KEY_BACKSLASH] = Keycode::Backslash;
    arr[KEY_SEMICOLON] = Keycode::Semicolon;
    arr[KEY_APOSTROPHE] = Keycode::Apostrophe;
    arr[KEY_GRAVE] = Keycode::Grave;
    arr[KEY_COMMA] = Keycode::Comma;
    arr[KEY_DOT] = Keycode::Period;
    arr[KEY_SLASH] = Keycode::Slash;

    // Navigation
    arr[KEY_INSERT] = Keycode::Insert;
    arr[KEY_DELETE] = Keycode::Delete;
    arr[KEY_HOME] = Keycode::Home;
    arr[KEY_END] = Keycode::End;
    arr[KEY_PAGEUP] = Keycode::PageUp;
    arr[KEY_PAGEDOWN] = Keycode::PageDown;
    arr[KEY_UP] = Keycode::UpArrow;
    arr[KEY_DOWN] = Keycode::DownArrow;
    arr[KEY_LEFT] = Keycode::LeftArrow;
    arr[KEY_RIGHT] = Keycode::RightArrow;

    // Keypad
    arr[KEY_NUMLOCK] = Keycode::NumLock;
    arr[KEY_KPSLASH] = Keycode::KeypadSlash;
    arr[KEY_KPASTERISK] = Keycode::KeypadAsterisk;
    arr[KEY_KPMINUS] = Keycode::KeypadMinus;
    arr[KEY_KPPLUS] = Keycode::KeypadPlus;
    arr[KEY_KPENTER] = Keycode::KeypadEnter;
    arr[KEY_KP0] = Keycode::Keypad0;
    arr[KEY_KP1] = Keycode::Keypad1;
    arr[KEY_KP2] = Keycode::Keypad2;
    arr[KEY_KP3] = Keycode::Keypad3;
    arr[KEY_KP4] = Keycode::Keypad4;
    arr[KEY_KP5] = Keycode::Keypad5;
    arr[KEY_KP6] = Keycode::Keypad6;
    arr[KEY_KP7] = Keycode::Keypad7;
    arr[KEY_KP8] = Keycode::Keypad8;
    arr[KEY_KP9] = Keycode::Keypad9;
    arr[KEY_KPDOT] = Keycode::KeypadPeriod;

    // PrintScreen, ScrollLock, Pause/Break
    arr[KEY_SYSRQ] = Keycode::PrintScreen;
    arr[KEY_SCROLLLOCK] = Keycode::ScrollLock;
    arr[KEY_PAUSE] = Keycode::Pause;

    return arr;
}();

constexpr auto evdev_mouse_to_keycode = []() {
    std::array<Keycode, BTN_TASK - BTN_LEFT + 1> arr{Keycode::None};

    // Mouse buttons
    arr[BTN_LEFT    - BTN_LEFT] = Keycode::LeftClick;
    arr[BTN_RIGHT   - BTN_LEFT] = Keycode::RightClick;
    arr[BTN_MIDDLE  - BTN_LEFT] = Keycode::MiddleClick;
    arr[BTN_FORWARD - BTN_LEFT] = Keycode::MouseForward;
    arr[BTN_BACK    - BTN_LEFT] = Keycode::MouseBack;

    return arr;
}();

Keycode evdev_to_keycode(std::uint16_t code)
{
    if (code < 256)
        return evdev_keyboard_to_keycode[code];
    if (code >= BTN_LEFT && code <= BTN_TASK)
        return evdev_mouse_to_keycode[code - BTN_LEFT];
    return Keycode::None;
}
