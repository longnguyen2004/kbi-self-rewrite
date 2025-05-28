#include <keycode.h>
#include <unordered_map>
#include <string_view>

static std::string_view hid_keycode_strings[] = {
    "None", "ErrorRollOver", "POSTFail", "ErrorUndefined", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9", "Num0",
    "Enter", "Escape", "Backspace", "Tab", "Space", "Minus", "Equal", "LeftBracket", "RightBracket", "Backslash", "NonUSHash", "Semicolon", "Apostrophe", "Grave", "Comma", "Period", "Slash",
    "CapsLock", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "PrintScreen", "ScrollLock", "Pause", "Insert", "Home", "PageUp", "Delete", "End", "PageDown",
    "RightArrow", "LeftArrow", "DownArrow", "UpArrow", "NumLock", "KeypadSlash", "KeypadAsterisk", "KeypadMinus", "KeypadPlus", "KeypadEnter", "Keypad1", "Keypad2", "Keypad3", "Keypad4",
    "Keypad5", "Keypad6", "Keypad7", "Keypad8", "Keypad9", "Keypad0", "KeypadPeriod", "NonUSBackslash", "Application", "Power", "KeypadEqual", "F13", "F14", "F15", "F16", "F17", "F18",
    "F19", "F20", "F21", "F22", "F23", "F24", "Execute", "Help", "Menu", "Select", "Stop", "Again", "Undo", "Cut", "Copy", "Paste", "Find", "Mute", "VolumeUp", "VolumeDown",
    "LockingCapsLock", "LockingNumLock", "LockingScrollLock", "KeypadComma", "KeypadEqualSign", "International1", "International2", "International3", "International4", "International5",
    "International6", "International7", "International8", "International9", "LANG1", "LANG2", "LANG3", "LANG4", "LANG5", "LANG6", "LANG7", "LANG8", "LANG9", "AlternateErase", "SysReq",
    "Cancel", "Clear", "Prior", "Return", "Separator", "Out", "Oper", "ClearAgain", "CrSel", "ExSel",
};

static std::string_view hid_keycode_modifier_strings[] = {
    "LeftControl",
    "LeftShift",
    "LeftAlt",
    "LeftGUI",
    "RightControl",
    "RightShift",
    "RightAlt",
    "RightGUI",
};

static std::string_view mouse_buttons_strings[] = {
    "LeftClick",
    "RightClick",
    "MiddleClick",
    "MouseBack",
    "MouseForward",
};

static std::string_view gamepad_buttons_strings[] = {
    "Button1",
    "Button2",
    "Button3",
    "Button4",
    "Button5",
    "Button6",
    "Button7",
    "Button8",
    "Button9",
    "Button10",
    "Button11",
    "Button12",
    "Button13",
    "Button14",
    "Button15",
    "Button16",
    "Button17",
    "Button18",
    "Button19",
    "Button20",
    "Button21",
    "Button22",
    "Button23",
    "Button24",
    "Button25",
    "Button26",
    "Button27",
    "Button28",
    "Button29",
    "Button30",
    "Button31",
    "Button32",
    "HatN",
    "HatE",
    "HatS",
    "HatW",
};

std::string_view keycode_to_string(Keycode keycode) {
    if (keycode >= Keycode::Button1)
        return gamepad_buttons_strings[
            static_cast<int>(keycode) - static_cast<int>(Keycode::Button1)
        ];
    if (keycode >= Keycode::LeftClick)
        return mouse_buttons_strings[
            static_cast<int>(keycode) - static_cast<int>(Keycode::LeftClick)
        ];
    if (keycode >= Keycode::LeftControl)
        return hid_keycode_modifier_strings[
            static_cast<int>(keycode) - static_cast<int>(Keycode::LeftControl)
        ];
    return hid_keycode_strings[static_cast<int>(keycode)];
}
