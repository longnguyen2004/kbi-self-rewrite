#include "core/keycode_util.h"

#include <keycode.h>

#include <array>
#include <unordered_map>

namespace kbi {
namespace {

// Windows virtual-key codes, inlined from WinUser.h.
enum : int {
    VK_BACK = 0x08,
    VK_TAB = 0x09,
    VK_RETURN = 0x0d,
    VK_PAUSE = 0x13,
    VK_CAPITAL = 0x14,
    VK_ESCAPE = 0x1b,
    VK_SPACE = 0x20,
    VK_PRIOR = 0x21,
    VK_NEXT = 0x22,
    VK_END = 0x23,
    VK_HOME = 0x24,
    VK_LEFT = 0x25,
    VK_UP = 0x26,
    VK_RIGHT = 0x27,
    VK_DOWN = 0x28,
    VK_SNAPSHOT = 0x2c,
    VK_INSERT = 0x2d,
    VK_DELETE = 0x2e,
    VK_LWIN = 0x5b,
    VK_RWIN = 0x5c,
    VK_APPS = 0x5d,
    VK_NUMPAD0 = 0x60,
    VK_NUMPAD1 = 0x61,
    VK_NUMPAD2 = 0x62,
    VK_NUMPAD3 = 0x63,
    VK_NUMPAD4 = 0x64,
    VK_NUMPAD5 = 0x65,
    VK_NUMPAD6 = 0x66,
    VK_NUMPAD7 = 0x67,
    VK_NUMPAD8 = 0x68,
    VK_NUMPAD9 = 0x69,
    VK_MULTIPLY = 0x6a,
    VK_ADD = 0x6b,
    VK_SEPARATOR = 0x6c,
    VK_SUBTRACT = 0x6d,
    VK_DECIMAL = 0x6e,
    VK_DIVIDE = 0x6f,
    VK_F1 = 0x70,
    VK_F2 = 0x71,
    VK_F3 = 0x72,
    VK_F4 = 0x73,
    VK_F5 = 0x74,
    VK_F6 = 0x75,
    VK_F7 = 0x76,
    VK_F8 = 0x77,
    VK_F9 = 0x78,
    VK_F10 = 0x79,
    VK_F11 = 0x7a,
    VK_F12 = 0x7b,
    VK_F13 = 0x7c,
    VK_F14 = 0x7d,
    VK_F15 = 0x7e,
    VK_F16 = 0x7f,
    VK_F17 = 0x80,
    VK_F18 = 0x81,
    VK_F19 = 0x82,
    VK_F20 = 0x83,
    VK_F21 = 0x84,
    VK_F22 = 0x85,
    VK_F23 = 0x86,
    VK_F24 = 0x87,
    VK_NUMLOCK = 0x90,
    VK_SCROLL = 0x91,
    VK_LSHIFT = 0xa0,
    VK_RSHIFT = 0xa1,
    VK_LCONTROL = 0xa2,
    VK_RCONTROL = 0xa3,
    VK_LMENU = 0xa4,
    VK_RMENU = 0xa5,
    VK_OEM_1 = 0xba,
    VK_OEM_PLUS = 0xbb,
    VK_OEM_COMMA = 0xbc,
    VK_OEM_MINUS = 0xbd,
    VK_OEM_PERIOD = 0xbe,
    VK_OEM_2 = 0xbf,
    VK_OEM_3 = 0xc0,
    VK_OEM_4 = 0xdb,
    VK_OEM_5 = 0xdc,
    VK_OEM_6 = 0xdd,
    VK_OEM_7 = 0xde,
};

std::array<int, 256> build_vk_table() {
    std::array<int, 256> table{};
    table.fill(static_cast<int>(Keycode::None));

    table[VK_BACK] = static_cast<int>(Keycode::Backspace);
    table[VK_TAB] = static_cast<int>(Keycode::Tab);
    table[VK_RETURN] = static_cast<int>(Keycode::Enter);
    table[VK_PAUSE] = static_cast<int>(Keycode::Pause);
    table[VK_CAPITAL] = static_cast<int>(Keycode::CapsLock);
    table[VK_ESCAPE] = static_cast<int>(Keycode::Escape);
    table[VK_SPACE] = static_cast<int>(Keycode::Space);
    table[VK_PRIOR] = static_cast<int>(Keycode::PageUp);
    table[VK_NEXT] = static_cast<int>(Keycode::PageDown);
    table[VK_END] = static_cast<int>(Keycode::End);
    table[VK_HOME] = static_cast<int>(Keycode::Home);
    table[VK_LEFT] = static_cast<int>(Keycode::LeftArrow);
    table[VK_UP] = static_cast<int>(Keycode::UpArrow);
    table[VK_RIGHT] = static_cast<int>(Keycode::RightArrow);
    table[VK_DOWN] = static_cast<int>(Keycode::DownArrow);
    table[VK_SNAPSHOT] = static_cast<int>(Keycode::PrintScreen);
    table[VK_INSERT] = static_cast<int>(Keycode::Insert);
    table[VK_DELETE] = static_cast<int>(Keycode::Delete);

    table[0x30] = static_cast<int>(Keycode::Num0);
    table[0x31] = static_cast<int>(Keycode::Num1);
    table[0x32] = static_cast<int>(Keycode::Num2);
    table[0x33] = static_cast<int>(Keycode::Num3);
    table[0x34] = static_cast<int>(Keycode::Num4);
    table[0x35] = static_cast<int>(Keycode::Num5);
    table[0x36] = static_cast<int>(Keycode::Num6);
    table[0x37] = static_cast<int>(Keycode::Num7);
    table[0x38] = static_cast<int>(Keycode::Num8);
    table[0x39] = static_cast<int>(Keycode::Num9);

    table[0x41] = static_cast<int>(Keycode::A);
    table[0x42] = static_cast<int>(Keycode::B);
    table[0x43] = static_cast<int>(Keycode::C);
    table[0x44] = static_cast<int>(Keycode::D);
    table[0x45] = static_cast<int>(Keycode::E);
    table[0x46] = static_cast<int>(Keycode::F);
    table[0x47] = static_cast<int>(Keycode::G);
    table[0x48] = static_cast<int>(Keycode::H);
    table[0x49] = static_cast<int>(Keycode::I);
    table[0x4a] = static_cast<int>(Keycode::J);
    table[0x4b] = static_cast<int>(Keycode::K);
    table[0x4c] = static_cast<int>(Keycode::L);
    table[0x4d] = static_cast<int>(Keycode::M);
    table[0x4e] = static_cast<int>(Keycode::N);
    table[0x4f] = static_cast<int>(Keycode::O);
    table[0x50] = static_cast<int>(Keycode::P);
    table[0x51] = static_cast<int>(Keycode::Q);
    table[0x52] = static_cast<int>(Keycode::R);
    table[0x53] = static_cast<int>(Keycode::S);
    table[0x54] = static_cast<int>(Keycode::T);
    table[0x55] = static_cast<int>(Keycode::U);
    table[0x56] = static_cast<int>(Keycode::V);
    table[0x57] = static_cast<int>(Keycode::W);
    table[0x58] = static_cast<int>(Keycode::X);
    table[0x59] = static_cast<int>(Keycode::Y);
    table[0x5a] = static_cast<int>(Keycode::Z);

    table[VK_LWIN] = static_cast<int>(Keycode::LeftGUI);
    table[VK_RWIN] = static_cast<int>(Keycode::RightGUI);
    table[VK_APPS] = static_cast<int>(Keycode::Application);

    table[VK_NUMPAD0] = static_cast<int>(Keycode::Keypad0);
    table[VK_NUMPAD1] = static_cast<int>(Keycode::Keypad1);
    table[VK_NUMPAD2] = static_cast<int>(Keycode::Keypad2);
    table[VK_NUMPAD3] = static_cast<int>(Keycode::Keypad3);
    table[VK_NUMPAD4] = static_cast<int>(Keycode::Keypad4);
    table[VK_NUMPAD5] = static_cast<int>(Keycode::Keypad5);
    table[VK_NUMPAD6] = static_cast<int>(Keycode::Keypad6);
    table[VK_NUMPAD7] = static_cast<int>(Keycode::Keypad7);
    table[VK_NUMPAD8] = static_cast<int>(Keycode::Keypad8);
    table[VK_NUMPAD9] = static_cast<int>(Keycode::Keypad9);
    table[VK_MULTIPLY] = static_cast<int>(Keycode::KeypadAsterisk);
    table[VK_ADD] = static_cast<int>(Keycode::KeypadPlus);
    table[VK_SEPARATOR] = static_cast<int>(Keycode::KeypadComma);
    table[VK_SUBTRACT] = static_cast<int>(Keycode::KeypadMinus);
    table[VK_DECIMAL] = static_cast<int>(Keycode::KeypadPeriod);
    table[VK_DIVIDE] = static_cast<int>(Keycode::KeypadSlash);

    table[VK_F1] = static_cast<int>(Keycode::F1);
    table[VK_F2] = static_cast<int>(Keycode::F2);
    table[VK_F3] = static_cast<int>(Keycode::F3);
    table[VK_F4] = static_cast<int>(Keycode::F4);
    table[VK_F5] = static_cast<int>(Keycode::F5);
    table[VK_F6] = static_cast<int>(Keycode::F6);
    table[VK_F7] = static_cast<int>(Keycode::F7);
    table[VK_F8] = static_cast<int>(Keycode::F8);
    table[VK_F9] = static_cast<int>(Keycode::F9);
    table[VK_F10] = static_cast<int>(Keycode::F10);
    table[VK_F11] = static_cast<int>(Keycode::F11);
    table[VK_F12] = static_cast<int>(Keycode::F12);
    table[VK_F13] = static_cast<int>(Keycode::F13);
    table[VK_F14] = static_cast<int>(Keycode::F14);
    table[VK_F15] = static_cast<int>(Keycode::F15);
    table[VK_F16] = static_cast<int>(Keycode::F16);
    table[VK_F17] = static_cast<int>(Keycode::F17);
    table[VK_F18] = static_cast<int>(Keycode::F18);
    table[VK_F19] = static_cast<int>(Keycode::F19);
    table[VK_F20] = static_cast<int>(Keycode::F20);
    table[VK_F21] = static_cast<int>(Keycode::F21);
    table[VK_F22] = static_cast<int>(Keycode::F22);
    table[VK_F23] = static_cast<int>(Keycode::F23);
    table[VK_F24] = static_cast<int>(Keycode::F24);

    table[VK_NUMLOCK] = static_cast<int>(Keycode::NumLock);
    table[VK_SCROLL] = static_cast<int>(Keycode::ScrollLock);

    table[VK_LSHIFT] = static_cast<int>(Keycode::LeftShift);
    table[VK_RSHIFT] = static_cast<int>(Keycode::RightShift);
    table[VK_LCONTROL] = static_cast<int>(Keycode::LeftControl);
    table[VK_RCONTROL] = static_cast<int>(Keycode::RightControl);
    table[VK_LMENU] = static_cast<int>(Keycode::LeftAlt);
    table[VK_RMENU] = static_cast<int>(Keycode::RightAlt);

    table[VK_OEM_1] = static_cast<int>(Keycode::Semicolon);
    table[VK_OEM_PLUS] = static_cast<int>(Keycode::Equal);
    table[VK_OEM_COMMA] = static_cast<int>(Keycode::Comma);
    table[VK_OEM_MINUS] = static_cast<int>(Keycode::Minus);
    table[VK_OEM_PERIOD] = static_cast<int>(Keycode::Period);
    table[VK_OEM_2] = static_cast<int>(Keycode::Slash);
    table[VK_OEM_3] = static_cast<int>(Keycode::Grave);
    table[VK_OEM_4] = static_cast<int>(Keycode::LeftBracket);
    table[VK_OEM_5] = static_cast<int>(Keycode::Backslash);
    table[VK_OEM_6] = static_cast<int>(Keycode::RightBracket);
    table[VK_OEM_7] = static_cast<int>(Keycode::Apostrophe);

    return table;
}

const std::array<int, 256>& vk_table() {
    static const std::array<int, 256> table = build_vk_table();
    return table;
}

const std::unordered_map<std::string_view, int>& key_name_table() {
    static const std::unordered_map<std::string_view, int> table = {
        // Letters
        {"A", 0x41}, {"B", 0x42}, {"C", 0x43}, {"D", 0x44}, {"E", 0x45},
        {"F", 0x46}, {"G", 0x47}, {"H", 0x48}, {"I", 0x49}, {"J", 0x4a},
        {"K", 0x4b}, {"L", 0x4c}, {"M", 0x4d}, {"N", 0x4e}, {"O", 0x4f},
        {"P", 0x50}, {"Q", 0x51}, {"R", 0x52}, {"S", 0x53}, {"T", 0x54},
        {"U", 0x55}, {"V", 0x56}, {"W", 0x57}, {"X", 0x58}, {"Y", 0x59},
        {"Z", 0x5a},
        // Numbers
        {"D0", 0x30}, {"D1", 0x31}, {"D2", 0x32}, {"D3", 0x33}, {"D4", 0x34},
        {"D5", 0x35}, {"D6", 0x36}, {"D7", 0x37}, {"D8", 0x38}, {"D9", 0x39},
        // Function keys
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
        {"F13", VK_F13}, {"F14", VK_F14}, {"F15", VK_F15}, {"F16", VK_F16},
        {"F17", VK_F17}, {"F18", VK_F18}, {"F19", VK_F19}, {"F20", VK_F20},
        {"F21", VK_F21}, {"F22", VK_F22}, {"F23", VK_F23}, {"F24", VK_F24},
        // Control keys
        {"Back", VK_BACK}, {"Tab", VK_TAB}, {"Enter", VK_RETURN},
        {"Return", VK_RETURN}, {"Pause", VK_PAUSE}, {"Capital", VK_CAPITAL},
        {"CapsLock", VK_CAPITAL}, {"Escape", VK_ESCAPE}, {"Space", VK_SPACE},
        {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT}, {"End", VK_END},
        {"Home", VK_HOME}, {"Left", VK_LEFT}, {"Up", VK_UP}, {"Right", VK_RIGHT},
        {"Down", VK_DOWN}, {"PrintScreen", VK_SNAPSHOT}, {"Snapshot", VK_SNAPSHOT},
        {"Insert", VK_INSERT}, {"Delete", VK_DELETE}, {"LWin", VK_LWIN},
        {"RWin", VK_RWIN}, {"Apps", VK_APPS}, {"NumLock", VK_NUMLOCK},
        {"Scroll", VK_SCROLL},
        // Numpad
        {"NumPad0", VK_NUMPAD0}, {"NumPad1", VK_NUMPAD1}, {"NumPad2", VK_NUMPAD2},
        {"NumPad3", VK_NUMPAD3}, {"NumPad4", VK_NUMPAD4}, {"NumPad5", VK_NUMPAD5},
        {"NumPad6", VK_NUMPAD6}, {"NumPad7", VK_NUMPAD7}, {"NumPad8", VK_NUMPAD8},
        {"NumPad9", VK_NUMPAD9}, {"Multiply", VK_MULTIPLY}, {"Add", VK_ADD},
        {"Separator", VK_SEPARATOR}, {"Subtract", VK_SUBTRACT},
        {"Decimal", VK_DECIMAL}, {"Divide", VK_DIVIDE},
        // Modifiers
        {"LShiftKey", VK_LSHIFT}, {"RShiftKey", VK_RSHIFT},
        {"LControlKey", VK_LCONTROL}, {"RControlKey", VK_RCONTROL},
        {"LMenu", VK_LMENU}, {"RMenu", VK_RMENU},
        // OEM keys
        {"OemSemicolon", VK_OEM_1}, {"Oem1", VK_OEM_1},
        {"Oemplus", VK_OEM_PLUS}, {"Oemcomma", VK_OEM_COMMA},
        {"OemMinus", VK_OEM_MINUS}, {"OemPeriod", VK_OEM_PERIOD},
        {"OemQuestion", VK_OEM_2}, {"Oem2", VK_OEM_2},
        {"Oemtilde", VK_OEM_3}, {"Oem3", VK_OEM_3},
        {"OemOpenBrackets", VK_OEM_4}, {"Oem4", VK_OEM_4},
        {"OemPipe", VK_OEM_5}, {"Oem5", VK_OEM_5},
        {"OemCloseBrackets", VK_OEM_6}, {"Oem6", VK_OEM_6},
        {"OemQuotes", VK_OEM_7}, {"Oem7", VK_OEM_7},
    };
    return table;
}

} // namespace

std::string_view keycode_name(int code) {
    if (code < 0)
        return "Unknown";
    if (code >= static_cast<int>(Keycode::Button1) &&
        code <= static_cast<int>(Keycode::HatW))
        return keycode_to_string(static_cast<Keycode>(code));
    if (code >= static_cast<int>(Keycode::LeftClick) &&
        code <= static_cast<int>(Keycode::MouseForward))
        return keycode_to_string(static_cast<Keycode>(code));
    if (code >= static_cast<int>(Keycode::LeftControl) &&
        code <= static_cast<int>(Keycode::RightGUI))
        return keycode_to_string(static_cast<Keycode>(code));
    if (code >= 0 && code <= static_cast<int>(Keycode::ExSel))
        return keycode_to_string(static_cast<Keycode>(code));
    return "Unknown";
}

int vk_to_keycode(int vk) {
    if (vk < 0 || vk >= static_cast<int>(vk_table().size()))
        return static_cast<int>(Keycode::None);
    return vk_table()[static_cast<std::size_t>(vk)];
}

int key_name_to_vk(std::string_view name) {
    const auto& table = key_name_table();
    const auto it = table.find(name);
    if (it == table.end())
        return -1;
    return it->second;
}

} // namespace kbi
