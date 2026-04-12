#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"
#include "task.h"
#include "interrupts.h"
#include "timer.h"
#include "user.h"
#include "cyralithfs.h"
#include "network.h"
#include "app.h"
#include "extprog.h"
#include "actionlog.h"
#include "automation.h"
#include "recovery.h"
#include "paging.h"
#include "process.h"
#include "arcade.h"
#include "syslog.h"

static void show_boot_banner(void) {
    console_writeln("============================================================");
    console_writeln("=== Cyralith Stable Build 2.7.0 ============================");
    console_writeln("Thank you for using Cyralith!");
    console_writeln("Type 'help' for help.");
    console_writeln("Programmiert von Obsidian.");
    console_writeln("============================================================");
    console_writeln("");
}

static void boot_step(const char* label) {
    console_write("[boot] ");
    console_writeln(label);
    recovery_boot_stage(label);
    syslog_write(SYSLOG_INFO, "boot", label);
}

void kernel_main(void) {
    console_init();
    recovery_init();
    syslog_init();
    recovery_boot_begin();
    syslog_write(SYSLOG_INFO, "boot", "kernel entry");
    if (recovery_safe_mode_enabled() != 0) {
        syslog_write(SYSLOG_WARN, "recovery", "safe mode enabled");
    }
    boot_step("Memory");
    kmem_init();
    boot_step("Tasks");
    task_init();
    actionlog_init();
    automation_init();
    boot_step("Users");
    user_init();
    boot_step("Paging");
    paging_init();
    boot_step("Processes");
    process_init();
    process_bootstrap_shell(user_current()->username);
    boot_step("Network core");
    network_init();
    boot_step("CyralithFS");
    afs_init();
    boot_step("Apps");
    app_init();
    boot_step("Arcade");
    arcade_init();
    boot_step("External programs");
    extprog_init();
    boot_step("Keyboard");
    keyboard_init();
    boot_step("Interrupt groundwork");
    interrupts_init();
    timer_init(100U);
    interrupts_disable();
    boot_step("Shell (IRQ safe mode)");

    console_clear();
    show_boot_banner();
    if (recovery_safe_mode_enabled() != 0) {
        console_writeln("[safe] Reliability safe mode is enabled. Use 'safemode off' after checks.");
        console_writeln("");
    }
    shell_init();
    recovery_boot_ready();
    syslog_write(SYSLOG_INFO, "boot", "shell ready");

    {
        uint32_t last_tick = 0U;
        while (1) {
            uint32_t now = timer_ticks();
            while (last_tick != now) {
                last_tick++;
                task_scheduler_tick();
                process_scheduler_tick();
                automation_poll();
            }

            {
                int key = keyboard_read_key();
                if (key != KEY_NONE) {
                    shell_handle_key(key);
                } else {
                    shell_poll();
                    __asm__ volatile ("pause");
                }
            }
        }
    }
}
