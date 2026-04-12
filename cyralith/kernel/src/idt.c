#include "idt.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);
extern void isr128(void);

static void lidt(const idt_ptr_t* ptr) {
    __asm__ volatile ("lidtl (%0)" : : "r"(ptr));
}

void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFFU);
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].flags = flags;
    idt[vector].offset_high = (uint16_t)((handler >> 16U) & 0xFFFFU);
}

void idt_init(void) {
    for (unsigned int i = 0; i < 256U; ++i) {
        idt_set_gate((uint8_t)i, 0U, 0x08U, 0x8EU);
    }

#define SET_ISR(n) idt_set_gate((uint8_t)(n), (uint32_t)isr##n, 0x08U, 0x8EU)
#define SET_IRQ(n) idt_set_gate((uint8_t)(32 + (n)), (uint32_t)irq##n, 0x08U, 0x8EU)
    SET_ISR(0);
    SET_ISR(1);
    SET_ISR(2);
    SET_ISR(3);
    SET_ISR(4);
    SET_ISR(5);
    SET_ISR(6);
    SET_ISR(7);
    SET_ISR(8);
    SET_ISR(9);
    SET_ISR(10);
    SET_ISR(11);
    SET_ISR(12);
    SET_ISR(13);
    SET_ISR(14);
    SET_ISR(15);
    SET_ISR(16);
    SET_ISR(17);
    SET_ISR(18);
    SET_ISR(19);
    SET_ISR(20);
    SET_ISR(21);
    SET_ISR(22);
    SET_ISR(23);
    SET_ISR(24);
    SET_ISR(25);
    SET_ISR(26);
    SET_ISR(27);
    SET_ISR(28);
    SET_ISR(29);
    SET_ISR(30);
    SET_ISR(31);

    SET_IRQ(0);
    SET_IRQ(1);
    SET_IRQ(2);
    SET_IRQ(3);
    SET_IRQ(4);
    SET_IRQ(5);
    SET_IRQ(6);
    SET_IRQ(7);
    SET_IRQ(8);
    SET_IRQ(9);
    SET_IRQ(10);
    SET_IRQ(11);
    SET_IRQ(12);
    SET_IRQ(13);
    SET_IRQ(14);
    SET_IRQ(15);
    idt_set_gate(128U, (uint32_t)isr128, 0x08U, 0xEEU);
#undef SET_ISR
#undef SET_IRQ

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1U);
    idt_ptr.base = (uint32_t)&idt[0];
    lidt(&idt_ptr);
}
