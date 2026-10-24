#include "core/keycode_util.h"

#include <keycode.h>

#include <array>
#include <unordered_map>
#include <utility>

namespace Kbi {
namespace {

// Windows virtual-key codes, inlined from WinUser.h.
enum : int {
    VkBack = 0x08,
    VkTab = 0x09,
    VkReturn = 0x0d,
    VkPause = 0x13,
    VkCapital = 0x14,
    VkEscape = 0x1b,
    VkSpace = 0x20,
    VkPrior = 0x21,
    VkNext = 0x22,
    VkEnd = 0x23,
    VkHome = 0x24,
    VkLeft = 0x25,
    VkUp = 0x26,
    VkRight = 0x27,
    VkDown = 0x28,
    VkSnapshot = 0x2c,
    VkInsert = 0x2d,
    VkDelete = 0x2e,
    VkLwin = 0x5b,
    VkRwin = 0x5c,
    VkApps = 0x5d,
    VkNumpaD0 = 0x60,
    VkNumpaD1 = 0x61,
    VkNumpaD2 = 0x62,
    VkNumpaD3 = 0x63,
    VkNumpaD4 = 0x64,
    VkNumpaD5 = 0x65,
    VkNumpaD6 = 0x66,
    VkNumpaD7 = 0x67,
    VkNumpaD8 = 0x68,
    VkNumpaD9 = 0x69,
    VkMultiply = 0x6a,
    VkAdd = 0x6b,
    VkSeparator = 0x6c,
    VkSubtract = 0x6d,
    VkDecimal = 0x6e,
    VkDivide = 0x6f,
    VkF1 = 0x70,
    VkF2 = 0x71,
    VkF3 = 0x72,
    VkF4 = 0x73,
    VkF5 = 0x74,
    VkF6 = 0x75,
    VkF7 = 0x76,
    VkF8 = 0x77,
    VkF9 = 0x78,
    VkF10 = 0x79,
    VkF11 = 0x7a,
    VkF12 = 0x7b,
    VkF13 = 0x7c,
    VkF14 = 0x7d,
    VkF15 = 0x7e,
    VkF16 = 0x7f,
    VkF17 = 0x80,
    VkF18 = 0x81,
    VkF19 = 0x82,
    VkF20 = 0x83,
    VkF21 = 0x84,
    VkF22 = 0x85,
    VkF23 = 0x86,
    VkF24 = 0x87,
    VkNumlock = 0x90,
    VkScroll = 0x91,
    VkLshift = 0xa0,
    VkRshift = 0xa1,
    VkLcontrol = 0xa2,
    VkRcontrol = 0xa3,
    VkLmenu = 0xa4,
    VkRmenu = 0xa5,
    VkOem1 = 0xba,
    VkOemPlus = 0xbb,
    VkOemComma = 0xbc,
    VkOemMinus = 0xbd,
    VkOemPeriod = 0xbe,
    VkOem2 = 0xbf,
    VkOem3 = 0xc0,
    VkOem4 = 0xdb,
    VkOem5 = 0xdc,
    VkOem6 = 0xdd,
    VkOem7 = 0xde,
};

std::array<int, 256> buildVkTable()
{
    std::array<int, 256> table {};
    table.fill(static_cast<int>(Keycode::None));

    table[VkBack] = static_cast<int>(Keycode::Backspace);
    table[VkTab] = static_cast<int>(Keycode::Tab);
    table[VkReturn] = static_cast<int>(Keycode::Enter);
    table[VkPause] = static_cast<int>(Keycode::Pause);
    table[VkCapital] = static_cast<int>(Keycode::CapsLock);
    table[VkEscape] = static_cast<int>(Keycode::Escape);
    table[VkSpace] = static_cast<int>(Keycode::Space);
    table[VkPrior] = static_cast<int>(Keycode::PageUp);
    table[VkNext] = static_cast<int>(Keycode::PageDown);
    table[VkEnd] = static_cast<int>(Keycode::End);
    table[VkHome] = static_cast<int>(Keycode::Home);
    table[VkLeft] = static_cast<int>(Keycode::LeftArrow);
    table[VkUp] = static_cast<int>(Keycode::UpArrow);
    table[VkRight] = static_cast<int>(Keycode::RightArrow);
    table[VkDown] = static_cast<int>(Keycode::DownArrow);
    table[VkSnapshot] = static_cast<int>(Keycode::PrintScreen);
    table[VkInsert] = static_cast<int>(Keycode::Insert);
    table[VkDelete] = static_cast<int>(Keycode::Delete);

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

    table[VkLwin] = static_cast<int>(Keycode::LeftGui);
    table[VkRwin] = static_cast<int>(Keycode::RightGui);
    table[VkApps] = static_cast<int>(Keycode::Application);

    table[VkNumpaD0] = static_cast<int>(Keycode::Keypad0);
    table[VkNumpaD1] = static_cast<int>(Keycode::Keypad1);
    table[VkNumpaD2] = static_cast<int>(Keycode::Keypad2);
    table[VkNumpaD3] = static_cast<int>(Keycode::Keypad3);
    table[VkNumpaD4] = static_cast<int>(Keycode::Keypad4);
    table[VkNumpaD5] = static_cast<int>(Keycode::Keypad5);
    table[VkNumpaD6] = static_cast<int>(Keycode::Keypad6);
    table[VkNumpaD7] = static_cast<int>(Keycode::Keypad7);
    table[VkNumpaD8] = static_cast<int>(Keycode::Keypad8);
    table[VkNumpaD9] = static_cast<int>(Keycode::Keypad9);
    table[VkMultiply] = static_cast<int>(Keycode::KeypadAsterisk);
    table[VkAdd] = static_cast<int>(Keycode::KeypadPlus);
    table[VkSeparator] = static_cast<int>(Keycode::KeypadComma);
    table[VkSubtract] = static_cast<int>(Keycode::KeypadMinus);
    table[VkDecimal] = static_cast<int>(Keycode::KeypadPeriod);
    table[VkDivide] = static_cast<int>(Keycode::KeypadSlash);

    table[VkF1] = static_cast<int>(Keycode::F1);
    table[VkF2] = static_cast<int>(Keycode::F2);
    table[VkF3] = static_cast<int>(Keycode::F3);
    table[VkF4] = static_cast<int>(Keycode::F4);
    table[VkF5] = static_cast<int>(Keycode::F5);
    table[VkF6] = static_cast<int>(Keycode::F6);
    table[VkF7] = static_cast<int>(Keycode::F7);
    table[VkF8] = static_cast<int>(Keycode::F8);
    table[VkF9] = static_cast<int>(Keycode::F9);
    table[VkF10] = static_cast<int>(Keycode::F10);
    table[VkF11] = static_cast<int>(Keycode::F11);
    table[VkF12] = static_cast<int>(Keycode::F12);
    table[VkF13] = static_cast<int>(Keycode::F13);
    table[VkF14] = static_cast<int>(Keycode::F14);
    table[VkF15] = static_cast<int>(Keycode::F15);
    table[VkF16] = static_cast<int>(Keycode::F16);
    table[VkF17] = static_cast<int>(Keycode::F17);
    table[VkF18] = static_cast<int>(Keycode::F18);
    table[VkF19] = static_cast<int>(Keycode::F19);
    table[VkF20] = static_cast<int>(Keycode::F20);
    table[VkF21] = static_cast<int>(Keycode::F21);
    table[VkF22] = static_cast<int>(Keycode::F22);
    table[VkF23] = static_cast<int>(Keycode::F23);
    table[VkF24] = static_cast<int>(Keycode::F24);

    table[VkNumlock] = static_cast<int>(Keycode::NumLock);
    table[VkScroll] = static_cast<int>(Keycode::ScrollLock);

    table[VkLshift] = static_cast<int>(Keycode::LeftShift);
    table[VkRshift] = static_cast<int>(Keycode::RightShift);
    table[VkLcontrol] = static_cast<int>(Keycode::LeftControl);
    table[VkRcontrol] = static_cast<int>(Keycode::RightControl);
    table[VkLmenu] = static_cast<int>(Keycode::LeftAlt);
    table[VkRmenu] = static_cast<int>(Keycode::RightAlt);

    table[VkOem1] = static_cast<int>(Keycode::Semicolon);
    table[VkOemPlus] = static_cast<int>(Keycode::Equal);
    table[VkOemComma] = static_cast<int>(Keycode::Comma);
    table[VkOemMinus] = static_cast<int>(Keycode::Minus);
    table[VkOemPeriod] = static_cast<int>(Keycode::Period);
    table[VkOem2] = static_cast<int>(Keycode::Slash);
    table[VkOem3] = static_cast<int>(Keycode::Grave);
    table[VkOem4] = static_cast<int>(Keycode::LeftBracket);
    table[VkOem5] = static_cast<int>(Keycode::Backslash);
    table[VkOem6] = static_cast<int>(Keycode::RightBracket);
    table[VkOem7] = static_cast<int>(Keycode::Apostrophe);

    return table;
}

const std::array<int, 256>& vkTable()
{
    static const std::array<int, 256> table = buildVkTable();
    return table;
}

const std::unordered_map<std::string_view, int>& keyNameTable()
{
    static const std::unordered_map<std::string_view, int> table = {
        // Letters
        { "A", 0x41 },
        { "B", 0x42 },
        { "C", 0x43 },
        { "D", 0x44 },
        { "E", 0x45 },
        { "F", 0x46 },
        { "G", 0x47 },
        { "H", 0x48 },
        { "I", 0x49 },
        { "J", 0x4a },
        { "K", 0x4b },
        { "L", 0x4c },
        { "M", 0x4d },
        { "N", 0x4e },
        { "O", 0x4f },
        { "P", 0x50 },
        { "Q", 0x51 },
        { "R", 0x52 },
        { "S", 0x53 },
        { "T", 0x54 },
        { "U", 0x55 },
        { "V", 0x56 },
        { "W", 0x57 },
        { "X", 0x58 },
        { "Y", 0x59 },
        { "Z", 0x5a },
        // Numbers
        { "D0", 0x30 },
        { "D1", 0x31 },
        { "D2", 0x32 },
        { "D3", 0x33 },
        { "D4", 0x34 },
        { "D5", 0x35 },
        { "D6", 0x36 },
        { "D7", 0x37 },
        { "D8", 0x38 },
        { "D9", 0x39 },
        // Function keys
        { "F1", VkF1 },
        { "F2", VkF2 },
        { "F3", VkF3 },
        { "F4", VkF4 },
        { "F5", VkF5 },
        { "F6", VkF6 },
        { "F7", VkF7 },
        { "F8", VkF8 },
        { "F9", VkF9 },
        { "F10", VkF10 },
        { "F11", VkF11 },
        { "F12", VkF12 },
        { "F13", VkF13 },
        { "F14", VkF14 },
        { "F15", VkF15 },
        { "F16", VkF16 },
        { "F17", VkF17 },
        { "F18", VkF18 },
        { "F19", VkF19 },
        { "F20", VkF20 },
        { "F21", VkF21 },
        { "F22", VkF22 },
        { "F23", VkF23 },
        { "F24", VkF24 },
        // Control keys
        { "Back", VkBack },
        { "Tab", VkTab },
        { "Enter", VkReturn },
        { "Return", VkReturn },
        { "Pause", VkPause },
        { "Capital", VkCapital },
        { "CapsLock", VkCapital },
        { "Escape", VkEscape },
        { "Space", VkSpace },
        { "PageUp", VkPrior },
        { "PageDown", VkNext },
        { "End", VkEnd },
        { "Home", VkHome },
        { "Left", VkLeft },
        { "Up", VkUp },
        { "Right", VkRight },
        { "Down", VkDown },
        { "PrintScreen", VkSnapshot },
        { "Snapshot", VkSnapshot },
        { "Insert", VkInsert },
        { "Delete", VkDelete },
        { "LWin", VkLwin },
        { "RWin", VkRwin },
        { "Apps", VkApps },
        { "NumLock", VkNumlock },
        { "Scroll", VkScroll },
        // Numpad
        { "NumPad0", VkNumpaD0 },
        { "NumPad1", VkNumpaD1 },
        { "NumPad2", VkNumpaD2 },
        { "NumPad3", VkNumpaD3 },
        { "NumPad4", VkNumpaD4 },
        { "NumPad5", VkNumpaD5 },
        { "NumPad6", VkNumpaD6 },
        { "NumPad7", VkNumpaD7 },
        { "NumPad8", VkNumpaD8 },
        { "NumPad9", VkNumpaD9 },
        { "Multiply", VkMultiply },
        { "Add", VkAdd },
        { "Separator", VkSeparator },
        { "Subtract", VkSubtract },
        { "Decimal", VkDecimal },
        { "Divide", VkDivide },
        // Modifiers
        { "LShiftKey", VkLshift },
        { "RShiftKey", VkRshift },
        { "LControlKey", VkLcontrol },
        { "RControlKey", VkRcontrol },
        { "LMenu", VkLmenu },
        { "RMenu", VkRmenu },
        // OEM keys
        { "OemSemicolon", VkOem1 },
        { "Oem1", VkOem1 },
        { "Oemplus", VkOemPlus },
        { "Oemcomma", VkOemComma },
        { "OemMinus", VkOemMinus },
        { "OemPeriod", VkOemPeriod },
        { "OemQuestion", VkOem2 },
        { "Oem2", VkOem2 },
        { "Oemtilde", VkOem3 },
        { "Oem3", VkOem3 },
        { "OemOpenBrackets", VkOem4 },
        { "Oem4", VkOem4 },
        { "OemPipe", VkOem5 },
        { "Oem5", VkOem5 },
        { "OemCloseBrackets", VkOem6 },
        { "Oem6", VkOem6 },
        { "OemQuotes", VkOem7 },
        { "Oem7", VkOem7 },
    };
    return table;
}

} // namespace

std::string_view keycodeName(int code)
{
    if (code < 0)
        return "Unknown";
    if (code >= static_cast<int>(Keycode::Button1) && code <= static_cast<int>(Keycode::HatW))
        return keycodeToString(static_cast<Keycode>(code));
    if (code >= static_cast<int>(Keycode::LeftClick)
        && code <= static_cast<int>(Keycode::MouseForward))
        return keycodeToString(static_cast<Keycode>(code));
    if (code >= static_cast<int>(Keycode::LeftControl)
        && code <= static_cast<int>(Keycode::RightGui))
        return keycodeToString(static_cast<Keycode>(code));
    if (code >= 0 && code <= static_cast<int>(Keycode::ExSel))
        return keycodeToString(static_cast<Keycode>(code));
    return "Unknown";
}

int vkToKeycode(int vk)
{
    if (vk < 0 || std::cmp_greater_equal(vk, vkTable().size()))
        return static_cast<int>(Keycode::None);
    return vkTable()[static_cast<std::size_t>(vk)];
}

int keyNameToVk(std::string_view name)
{
    const auto& table = keyNameTable();
    const auto it = table.find(name);
    if (it == table.end())
        return -1;
    return it->second;
}

} // namespace Kbi
