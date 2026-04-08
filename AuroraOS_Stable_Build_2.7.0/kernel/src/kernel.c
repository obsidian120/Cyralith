#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"
#include "task.h"
#include "interrupts.h"
#include "timer.h"
#include "user.h"
#include "aurorafs.h"
#include "network.h"
#include "app.h"
#include "extprog.h"

static void show_boot_banner(void) {
    console_writeln("============================================================");
    console_writeln("=== AuroraOS Stable Build 2.7.0 ============================");
    console_writeln("Thank you for using AuroraOS!");
    console_writeln("Type 'help' for help.");
    console_writeln("Programmiert von Obsidian.");
    console_writeln("============================================================");
    console_writeln("");
}

static void boot_step(const char* label) {
    console_write("[boot] ");
    console_writeln(label);
}

void kernel_main(void) {
    console_init();
    boot_step("Memory");
    kmem_init();
    boot_step("Tasks");
    task_init();
    boot_step("Users");
    user_init();
    boot_step("Network core");
    network_init();
    boot_step("AuroraFS");
    afs_init();
    boot_step("Apps");
    app_init();
    boot_step("External programs");
    extprog_init();
    boot_step("Keyboard");
    keyboard_init();
    boot_step("Interrupt groundwork");
    interrupts_init();
    timer_init(100U);
    interrupts_disable();
    boot_step("Shell");

    console_clear();
    show_boot_banner();
    shell_init();

    while (1) {
        int key = keyboard_read_key();
        if (key != KEY_NONE) {
            shell_handle_key(key);
        } else {
            __asm__ volatile ("pause");
        }
    }
}
