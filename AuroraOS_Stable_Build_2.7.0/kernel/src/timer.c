#include "timer.h"
#include "io.h"

#define PIT_BASE_FREQUENCY 1193180U

static uint32_t g_timer_ticks = 0;
static uint32_t g_timer_frequency = 100U;

void timer_init(uint32_t frequency_hz) {
    uint32_t divisor;

    if (frequency_hz == 0U) {
        frequency_hz = 100U;
    }

    g_timer_ticks = 0;
    g_timer_frequency = frequency_hz;
    divisor = PIT_BASE_FREQUENCY / frequency_hz;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFFU));
    outb(0x40, (uint8_t)((divisor >> 8U) & 0xFFU));
}

void timer_handle_interrupt(void) {
    g_timer_ticks++;
}

uint32_t timer_ticks(void) {
    return g_timer_ticks;
}

uint32_t timer_frequency(void) {
    return g_timer_frequency;
}
