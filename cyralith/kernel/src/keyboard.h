#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

enum {
    KEY_NONE = 0,
    KEY_ENTER = '\n',
    KEY_BACKSPACE = '\b',
    KEY_UP = 256,
    KEY_DOWN = 257,
    KEY_LEFT = 258,
    KEY_RIGHT = 259,
    KEY_PAGEUP = 260,
    KEY_PAGEDOWN = 261,
    KEY_CTRL_C = 262,
    KEY_CTRL_S = 263,
    KEY_CTRL_Q = 264,
    KEY_CTRL_F = 265,
    KEY_CTRL_G = 266
};

typedef enum {
    KEYBOARD_LAYOUT_DE = 0,
    KEYBOARD_LAYOUT_US = 1
} keyboard_layout_t;

void keyboard_init(void);
void keyboard_handle_interrupt_scancode(uint8_t scancode);
int keyboard_read_key(void);
void keyboard_set_layout(keyboard_layout_t layout);
keyboard_layout_t keyboard_get_layout(void);
const char* keyboard_layout_name(void);

#endif
