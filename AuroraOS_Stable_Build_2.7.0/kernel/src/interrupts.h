#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void interrupt_handler_c(uint32_t vector, uint32_t error_code);

#endif
