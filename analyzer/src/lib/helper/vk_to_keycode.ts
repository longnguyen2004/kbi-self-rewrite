import { Keycode } from "$lib/keycode/keycode";

// Inlined VK_* macros from WinUser.h
const VK_BACK      = 0x08;
const VK_TAB       = 0x09;
const VK_RETURN    = 0x0D;
const VK_PAUSE     = 0x13;
const VK_CAPITAL   = 0x14;
const VK_ESCAPE    = 0x1B;
const VK_SPACE     = 0x20;
const VK_PRIOR     = 0x21;
const VK_NEXT      = 0x22;
const VK_END       = 0x23;
const VK_HOME      = 0x24;
const VK_LEFT      = 0x25;
const VK_UP        = 0x26;
const VK_RIGHT     = 0x27;
const VK_DOWN      = 0x28;
const VK_SNAPSHOT  = 0x2C;
const VK_INSERT    = 0x2D;
const VK_DELETE    = 0x2E;
const VK_LWIN      = 0x5B;
const VK_RWIN      = 0x5C;
const VK_APPS      = 0x5D;
const VK_NUMPAD0   = 0x60;
const VK_NUMPAD1   = 0x61;
const VK_NUMPAD2   = 0x62;
const VK_NUMPAD3   = 0x63;
const VK_NUMPAD4   = 0x64;
const VK_NUMPAD5   = 0x65;
const VK_NUMPAD6   = 0x66;
const VK_NUMPAD7   = 0x67;
const VK_NUMPAD8   = 0x68;
const VK_NUMPAD9   = 0x69;
const VK_MULTIPLY  = 0x6A;
const VK_ADD       = 0x6B;
const VK_SEPARATOR = 0x6C;
const VK_SUBTRACT  = 0x6D;
const VK_DECIMAL   = 0x6E;
const VK_DIVIDE    = 0x6F;
const VK_F1        = 0x70;
const VK_F2        = 0x71;
const VK_F3        = 0x72;
const VK_F4        = 0x73;
const VK_F5        = 0x74;
const VK_F6        = 0x75;
const VK_F7        = 0x76;
const VK_F8        = 0x77;
const VK_F9        = 0x78;
const VK_F10       = 0x79;
const VK_F11       = 0x7A;
const VK_F12       = 0x7B;
const VK_F13       = 0x7C;
const VK_F14       = 0x7D;
const VK_F15       = 0x7E;
const VK_F16       = 0x7F;
const VK_F17       = 0x80;
const VK_F18       = 0x81;
const VK_F19       = 0x82;
const VK_F20       = 0x83;
const VK_F21       = 0x84;
const VK_F22       = 0x85;
const VK_F23       = 0x86;
const VK_F24       = 0x87;
const VK_NUMLOCK   = 0x90;
const VK_SCROLL    = 0x91;
const VK_LSHIFT    = 0xA0;
const VK_RSHIFT    = 0xA1;
const VK_LCONTROL  = 0xA2;
const VK_RCONTROL  = 0xA3;
const VK_LMENU     = 0xA4;
const VK_RMENU     = 0xA5;
const VK_OEM_1     = 0xBA;
const VK_OEM_PLUS  = 0xBB;
const VK_OEM_COMMA = 0xBC;
const VK_OEM_MINUS = 0xBD;
const VK_OEM_PERIOD = 0xBE;
const VK_OEM_2     = 0xBF;
const VK_OEM_3     = 0xC0;
const VK_OEM_4     = 0xDB;
const VK_OEM_5     = 0xDC;
const VK_OEM_6     = 0xDD;
const VK_OEM_7     = 0xDE;

const vkToKeycodeArr = (() => {
    // Thank you Copilot
    // Create the mapping array
    const arr = new Array<Keycode>(256).fill(Keycode.None);

    arr[VK_BACK]      = Keycode.Backspace;
    arr[VK_TAB]       = Keycode.Tab;
    arr[VK_RETURN]    = Keycode.Enter;
    arr[VK_PAUSE]     = Keycode.Pause;
    arr[VK_CAPITAL]   = Keycode.CapsLock;
    arr[VK_ESCAPE]    = Keycode.Escape;
    arr[VK_SPACE]     = Keycode.Space;
    arr[VK_PRIOR]     = Keycode.PageUp;
    arr[VK_NEXT]      = Keycode.PageDown;
    arr[VK_END]       = Keycode.End;
    arr[VK_HOME]      = Keycode.Home;
    arr[VK_LEFT]      = Keycode.LeftArrow;
    arr[VK_UP]        = Keycode.UpArrow;
    arr[VK_RIGHT]     = Keycode.RightArrow;
    arr[VK_DOWN]      = Keycode.DownArrow;
    arr[VK_SNAPSHOT]  = Keycode.PrintScreen;
    arr[VK_INSERT]    = Keycode.Insert;
    arr[VK_DELETE]    = Keycode.Delete;

    // 0-9
    arr[0x30] = Keycode.Num0;
    arr[0x31] = Keycode.Num1;
    arr[0x32] = Keycode.Num2;
    arr[0x33] = Keycode.Num3;
    arr[0x34] = Keycode.Num4;
    arr[0x35] = Keycode.Num5;
    arr[0x36] = Keycode.Num6;
    arr[0x37] = Keycode.Num7;
    arr[0x38] = Keycode.Num8;
    arr[0x39] = Keycode.Num9;

    // A-Z
    arr[0x41] = Keycode.A;
    arr[0x42] = Keycode.B;
    arr[0x43] = Keycode.C;
    arr[0x44] = Keycode.D;
    arr[0x45] = Keycode.E;
    arr[0x46] = Keycode.F;
    arr[0x47] = Keycode.G;
    arr[0x48] = Keycode.H;
    arr[0x49] = Keycode.I;
    arr[0x4A] = Keycode.J;
    arr[0x4B] = Keycode.K;
    arr[0x4C] = Keycode.L;
    arr[0x4D] = Keycode.M;
    arr[0x4E] = Keycode.N;
    arr[0x4F] = Keycode.O;
    arr[0x50] = Keycode.P;
    arr[0x51] = Keycode.Q;
    arr[0x52] = Keycode.R;
    arr[0x53] = Keycode.S;
    arr[0x54] = Keycode.T;
    arr[0x55] = Keycode.U;
    arr[0x56] = Keycode.V;
    arr[0x57] = Keycode.W;
    arr[0x58] = Keycode.X;
    arr[0x59] = Keycode.Y;
    arr[0x5A] = Keycode.Z;

    arr[VK_LWIN]      = Keycode.LeftGUI;
    arr[VK_RWIN]      = Keycode.RightGUI;
    arr[VK_APPS]      = Keycode.Application;

    // Keypad
    arr[VK_NUMPAD0]   = Keycode.Keypad0;
    arr[VK_NUMPAD1]   = Keycode.Keypad1;
    arr[VK_NUMPAD2]   = Keycode.Keypad2;
    arr[VK_NUMPAD3]   = Keycode.Keypad3;
    arr[VK_NUMPAD4]   = Keycode.Keypad4;
    arr[VK_NUMPAD5]   = Keycode.Keypad5;
    arr[VK_NUMPAD6]   = Keycode.Keypad6;
    arr[VK_NUMPAD7]   = Keycode.Keypad7;
    arr[VK_NUMPAD8]   = Keycode.Keypad8;
    arr[VK_NUMPAD9]   = Keycode.Keypad9;
    arr[VK_MULTIPLY]  = Keycode.KeypadAsterisk;
    arr[VK_ADD]       = Keycode.KeypadPlus;
    arr[VK_SEPARATOR] = Keycode.KeypadComma;
    arr[VK_SUBTRACT]  = Keycode.KeypadMinus;
    arr[VK_DECIMAL]   = Keycode.KeypadPeriod;
    arr[VK_DIVIDE]    = Keycode.KeypadSlash;

    // Function keys
    arr[VK_F1]  = Keycode.F1;
    arr[VK_F2]  = Keycode.F2;
    arr[VK_F3]  = Keycode.F3;
    arr[VK_F4]  = Keycode.F4;
    arr[VK_F5]  = Keycode.F5;
    arr[VK_F6]  = Keycode.F6;
    arr[VK_F7]  = Keycode.F7;
    arr[VK_F8]  = Keycode.F8;
    arr[VK_F9]  = Keycode.F9;
    arr[VK_F10] = Keycode.F10;
    arr[VK_F11] = Keycode.F11;
    arr[VK_F12] = Keycode.F12;
    arr[VK_F13] = Keycode.F13;
    arr[VK_F14] = Keycode.F14;
    arr[VK_F15] = Keycode.F15;
    arr[VK_F16] = Keycode.F16;
    arr[VK_F17] = Keycode.F17;
    arr[VK_F18] = Keycode.F18;
    arr[VK_F19] = Keycode.F19;
    arr[VK_F20] = Keycode.F20;
    arr[VK_F21] = Keycode.F21;
    arr[VK_F22] = Keycode.F22;
    arr[VK_F23] = Keycode.F23;
    arr[VK_F24] = Keycode.F24;

    arr[VK_NUMLOCK]   = Keycode.NumLock;
    arr[VK_SCROLL]    = Keycode.ScrollLock;

    arr[VK_LSHIFT]    = Keycode.LeftShift;
    arr[VK_RSHIFT]    = Keycode.RightShift;
    arr[VK_LCONTROL]  = Keycode.LeftControl;
    arr[VK_RCONTROL]  = Keycode.RightControl;
    arr[VK_LMENU]     = Keycode.LeftAlt;
    arr[VK_RMENU]     = Keycode.RightAlt;

    arr[VK_OEM_1]      = Keycode.Semicolon;
    arr[VK_OEM_PLUS]   = Keycode.Equal;
    arr[VK_OEM_COMMA]  = Keycode.Comma;
    arr[VK_OEM_MINUS]  = Keycode.Minus;
    arr[VK_OEM_PERIOD] = Keycode.Period;
    arr[VK_OEM_2]      = Keycode.Slash;
    arr[VK_OEM_3]      = Keycode.Grave;
    arr[VK_OEM_4]      = Keycode.LeftBracket;
    arr[VK_OEM_5]      = Keycode.Backslash;
    arr[VK_OEM_6]      = Keycode.RightBracket;
    arr[VK_OEM_7]      = Keycode.Apostrophe;
    return arr
})();

const keyStrToVkMap: Record<string, number | undefined> = {
    // Letters
    A: 0x41, B: 0x42, C: 0x43, D: 0x44, E: 0x45, F: 0x46, G: 0x47,
    H: 0x48, I: 0x49, J: 0x4A, K: 0x4B, L: 0x4C, M: 0x4D, N: 0x4E,
    O: 0x4F, P: 0x50, Q: 0x51, R: 0x52, S: 0x53, T: 0x54, U: 0x55,
    V: 0x56, W: 0x57, X: 0x58, Y: 0x59, Z: 0x5A,
    // Numbers
    D0: 0x30, D1: 0x31, D2: 0x32, D3: 0x33, D4: 0x34,
    D5: 0x35, D6: 0x36, D7: 0x37, D8: 0x38, D9: 0x39,
    // Function keys
    F1:  VK_F1,  F2:  VK_F2,  F3:  VK_F3 , F4:  VK_F4 , F5:  VK_F5 , F6:  VK_F6,
    F7:  VK_F7,  F8:  VK_F8,  F9:  VK_F9 , F10: VK_F10, F11: VK_F11, F12: VK_F12,
    F13: VK_F13, F14: VK_F14, F15: VK_F15, F16: VK_F16, F17: VK_F17, F18: VK_F18,
    F19: VK_F19, F20: VK_F20, F21: VK_F21, F22: VK_F22, F23: VK_F23, F24: VK_F24,
    // Control keys
    Back: VK_BACK,
    Tab: VK_TAB,
    Enter: VK_RETURN,
    Pause: VK_PAUSE,
    Capital: VK_CAPITAL,
    CapsLock: VK_CAPITAL,
    Escape: VK_ESCAPE,
    Space: VK_SPACE,
    PageUp: VK_PRIOR,
    PageDown: VK_NEXT,
    End: VK_END,
    Home: VK_HOME,
    Left: VK_LEFT,
    Up: VK_UP,
    Right: VK_RIGHT,
    Down: VK_DOWN,
    PrintScreen: VK_SNAPSHOT,
    Snapshot: VK_SNAPSHOT,
    Insert: VK_INSERT,
    Delete: VK_DELETE,
    LWin: VK_LWIN,
    RWin: VK_RWIN,
    Apps: VK_APPS,
    NumLock: VK_NUMLOCK,
    Scroll: VK_SCROLL,
    // Numpad
    NumPad0: VK_NUMPAD0, NumPad1: VK_NUMPAD1, NumPad2: VK_NUMPAD2, NumPad3: VK_NUMPAD3, NumPad4: VK_NUMPAD4,
    NumPad5: VK_NUMPAD5, NumPad6: VK_NUMPAD6, NumPad7: VK_NUMPAD7, NumPad8: VK_NUMPAD8, NumPad9: VK_NUMPAD9,
    Multiply: VK_MULTIPLY, Add: VK_ADD, Separator: VK_SEPARATOR,
    Subtract: VK_SUBTRACT, Decimal: VK_DECIMAL, Divide: VK_DIVIDE,
    // Modifiers
    LShiftKey: VK_LSHIFT,
    RShiftKey: VK_RSHIFT,
    LControlKey: VK_LCONTROL,
    RControlKey: VK_RCONTROL,
    LMenu: VK_LMENU,
    RMenu: VK_RMENU,
    OemSemicolon: VK_OEM_1,
    Oem1: VK_OEM_1,
    Oemplus: VK_OEM_PLUS,
    Oemcomma: VK_OEM_COMMA,
    OemMinus: VK_OEM_MINUS,
    OemPeriod: VK_OEM_PERIOD,
    OemQuestion: VK_OEM_2,
    Oem2: VK_OEM_2,
    OemTilde: VK_OEM_3,
    Oem3: VK_OEM_3,
    OemOpenBrackets: VK_OEM_4,
    Oem4: VK_OEM_4,
    OemPipe: VK_OEM_5,
    Oem5: VK_OEM_5,
    OemCloseBrackets: VK_OEM_6,
    Oem6: VK_OEM_6,
    OemQuotes: VK_OEM_7,
    Oem7: VK_OEM_7
};

export function vkToKeycode(vk: number) {
    if (vk < 0 || vk >= vkToKeycodeArr.length) {
        return Keycode.None;
    }
    return vkToKeycodeArr[vk];
}

export function keyStrToVk(str: string) {
    return keyStrToVkMap[str];
}