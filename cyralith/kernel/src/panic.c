#include "panic.h"
#include "console.h"
#include "recovery.h"
#include "syslog.h"
#include "string.h"
#include "io.h"
#include "timer.h"

static void panic_write_line(const char* label, const char* value) {
    console_write(label);
    console_writeln(value != (const char*)0 ? value : "-");
}

static void panic_write_separator(void) {
    console_writeln("-------------------------------------------------------------------------------");
}

static uint32_t panic_mix_u32(uint32_t seed, uint32_t value) {
    seed ^= value + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

static uint32_t panic_hash_text(uint32_t seed, const char* text) {
    if (text == (const char*)0) {
        return panic_mix_u32(seed, 0U);
    }

    while (*text != '\0') {
        seed = panic_mix_u32(seed, (uint32_t)(unsigned char)(*text));
        text++;
    }
    return seed;
}

static void panic_write_failure_id(uint32_t seed) {
    console_write("Failure ID  : CRY-");
    console_write_hex(seed);
    console_putc('\n');
}

static void panic_write_progress(uint32_t progress) {
    console_write("Progress    : ");
    console_write_dec(progress);
    console_writeln("% complete");
}

static void panic_write_header(const char* headline, const char* subline) {
    console_set_color(15U, 4U);
    console_clear();
    console_writeln("");
    console_writeln(headline != (const char*)0 ? headline : "Cyralith ran into a critical problem and had to stop.");
    console_writeln("");
    if (subline != (const char*)0 && *subline != '\0') {
        console_writeln(subline);
        console_writeln("");
    }
    console_writeln("We're collecting debug information and then the system will reboot automatically.");
    console_writeln("You can take a screenshot of this screen and share it for debugging.");
    console_writeln("");
}

static const char* panic_classify_message(const char* title, const char* message) {
    if ((title != (const char*)0 && kcontains_ci(title, "manual panic")) ||
        (message != (const char*)0 && kcontains_ci(message, "panic test"))) {
        return "Manual test panic";
    }
    if ((title != (const char*)0 && kcontains_ci(title, "memory")) ||
        (message != (const char*)0 && (kcontains_ci(message, "memory") || kcontains_ci(message, "alloc") ||
                                       kcontains_ci(message, "heap") || kcontains_ci(message, "page")))) {
        return "Memory fault";
    }
    if ((title != (const char*)0 && (kcontains_ci(title, "protect") || kcontains_ci(title, "permission") ||
                                     kcontains_ci(title, "denied"))) ||
        (message != (const char*)0 && (kcontains_ci(message, "protect") || kcontains_ci(message, "permission") ||
                                       kcontains_ci(message, "denied")))) {
        return "Protection fault";
    }
    if ((title != (const char*)0 && (kcontains_ci(title, "driver") || kcontains_ci(title, "network") ||
                                     kcontains_ci(title, "disk"))) ||
        (message != (const char*)0 && (kcontains_ci(message, "driver") || kcontains_ci(message, "network") ||
                                       kcontains_ci(message, "disk") || kcontains_ci(message, "pci")))) {
        return "Driver or device fault";
    }
    return "Kernel panic";
}

static const char* panic_classify_exception(uint32_t vector) {
    switch (vector) {
        case 0U:
        case 4U:
        case 16U:
        case 19U:
            return "Math or execution fault";
        case 6U:
            return "Invalid instruction fault";
        case 8U:
            return "Critical double fault";
        case 10U:
        case 11U:
        case 12U:
        case 13U:
        case 17U:
            return "Protection fault";
        case 14U:
            return "Memory fault";
        case 18U:
            return "Hardware fault";
        default:
            return "Kernel exception";
    }
}

static const char* panic_hint_for_message(const char* classification) {
    if (classification != (const char*)0 && kstrcmp(classification, "Manual test panic") == 0) {
        return "This panic was triggered intentionally for testing.";
    }
    if (classification != (const char*)0 && kstrcmp(classification, "Memory fault") == 0) {
        return "Check recent paging, heap, or pointer-related changes.";
    }
    if (classification != (const char*)0 && kstrcmp(classification, "Protection fault") == 0) {
        return "Check permissions, selectors, and privileged operations.";
    }
    if (classification != (const char*)0 && kstrcmp(classification, "Driver or device fault") == 0) {
        return "Try safe mode and disable the most recent driver change.";
    }
    return "Check the last command or feature added before the crash.";
}

static const char* panic_hint_for_exception(uint32_t vector) {
    switch (vector) {
        case 14U:
            return "Check CR2/fault address, paging setup, and invalid pointers.";
        case 13U:
            return "Check GDT, IDT, TSS, IRQ handlers, and privileged instructions.";
        case 8U:
            return "A second fault happened during error handling. Check early exception paths.";
        case 6U:
            return "Check function pointers, bad jumps, or corrupted code paths.";
        case 18U:
            return "This may point to a lower-level CPU or emulator issue.";
        default:
            return "Review the last low-level change made before this crash.";
    }
}

static void panic_delay_busy_round(void) {
    volatile uint32_t i;
    for (i = 0U; i < 25000000U; ++i) {
        __asm__ volatile ("pause");
    }
}

static void panic_wait_about_20_seconds(void) {
    uint32_t start = timer_ticks();
    uint32_t freq = timer_frequency();
    if (freq != 0U) {
        uint32_t target = freq * 20U;
        uint32_t spin_guard = 0U;
        while ((timer_ticks() - start) < target && spin_guard < 400000000U) {
            __asm__ volatile ("pause");
            spin_guard++;
        }
        if ((timer_ticks() - start) >= target) {
            return;
        }
    }

    {
        uint32_t sec;
        for (sec = 0U; sec < 20U; ++sec) {
            panic_delay_busy_round();
        }
    }
}

static void panic_reboot_now(void) __attribute__((noreturn));
static void panic_reboot_now(void) {
    __asm__ volatile ("cli");
    while ((inb(0x64) & 0x02U) != 0U) {
    }
    outb(0x64, 0xFEU);
    outb(0xCF9, 0x06U);
    for (;;) {
        __asm__ volatile ("pause");
    }
}

void panic_halt(void) {
    console_writeln("Auto reboot : about 20 seconds");
    panic_wait_about_20_seconds();
    panic_reboot_now();
}

void panic_show_message(const char* title, const char* message) {
    const char* classification = panic_classify_message(title, message);
    const char* hint = panic_hint_for_message(classification);
    uint32_t panic_id = 0xC0A1A11CU;
    panic_id = panic_hash_text(panic_id, title);
    panic_id = panic_hash_text(panic_id, message);
    panic_id = panic_hash_text(panic_id, classification);
    panic_id = panic_hash_text(panic_id, recovery_last_boot_stage());
    panic_id = panic_hash_text(panic_id, recovery_last_issue());

    panic_write_header(
        "Cyralith ran into a critical problem and had to stop.",
        "A fatal kernel panic occurred before the system could recover."
    );
    panic_write_progress(100U);
    panic_write_failure_id(panic_id);
    panic_write_line("Crash class : ", classification);
    panic_write_line("Stop reason : ", title != (const char*)0 ? title : "Kernel panic");
    panic_write_line("Message     : ", message != (const char*)0 ? message : "unknown fatal error");
    panic_write_line("Boot stage  : ", recovery_last_boot_stage());
    panic_write_line("Last issue  : ", recovery_last_issue());
    console_writeln("System state: panic, auto reboot armed");
    panic_write_separator();
    console_writeln("Suggested next steps:");
    console_write("- ");
    console_writeln(hint);
    console_writeln("- reboot and check 'bootinfo', 'health', and 'log errors'");
    console_writeln("- use safe mode if the crash happens again during startup");
    syslog_write(SYSLOG_ERROR, "panic", title != (const char*)0 ? title : "kernel panic");
    panic_halt();
}

void panic_show_exception(uint32_t vector, uint32_t error_code, const char* name, uint32_t fault_addr) {
    const char* classification = panic_classify_exception(vector);
    const char* hint = panic_hint_for_exception(vector);
    uint32_t panic_id = 0xC0DE0001U;
    panic_id = panic_mix_u32(panic_id, vector);
    panic_id = panic_mix_u32(panic_id, error_code);
    panic_id = panic_mix_u32(panic_id, fault_addr);
    panic_id = panic_hash_text(panic_id, name);
    panic_id = panic_hash_text(panic_id, classification);
    panic_id = panic_hash_text(panic_id, recovery_last_boot_stage());
    panic_id = panic_hash_text(panic_id, recovery_last_issue());

    panic_write_header(
        "Cyralith ran into a problem and had to stop.",
        "A fatal CPU exception interrupted the kernel and the system was halted."
    );

    panic_write_progress(100U);
    panic_write_failure_id(panic_id);
    panic_write_line("Crash class : ", classification);
    panic_write_line("Stop code   : ", name != (const char*)0 ? name : "Unknown exception");
    console_write("Vector      : ");
    console_write_dec(vector);
    console_putc('\n');
    console_write("Error code  : ");
    console_write_hex(error_code);
    console_putc('\n');
    if (vector == 14U) {
        console_write("Fault addr  : ");
        console_write_hex(fault_addr);
        console_putc('\n');
    }
    panic_write_line("Boot stage  : ", recovery_last_boot_stage());
    panic_write_line("Last issue  : ", recovery_last_issue());
    console_writeln("System state: panic, auto reboot armed");
    panic_write_separator();
    console_writeln("Suggested next steps:");
    console_write("- ");
    console_writeln(hint);
    console_writeln("- reboot and use 'bootinfo', 'health', and 'log errors'");
    console_writeln("- keep IRQs disabled while investigating early crashes");
    syslog_write(SYSLOG_ERROR, "panic", name != (const char*)0 ? name : "cpu exception");
    panic_halt();
}
