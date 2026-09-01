#ifndef TYPES_KEYS_H
#define TYPES_KEYS_H

#if !defined(VK_BACK)
#define VK_BACK 0x08
#endif
#if !defined(VK_TAB)
#define VK_TAB 0x09
#endif
#if !defined(VK_RETURN)
#define VK_RETURN 0x0D
#endif
#if !defined(VK_SHIFT)
#define VK_SHIFT 0x10
#endif
#if !defined(VK_CONTROL)
#define VK_CONTROL 0x11
#endif
#if !defined(VK_MENU)
#define VK_MENU 0x12
#endif
#if !defined(VK_ESCAPE)
#define VK_ESCAPE 0x1B
#endif
#if !defined(VK_SPACE)
#define VK_SPACE 0x20
#endif
#if !defined(VK_LEFT)
#define VK_LEFT 0x25
#endif
#if !defined(VK_UP)
#define VK_UP 0x26
#endif
#if !defined(VK_RIGHT)
#define VK_RIGHT 0x27
#endif
#if !defined(VK_DOWN)
#define VK_DOWN 0x28
#endif
#if !defined(VK_LWIN)
#define VK_LWIN 0x5B
#endif
#if !defined(VK_RWIN)
#define VK_RWIN 0x5C
#endif
#if !defined(VK_OEM_COMMA)
#define VK_OEM_COMMA 0xBC
#endif
#if !defined(VK_OEM_PERIOD)
#define VK_OEM_PERIOD 0xBE
#endif
#if !defined(VK_OEM_1)
#define VK_OEM_1 0xBA
#endif
#if !defined(VK_OEM_2)
#define VK_OEM_2 0xBF
#endif
#if !defined(VK_OEM_4)
#define VK_OEM_4 0xDB
#endif
#if !defined(VK_OEM_7)
#define VK_OEM_7 0xDE
#endif
#if !defined(VK_OEM_6)
#define VK_OEM_6 0xDD
#endif
#if !defined(VK_OEM_3)
#define VK_OEM_3 0xC0
#endif
#if !defined(VK_OEM_MINUS)
#define VK_OEM_MINUS 0xBD
#endif
#if !defined(VK_OEM_PLUS)
#define VK_OEM_PLUS 0xBB
#endif
#if !defined(VK_LSHIFT)
#define VK_LSHIFT 0xA0
#endif
#if !defined(VK_RSHIFT)
#define VK_RSHIFT 0xA1
#endif
#if !defined(VK_DELETE)
#define VK_DELETE 0x2E
#endif
#if !defined(VK_HOME)
#define VK_HOME 0x24
#endif
#if !defined(VK_END)
#define VK_END 0x23
#endif
#if !defined(VK_PRIOR)
#define VK_PRIOR 0x21
#endif
#if !defined(VK_NEXT)
#define VK_NEXT 0x22
#endif
#if !defined(VK_F1)
#define VK_F1 0x70
#endif
#if !defined(VK_F12)
#define VK_F12 0x7B
#endif
#if !defined(VK_LCONTROL)
#define VK_LCONTROL 0xA2
#endif
#if !defined(VK_RCONTROL)
#define VK_RCONTROL 0xA3
#endif
#if !defined(VK_LMENU)
#define VK_LMENU 0xA4
#endif
#if !defined(VK_RMENU)
#define VK_RMENU 0xA5
#endif

#define HOTKEY_MOD_NONE         0x00
#define HOTKEY_MOD_ALT          0x01
#define HOTKEY_MOD_SHIFT        0x02
#define HOTKEY_MOD_CTRL         0x04
#define HOTKEY_MOD_SUPER         0x08
#define HOTKEY_MOD_ALT_SHIFT    (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT)
#define HOTKEY_MOD_ALT_CTRL     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_SHIFT_CTRL   (HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_ALT_SHIFT_CTRL (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)

#endif
