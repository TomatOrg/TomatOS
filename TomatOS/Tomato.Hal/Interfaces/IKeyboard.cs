﻿using System;

namespace Tomato.Hal.Interfaces;

/// <summary>
/// Represents a key event
/// </summary>
public struct KeyEvent
{
    public KeyCode ScanCode { get; }
    public bool Released { get; }
    
    public KeyEvent(KeyCode c, bool r)
    {
        ScanCode = c;
        Released = r;
    }
}

public enum KeyCode
{
    None = 0,
    A = 0x04,
    B = 0x05,
    C = 0x06,
    D = 0x07,
    E = 0x08,
    F = 0x09,
    G = 0x0A,
    H = 0x0B,
    I = 0x0C,
    J = 0x0D,
    K = 0x0E,
    L = 0x0F,
    M = 0x10,
    N = 0x11,
    O = 0x12,
    P = 0x13,
    Q = 0x14,
    R = 0x15,
    S = 0x16,
    T = 0x17,
    U = 0x18,
    V = 0x19,
    W = 0x1A,
    X = 0x1B,
    Y = 0x1C,
    Z = 0x1D,
    Num1 = 0x1E,
    Num2 = 0x1F,
    Num3 = 0x20,
    Num4 = 0x21,
    Num5 = 0x22,
    Num6 = 0x23,
    Num7 = 0x24,
    Num8 = 0x25,
    Num9 = 0x26,
    Num0 = 0x27,
    Enter = 0x28,
    Escape = 0x29,
    Backspace = 0x2A,
    Tab = 0x2B,
    Space = 0x2C,
    Hyphen = 0x2D,
    Equals = 0x2E,
    LeftBrace = 0x2F,
    RightBrace = 0x30,
    Comma = 0x36,
    Period = 0x37,
    Slash = 0x38,
    Punctuation1 = 0x31, // On Us keyboard, \|
    Punctuation2 = 0x32, // Not on Us keyboard
    Punctuation3 = 0x33, // On Us keyboard, ;:
    Punctuation4 = 0x34, // On Us keyboard, '"
    Punctuation5 = 0x35, // On Us keyboard, `~
    Punctuation6 = 0x64, // Not on Us keyboard
    F1 = 0x3A,
    F2 = 0x3B,
    F3 = 0x3C,
    F4 = 0x3D,
    F5 = 0x3E,
    F6 = 0x3F,
    F7 = 0x40,
    F8 = 0x41,
    F9 = 0x42,
    F10 = 0x43,
    F11 = 0x44,
    F12 = 0x45,
    F13 = 0x68,
    F14 = 0x69,
    F15 = 0x6A,
    F16 = 0x6B,
    F17 = 0x6C,
    F18 = 0x6D,
    F19 = 0x6E,
    F20 = 0x6F,
    F21 = 0x70,
    F22 = 0x71,
    F23 = 0x72,
    F24 = 0x73,
    CapsLock = 0x39,
    PrintScreen = 0x46,
    ScrollLock = 0x47,
    Pause = 0x48,
    Insert = 0x49,
    Home = 0x4A,
    PageUp = 0x4B,
    Delete = 0x4C,
    End = 0x4D,
    PageDown = 0x4E,
    RightArrow = 0x4F,
    LeftArrow = 0x50,
    DownArrow = 0x51,
    UpArrow = 0x52,
    NumLock = 0x53,
    ContextMenu = 0x65,
    SystemRequest = 0x9A,
    ActionExecute = 0x74,
    ActionHelp = 0x75,
    ActionMenu = 0x76,
    ActionSelect = 0x77,
    ActionStop = 0x78,
    ActionAgain = 0x79,
    ActionUndo = 0x7A,
    ActionCut = 0x7B,
    ActionCopy = 0x7C,
    ActionPaste = 0x7D,
    ActionFind = 0x7E,
    ActionCancel = 0x9B,
    ActionClear = 0x9C,
    ActionPrior = 0x9D,
    ActionReturn = 0x9E,
    ActionSeparator = 0x9F,
    MmMute = 0x7F,
    MmLouder = 0x80,
    MmQuieter = 0x81,
    MmNext = 0x103,
    MmPrevious = 0x104,
    MmStop = 0x105,
    MmPause = 0x106,
    MmSelect = 0x107,
    MmEmail = 0x108,
    MmCalc = 0x109,
    MmFiles = 0x10A,
    International1 = 0x87,
    International2 = 0x88,
    International3 = 0x89,
    International4 = 0x8A,
    International5 = 0x8B,
    International6 = 0x8C,
    International7 = 0x8D,
    International8 = 0x8E,
    International9 = 0x8F,
    HangulEnglishToggle = 0x90,
    HanjaConversion = 0x91,
    Katakana = 0x92,
    Hiragana = 0x93,
    HankakuZenkakuToggle = 0x94,
    AlternateErase = 0x99,
    ThousandsSeparator = 0xB2,
    DecimalSeparator = 0xB3,
    CurrencyUnit = 0xB4,
    CurrencySubunit = 0xB5,
    NumpadDivide = 0x54,
    NumpadMultiply = 0x55,
    NumpadSubtract = 0x56,
    NumpadAdd = 0x57,
    NumpadEnter = 0x58,
    Numpad1 = 0x59,
    Numpad2 = 0x5A,
    Numpad3 = 0x5B,
    Numpad4 = 0x5C,
    Numpad5 = 0x5D,
    Numpad6 = 0x5E,
    Numpad7 = 0x5F,
    Numpad8 = 0x60,
    Numpad9 = 0x61,
    Numpad0 = 0x62,
    NumpadPoint = 0x63,
    NumpadEquals = 0x67,
    NumpadComma = 0x82,
    Numpad00 = 0xB0,
    Numpad000 = 0xB1,
    NumpadLeftParen = 0xB6,
    NumpadRightParen = 0xB7,
    NumpadLeftBrace = 0xB8,
    NumpadRightBrace = 0xB9,
    NumpadTab = 0xBa,
    NumpadBackspace = 0xBb,
    NumpadA = 0xBc,
    NumpadB = 0xBd,
    NumpadC = 0xBe,
    NumpadD = 0xBf,
    NumpadE = 0xC0,
    NumpadF = 0xC1,
    NumpadXor = 0xC2,
    NumpadCaret = 0xC3,
    NumpadPercent = 0xC4,
    NumpadLessThan = 0xC5,
    NumpadGreaterThan = 0xC6,
    NumpadAmpersand = 0xC7,
    NumpadDoubleAmpersand = 0xC8,
    NumpadBar = 0xC9,
    NumpadDoubleBar = 0xCa,
    NumpadColon = 0xCb,
    NumpadHash = 0xCc,
    NumpadSpace = 0xCd,
    NumpadAt = 0xCe,
    NumpadExclamationMark = 0xCf,
    NumpadMemoryStore = 0xD0,
    NumpadMemoryRecall = 0xD1,
    NumpadMemoryClear = 0xD2,
    NumpadMemoryAdd = 0xD3,
    NumpadMemorySubtract = 0xD4,
    NumpadMemoryMultiply = 0xD5,
    NumpadMemoryDivide = 0xD6,
    NumpadNegate = 0xD7,
    NumpadClearAll = 0xD8,
    NumpadClear = 0xD9,
    NumpadBinary = 0xDa,
    NumpadOctal = 0xDb,
    NumpadDecimal = 0xDc,
    NumpadHexadecimal = 0xDd,
    LeftCtrl = 0xE0,
    LeftShift = 0xE1,
    LeftAlt = 0xE2,
    LeftFlag = 0xE3,
    RightCtrl = 0xE4,
    RightShift = 0xE5,
    RightAlt = 0xE6,
    RightFlag = 0xE7,
    AcpiPower = 0x100,
    AcpiSleep = 0x101,
    AcpiWake = 0x102,
    WwwSearch = 0x10B,
    WwwHome = 0x10C,
    WwwBack = 0x10D,
    WwwForward = 0x10E,
    WwwStop = 0x10F,
    WwwRefresh = 0x110,
    WwwStarred = 0x111,
}

public delegate void KeyboardCallback(in KeyEvent key);

public interface IKeyboard
{
    public void RegisterCallback(KeyboardCallback callback);
}