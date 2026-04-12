#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

struct interrupt_frame;

enum {
    SYSCALL_YIELD = 0U,
    SYSCALL_EXIT = 1U,
    SYSCALL_GETPID = 2U
};

void syscall_init(void);
int syscall_dispatch(struct interrupt_frame* frame);
uint32_t syscall_last_number(void);
uint32_t syscall_total_calls(void);
const char* syscall_name(uint32_t number);

#endif
