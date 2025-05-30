#include "vk_to_keycode.h"
#include <array>
#include <windows.h>

static constexpr auto vk_to_keycode_arr = []() {
    std::array<Keycode, 256> arr{Keycode::None};

    arr[VK_BACK]      = Keycode::Backspace;
    arr[VK_TAB]       = Keycode::Tab;
    arr[VK_RETURN]    = Keycode::Enter;
    arr[VK_CAPITAL]   = Keycode::CapsLock;
    arr[VK_ESCAPE]    = Keycode::Escape;
    arr[VK_SPACE]     = Keycode::Space;
    arr[VK_PRIOR]     = Keycode::PageUp;
    arr[VK_NEXT]      = Keycode::PageDown;
    arr[VK_END]       = Keycode::End;
    arr[VK_HOME]      = Keycode::Home;
    arr[VK_LEFT]      = Keycode::LeftArrow;
    arr[VK_UP]        = Keycode::UpArrow;
    arr[VK_RIGHT]     = Keycode::RightArrow;
    arr[VK_DOWN]      = Keycode::DownArrow;
    arr[VK_INSERT]    = Keycode::Insert;
    arr[VK_DELETE]    = Keycode::Delete;

    // 0-9
    arr['0'] = Keycode::Num0;
    arr['1'] = Keycode::Num1;
    arr['2'] = Keycode::Num2;
    arr['3'] = Keycode::Num3;
    arr['4'] = Keycode::Num4;
    arr['5'] = Keycode::Num5;
    arr['6'] = Keycode::Num6;
    arr['7'] = Keycode::Num7;
    arr['8'] = Keycode::Num8;
    arr['9'] = Keycode::Num9;

    // A-Z
    arr['A'] = Keycode::A;
    arr['B'] = Keycode::B;
    arr['C'] = Keycode::C;
    arr['D'] = Keycode::D;
    arr['E'] = Keycode::E;
    arr['F'] = Keycode::F;
    arr['G'] = Keycode::G;
    arr['H'] = Keycode::H;
    arr['I'] = Keycode::I;
    arr['J'] = Keycode::J;
    arr['K'] = Keycode::K;
    arr['L'] = Keycode::L;
    arr['M'] = Keycode::M;
    arr['N'] = Keycode::N;
    arr['O'] = Keycode::O;
    arr['P'] = Keycode::P;
    arr['Q'] = Keycode::Q;
    arr['R'] = Keycode::R;
    arr['S'] = Keycode::S;
    arr['T'] = Keycode::T;
    arr['U'] = Keycode::U;
    arr['V'] = Keycode::V;
    arr['W'] = Keycode::W;
    arr['X'] = Keycode::X;
    arr['Y'] = Keycode::Y;
    arr['Z'] = Keycode::Z;

    arr[VK_LWIN]      = Keycode::LeftGUI;
    arr[VK_RWIN]      = Keycode::RightGUI;
    arr[VK_APPS]      = Keycode::Application;

    // Keypad
    arr[VK_NUMPAD0]   = Keycode::Keypad0;
    arr[VK_NUMPAD1]   = Keycode::Keypad1;
    arr[VK_NUMPAD2]   = Keycode::Keypad2;
    arr[VK_NUMPAD3]   = Keycode::Keypad3;
    arr[VK_NUMPAD4]   = Keycode::Keypad4;
    arr[VK_NUMPAD5]   = Keycode::Keypad5;
    arr[VK_NUMPAD6]   = Keycode::Keypad6;
    arr[VK_NUMPAD7]   = Keycode::Keypad7;
    arr[VK_NUMPAD8]   = Keycode::Keypad8;
    arr[VK_NUMPAD9]   = Keycode::Keypad9;
    arr[VK_MULTIPLY]  = Keycode::KeypadAsterisk;
    arr[VK_ADD]       = Keycode::KeypadPlus;
    arr[VK_SUBTRACT]  = Keycode::KeypadMinus;
    arr[VK_DECIMAL]   = Keycode::KeypadPeriod;
    arr[VK_DIVIDE]    = Keycode::KeypadSlash;

    // Function keys
    arr[VK_F1]  = Keycode::F1;
    arr[VK_F2]  = Keycode::F2;
    arr[VK_F3]  = Keycode::F3;
    arr[VK_F4]  = Keycode::F4;
    arr[VK_F5]  = Keycode::F5;
    arr[VK_F6]  = Keycode::F6;
    arr[VK_F7]  = Keycode::F7;
    arr[VK_F8]  = Keycode::F8;
    arr[VK_F9]  = Keycode::F9;
    arr[VK_F10] = Keycode::F10;
    arr[VK_F11] = Keycode::F11;
    arr[VK_F12] = Keycode::F12;
    arr[VK_F13] = Keycode::F13;
    arr[VK_F14] = Keycode::F14;
    arr[VK_F15] = Keycode::F15;
    arr[VK_F16] = Keycode::F16;
    arr[VK_F17] = Keycode::F17;
    arr[VK_F18] = Keycode::F18;
    arr[VK_F19] = Keycode::F19;
    arr[VK_F20] = Keycode::F20;
    arr[VK_F21] = Keycode::F21;
    arr[VK_F22] = Keycode::F22;
    arr[VK_F23] = Keycode::F23;
    arr[VK_F24] = Keycode::F24;

    arr[VK_NUMLOCK]   = Keycode::NumLock;
    arr[VK_SCROLL]    = Keycode::ScrollLock;

    arr[VK_LSHIFT]    = Keycode::LeftShift;
    arr[VK_RSHIFT]    = Keycode::RightShift;
    arr[VK_LCONTROL]  = Keycode::LeftControl;
    arr[VK_RCONTROL]  = Keycode::RightControl;
    arr[VK_LMENU]     = Keycode::LeftAlt;
    arr[VK_RMENU]     = Keycode::RightAlt;
    
    return arr;
}();

Keycode vk_to_keycode(int vk) {
    if (vk < 0 || vk >= vk_to_keycode_arr.size()) {
        return Keycode::None;
    }
    return vk_to_keycode_arr[vk];
}
