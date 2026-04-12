#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
void timer_handle_interrupt(void);
uint32_t timer_ticks(void);
uint32_t timer_frequency(void);

#endif
