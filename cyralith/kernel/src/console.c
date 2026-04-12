#include "console.h"

static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const size_t HISTORY_LINES = 512;
static const size_t SCROLL_STEP = 20;

static char history[512][80];
static size_t history_line = 0;
static size_t history_col = 0;
static size_t history_used = 1;
static size_t viewport_top = 0;
static uint8_t color = 0x0F;

static uint16_t vga_entry(unsigned char ch, uint8_t c) {
    return (uint16_t)ch | ((uint16_t)c << 8);
}

static void vga_put_at(size_t x, size_t y, char ch) {
    VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry((unsigned char)ch, color);
}

static void clear_history_line(size_t line) {
    for (size_t x = 0; x < VGA_WIDTH; ++x) {
        history[line][x] = ' ';
    }
}

static size_t bottom_viewport_top(void) {
    if (history_used <= VGA_HEIGHT) {
        return 0;
    }
    return history_used - VGA_HEIGHT;
}

static void render(void) {
    for (size_t y = 0; y < VGA_HEIGHT; ++y) {
        size_t line_index = viewport_top + y;
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            char ch = ' ';
            if (line_index < history_used) {
                ch = history[line_index][x];
            }
            vga_put_at(x, y, ch);
        }
    }
}

static void render_history_line(size_t line_index) {
    if (line_index < viewport_top || line_index >= viewport_top + VGA_HEIGHT) {
        return;
    }

    size_t screen_y = line_index - viewport_top;
    for (size_t x = 0; x < VGA_WIDTH; ++x) {
        vga_put_at(x, screen_y, history[line_index][x]);
    }
}

static void follow_bottom(int force_render) {
    size_t new_top = bottom_viewport_top();
    if (force_render != 0 || viewport_top != new_top) {
        viewport_top = new_top;
        render();
        return;
    }
    viewport_top = new_top;
}

static void ensure_new_line(void) {
    if (history_line + 1U < HISTORY_LINES) {
        history_line++;
        if (history_used <= history_line) {
            history_used = history_line + 1U;
        }
        clear_history_line(history_line);
        history_col = 0;
        return;
    }

    for (size_t y = 1; y < HISTORY_LINES; ++y) {
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            history[y - 1U][x] = history[y][x];
        }
    }

    clear_history_line(HISTORY_LINES - 1U);
    history_line = HISTORY_LINES - 1U;
    history_col = 0;
    history_used = HISTORY_LINES;

    if (viewport_top > 0U) {
        viewport_top--;
    }
}

void console_set_color(uint8_t fg, uint8_t bg) {
    color = (uint8_t)(fg | (bg << 4));
    render();
}

void console_clear(void) {
    for (size_t y = 0; y < HISTORY_LINES; ++y) {
        clear_history_line(y);
    }

    history_line = 0;
    history_col = 0;
    history_used = 1;
    viewport_top = 0;
    render();
}

void console_init(void) {
    color = 0x0F;
    console_clear();
}

void console_putc(char c) {
    if (c == '\n') {
        int was_scrollback = console_is_scrollback_active();
        ensure_new_line();
        follow_bottom(was_scrollback != 0);
        return;
    }

    if (history_col >= VGA_WIDTH) {
        int was_scrollback = console_is_scrollback_active();
        ensure_new_line();
        follow_bottom(was_scrollback != 0);
    }

    history[history_line][history_col] = c;

    if (viewport_top == bottom_viewport_top() && history_line >= viewport_top && history_line < viewport_top + VGA_HEIGHT) {
        vga_put_at(history_col, history_line - viewport_top, c);
    } else {
        render_history_line(history_line);
    }

    history_col++;
}

void console_write(const char* str) {
    while (*str != '\0') {
        console_putc(*str++);
    }
}

void console_writeln(const char* str) {
    console_write(str);
    console_putc('\n');
}

void console_write_hex(uint32_t value) {
    static const char* HEX = "0123456789ABCDEF";
    console_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        console_putc(HEX[(value >> shift) & 0xF]);
    }
}

void console_write_dec(uint32_t value) {
    char buffer[11];
    size_t index = 0;

    if (value == 0U) {
        console_putc('0');
        return;
    }

    while (value > 0U && index < sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0U) {
        console_putc(buffer[--index]);
    }
}

void console_backspace(void) {
    if (history_col == 0U && history_line == 0U) {
        return;
    }

    if (history_col == 0U) {
        history_line--;
        history_col = VGA_WIDTH;
    }

    history_col--;
    history[history_line][history_col] = ' ';

    if (viewport_top == bottom_viewport_top() && history_line >= viewport_top && history_line < viewport_top + VGA_HEIGHT) {
        vga_put_at(history_col, history_line - viewport_top, ' ');
    } else {
        render_history_line(history_line);
    }
}

void console_scroll_page_up(void) {
    if (viewport_top == 0U) {
        return;
    }

    if (viewport_top > SCROLL_STEP) {
        viewport_top -= SCROLL_STEP;
    } else {
        viewport_top = 0;
    }
    render();
}

void console_scroll_page_down(void) {
    size_t bottom = bottom_viewport_top();
    if (viewport_top >= bottom) {
        viewport_top = bottom;
        render();
        return;
    }

    viewport_top += SCROLL_STEP;
    if (viewport_top > bottom) {
        viewport_top = bottom;
    }
    render();
}

void console_scroll_to_bottom(void) {
    follow_bottom(1);
}

int console_is_scrollback_active(void) {
    return viewport_top < bottom_viewport_top();
}
