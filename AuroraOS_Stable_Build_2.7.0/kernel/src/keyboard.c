#include "keyboard.h"
#include "io.h"

#define KEYBOARD_QUEUE_SIZE 128U

static int keyboard_queue[KEYBOARD_QUEUE_SIZE];
static unsigned int queue_head = 0;
static unsigned int queue_tail = 0;
static int extended_prefix = 0;
static int ctrl_down = 0;
static int shift_down = 0;
static keyboard_layout_t current_layout = KEYBOARD_LAYOUT_DE;

static int apply_shift_de(uint8_t scancode, int key) {
    switch (scancode) {
        case 0x02: return '!';
        case 0x03: return '"';
        case 0x05: return '$';
        case 0x06: return '%';
        case 0x07: return '&';
        case 0x08: return '/';
        case 0x09: return '(';
        case 0x0A: return ')';
        case 0x0B: return '=';
        case 0x0C: return '?';
        case 0x1A: return '*';
        case 0x1B: return '*';
        case 0x2B: return '\'';
        case 0x33: return ';';
        case 0x34: return ':';
        case 0x35: return '_';
        default: break;
    }

    if (key >= 'a' && key <= 'z') {
        return key - 32;
    }

    return key;
}

static int apply_shift_us(uint8_t scancode, int key) {
    switch (scancode) {
        case 0x02: return '!';
        case 0x03: return '@';
        case 0x04: return '#';
        case 0x05: return '$';
        case 0x06: return '%';
        case 0x07: return '^';
        case 0x08: return '&';
        case 0x09: return '*';
        case 0x0A: return '(';
        case 0x0B: return ')';
        case 0x0C: return '_';
        case 0x0D: return '+';
        case 0x1A: return '{';
        case 0x1B: return '}';
        case 0x27: return ':';
        case 0x28: return '"';
        case 0x29: return '~';
        case 0x2B: return '|';
        case 0x33: return '<';
        case 0x34: return '>';
        case 0x35: return '?';
        default: break;
    }

    if (key >= 'a' && key <= 'z') {
        return key - 32;
    }

    return key;
}

static int scancode_to_key(uint8_t scancode, int extended) {
    static const char map_de[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', 0,
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 0, '+',
        '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, 0, '^',
        0, '#', 'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, '*',
        0, ' ', 0
    };

    static const char map_us[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
        0, ' ', 0
    };
    int key;

    if (extended != 0) {
        switch (scancode) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x49: return KEY_PAGEUP;
            case 0x51: return KEY_PAGEDOWN;
            default: return KEY_NONE;
        }
    }

    if (scancode >= 128U) {
        return KEY_NONE;
    }

    if (current_layout == KEYBOARD_LAYOUT_US) {
        key = (int)map_us[scancode];
        if (shift_down != 0) {
            return apply_shift_us(scancode, key);
        }
        return key;
    }

    key = (int)map_de[scancode];
    if (shift_down != 0) {
        return apply_shift_de(scancode, key);
    }
    return key;
}

static void queue_key(int key) {
    unsigned int next_tail = (queue_tail + 1U) % KEYBOARD_QUEUE_SIZE;
    if (next_tail == queue_head) {
        return;
    }

    keyboard_queue[queue_tail] = key;
    queue_tail = next_tail;
}

static int process_scancode(uint8_t scancode) {
    int key;

    if (scancode == 0xE0U) {
        extended_prefix = 1;
        return KEY_NONE;
    }

    if ((scancode & 0x80U) != 0U) {
        uint8_t released = (uint8_t)(scancode & 0x7FU);
        if (released == 0x1DU) {
            ctrl_down = 0;
        }
        if (released == 0x2AU || released == 0x36U) {
            shift_down = 0;
        }
        extended_prefix = 0;
        return KEY_NONE;
    }

    if (scancode == 0x1DU) {
        ctrl_down = 1;
        extended_prefix = 0;
        return KEY_NONE;
    }

    if (scancode == 0x2AU || scancode == 0x36U) {
        shift_down = 1;
        extended_prefix = 0;
        return KEY_NONE;
    }

    key = scancode_to_key(scancode, extended_prefix);
    extended_prefix = 0;

    if (ctrl_down != 0) {
        if (key == 'c' || key == 'C') {
            return KEY_CTRL_C;
        }
        if (key == 's' || key == 'S') {
            return KEY_CTRL_S;
        }
        if (key == 'q' || key == 'Q') {
            return KEY_CTRL_Q;
        }
    }

    return key;
}

void keyboard_init(void) {
    queue_head = 0;
    queue_tail = 0;
    extended_prefix = 0;
    ctrl_down = 0;
    shift_down = 0;
    current_layout = KEYBOARD_LAYOUT_DE;
}

void keyboard_handle_interrupt_scancode(uint8_t scancode) {
    int key = process_scancode(scancode);
    if (key != KEY_NONE) {
        queue_key(key);
    }
}

int keyboard_read_key(void) {
    if (queue_head != queue_tail) {
        int queued = keyboard_queue[queue_head];
        queue_head = (queue_head + 1U) % KEYBOARD_QUEUE_SIZE;
        return queued;
    }

    if ((inb(0x64) & 1U) == 0U) {
        return KEY_NONE;
    }

    return process_scancode(inb(0x60));
}

void keyboard_set_layout(keyboard_layout_t layout) {
    current_layout = layout;
}

keyboard_layout_t keyboard_get_layout(void) {
    return current_layout;
}

const char* keyboard_layout_name(void) {
    if (current_layout == KEYBOARD_LAYOUT_US) {
        return "us";
    }
    return "de";
}
