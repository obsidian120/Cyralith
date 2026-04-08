#include "interrupts.h"
#include "idt.h"
#include "io.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"

static const char* exception_name(uint32_t vector) {
    static const char* names[] = {
        "Division by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
        "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
        "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
        "Stack fault", "General protection fault", "Page fault", "Reserved",
        "x87 floating point", "Alignment check", "Machine check", "SIMD floating point",
        "Virtualization", "Control protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor injection", "VMM communication", "Security", "Reserved"
    };

    if (vector < 32U) {
        return names[vector];
    }
    return "Unknown exception";
}

static void pic_send_eoi(uint32_t irq_vector) {
    if (irq_vector >= 40U) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

static void pic_remap(void) {
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask = inb(0xA1);

    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, master_mask);
    outb(0xA1, slave_mask);
}

static void pic_set_mask(uint8_t irq_line, int masked) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8U) {
        port = 0x21U;
    } else {
        port = 0xA1U;
        irq_line = (uint8_t)(irq_line - 8U);
    }

    value = inb(port);
    if (masked != 0) {
        value = (uint8_t)(value | (uint8_t)(1U << irq_line));
    } else {
        value = (uint8_t)(value & (uint8_t)~(1U << irq_line));
    }
    outb(port, value);
}

void interrupts_init(void) {
    __asm__ volatile ("cli");
    idt_init();
    pic_remap();

    for (uint8_t irq = 0; irq < 16U; ++irq) {
        pic_set_mask(irq, 1);
    }

    pic_set_mask(0, 0);
    pic_set_mask(1, 0);
    pic_set_mask(2, 0);
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}

void interrupt_handler_c(uint32_t vector, uint32_t error_code) {
    if (vector == 32U) {
        timer_handle_interrupt();
        pic_send_eoi(vector);
        return;
    }

    if (vector == 33U) {
        keyboard_handle_interrupt_scancode(inb(0x60));
        pic_send_eoi(vector);
        return;
    }

    if (vector >= 32U && vector <= 47U) {
        pic_send_eoi(vector);
        return;
    }

    console_writeln("");
    console_writeln("[interrupt] CPU exception detected.");
    console_write("[interrupt] vector=");
    console_write_dec(vector);
    console_write(" error=");
    console_write_dec(error_code);
    console_putc('\n');
    console_write("[interrupt] ");
    console_writeln(exception_name(vector));
    console_writeln("[interrupt] kernel halted.");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}
