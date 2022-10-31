using System;
using Tomato.Hal.Interfaces;

namespace Tomato.Terminal;

public static class KeyMap
{
    
    private static ReadOnlyMemory<int> _usKeymap = new int[90]
    {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
        '9', '0', '+', /*'´' */0, '\b',	/* Backspace */
        '\t',			/* Tab */
        'q', 'w', 'e', 'r',	/* 19 */
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
        0,			/* 29   - Control */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
        '\'', '<',   0,		/* Left shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
        'm', ',', '.', '-',   0,				/* Right shift */
        '*',
        0,	/* Alt */
        ' ',	/* Space bar */
        0,	/* Caps lock */
        0,	/* 59 - F1 key ... > */
        0,   0,   0,   0,   0,   0,   0,   0,
        0,	/* < ... F10 */
        0,	/* 69 - Num lock*/
        0,	/* Scroll Lock */
        0,	/* Home key */
        0,	/* Up Arrow */
        0,	/* Page Up */
        '-',
        0,	/* Left Arrow */
        0,
        0,	/* Right Arrow */
        '+',
        0,	/* 79 - End key*/
        0,	/* Down Arrow */
        0,	/* Page Down */
        0,	/* Insert Key */
        0,	/* Delete Key */
        0,   0,  '<',
        0,	/* F11 Key */
        0,	/* F12 Key */
        0,	/* All other keys are undefined */
    };
    
    private static ReadOnlyMemory<int> _usKeymapShift = new int[90]
    {
        0,  27, '!', '\"', '#', 0 /* shift+4 */, '%', '&', '/', '(',	/* 9 */
        ')', '=', '?', '`', '\b',	/* Backspace */
        '\t',			/* Tab */

        'Q', 'W', 'E', 'R',   /* 19 */
        'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'A', '\n', /* Enter key */
        0,          /* 29   - Control */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'O', /* 39 */
        '\'', '>',   0,        /* Left shift */
        '*', 'Z', 'X', 'C', 'V', 'B', 'N',            /* 49 */
        'M', ';', ':', '_',   0,              /* Right shift */

        '*',
        0,	/* Alt */
        ' ',	/* Space bar */
        0,	/* Caps lock */
        0,	/* 59 - F1 key ... > */
        0,   0,   0,   0,   0,   0,   0,   0,
        0,	/* < ... F10 */
        0,	/* 69 - Num lock*/
        0,	/* Scroll Lock */
        0,	/* Home key */
        0,	/* Up Arrow */
        0,	/* Page Up */
        '-',
        0,	/* Left Arrow */
        0,
        0,	/* Right Arrow */
        '+',
        0,	/* 79 - End key*/
        0,	/* Down Arrow */
        0,	/* Page Down */
        0,	/* Insert Key */
        0,	/* Delete Key */
        0,   0,   '>',
        0,	/* F11 Key */
        0,	/* F12 Key */
        0,	/* All other keys are undefined */
    };

    private static ReadOnlyMemory<int> _usKeymapAlt = new int[90]
    {
        0,  27, 0 /*alt+1*/, '@', 0, '$', 0, 0, '{', '[',	/* 9 */
        ']', '}', '\\', '=', '\b',	/* Backspace */
        '\t',			/* Tab */
        'q', 'w', 'e', 'r',	/* 19 */
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
        0,			/* 29   - Control */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
        '\'', '`',   0,		/* Left shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
        'm', ',', '.', '/',   0,				/* Right shift */
        '*',
        0,	/* Alt */
        ' ',	/* Space bar */
        0,	/* Caps lock */
        0,	/* 59 - F1 key ... > */
        0,   0,   0,   0,   0,   0,   0,   0,
        0,	/* < ... F10 */
        0,	/* 69 - Num lock*/
        0,	/* Scroll Lock */
        0,	/* Home key */
        0,	/* Up Arrow */
        0,	/* Page Up */
        '-',
        0,	/* Left Arrow */
        0,
        0,	/* Right Arrow */
        '+',
        0,	/* 79 - End key*/
        0,	/* Down Arrow */
        0,	/* Page Down */
        0,	/* Insert Key */
        0,	/* Delete Key */
        0,   0,  '|',
        0,	/* F11 Key */
        0,	/* F12 Key */
        0,	/* All other keys are undefined */
    };
    
    public const int LeftShift = 0x2A;
    public const int RightShift = 0x36;
    public const int LeftAlt = 0x38;
    public const int RightAlt = 0x38 | 0x80;
    public const int Enter = 0x1C;
    public const int Backspace = 0x0E;

    public static int GetCodepoint(int code, bool shiftHeld, bool altGrHeld)
    {
        // select one 
        var map = shiftHeld ? _usKeymapShift : (altGrHeld ? _usKeymapAlt : _usKeymap);

        // check in range
        if ((int)code > map.Length)
        {
            return -1;
        }

        // return it if not zero
        var value = map.Span[(int)code];
        if (value == 0)
        {
            return -1;
        }

        // return it 
        return value;
    }

}