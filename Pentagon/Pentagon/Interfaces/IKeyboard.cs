﻿using System;
using System.Collections.Generic;

namespace Pentagon.Interfaces
{
    internal class KeyEvent
    {
        public KeyCode Code;
        public bool Released;
        internal KeyEvent(KeyCode c, bool r)
        {
            Code = c;
            Released = r;
        }
    }

    internal enum KeyCode
    {
        None = 0,
        Escape,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,
        Num0,
        Hyphen,
        Equals,
        Backspace,
        Tab,
        Q,
        W,
        E,
        R,
        T,
        Y,
        U,
        I,
        O,
        P,
        LeftBrace,
        RightBrace,
        Enter,
        LeftCtrl,
        A,
        S,
        D,
        F,
        G,
        H,
        J,
        K,
        L,
        Punctuation3,
        Punctuation4,
        Punctuation5,
        LeftShift,
        Punctuation1,
        Z,
        X,
        C,
        V,
        B,
        N,
        M,
        Comma,
        Period,
        Slash,
        RightShift,
        NumMultiply,
        LeftAlt,
        Space,
        CapsLock,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        NumLock,
        ScrollLock,
        Numpad7,
        Numpad8,
        Numpad9,
        NumpadSubtract,
        Numpad4,
        Numpad5,
        Numpad6,
        NumpadAdd,
        Numpad1,
        Numpad2,
        Numpad3,
        Numpad0,
        NumpadPoint,
        F11,
        F12,
        MmPrevious,
        MmNext,
        NumpadEnter,
        RightCtrl,
        MmMute,
        MmCalc,
        MmPause,
        MmStop,
        MmQuieter,
        MmLouder,
        WwwHome,
        NumpadDivide,
        RightAlt,
        Home,
        UpArrow,
        PageUp,
        LeftArrow,
        RightArrow,
        End,
        DownArrow,
        PageDown,
        Insert,
        Delete,
        LeftFlag,
        RightFlag,
        ContextMenu,
        AcpiPower,
        AcpiSleep,
        AcpiWake,
        WwwSearch,
        WwwStarred,
        WwwRefresh,
        WwwStop,
        WwwForward,
        WwwBack,
        MmFiles,
        MmEmail,
        MmSelect
    }

    internal interface IKeyboard
    {
        internal void RegisterCallback(Action<KeyEvent> callback);
    }
}
