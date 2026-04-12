#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>

void console_init(void);
void console_clear(void);
void console_set_color(uint8_t fg, uint8_t bg);
void console_putc(char c);
void console_write(const char* str);
void console_writeln(const char* str);
void console_write_hex(uint32_t value);
void console_write_dec(uint32_t value);
void console_backspace(void);
void console_scroll_page_up(void);
void console_scroll_page_down(void);
void console_scroll_to_bottom(void);
int console_is_scrollback_active(void);

#endif
