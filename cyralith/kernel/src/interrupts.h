#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

typedef struct interrupt_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error_code;
} interrupt_frame_t;

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void interrupt_handler_c(interrupt_frame_t* frame);

#endif
