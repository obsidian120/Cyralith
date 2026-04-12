#ifndef CYRALITH_PANIC_H
#define CYRALITH_PANIC_H

#include <stdint.h>

void panic_show_exception(uint32_t vector, uint32_t error_code, const char* name, uint32_t fault_addr);
void panic_show_message(const char* title, const char* message);
void panic_halt(void) __attribute__((noreturn));

#endif
