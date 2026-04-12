#include "syscall.h"
#include "interrupts.h"
#include "process.h"

static uint32_t g_last_syscall = 0U;
static uint32_t g_total_syscalls = 0U;

void syscall_init(void) {
    g_last_syscall = 0U;
    g_total_syscalls = 0U;
}

const char* syscall_name(uint32_t number) {
    switch (number) {
        case SYSCALL_YIELD: return "yield";
        case SYSCALL_EXIT: return "exit";
        case SYSCALL_GETPID: return "getpid";
        default: return "unknown";
    }
}

int syscall_dispatch(struct interrupt_frame* frame) {
    const process_t* current;
    if (frame == (struct interrupt_frame*)0) {
        return -1;
    }

    g_last_syscall = frame->eax;
    g_total_syscalls++;
    current = process_current();
    if (current == (const process_t*)0) {
        frame->eax = 0xFFFFFFFFU;
        return -1;
    }

    switch (frame->eax) {
        case SYSCALL_YIELD:
            process_note_syscall(current->pid, frame->eax);
            process_scheduler_tick();
            frame->eax = 0U;
            return 0;
        case SYSCALL_EXIT:
            process_note_syscall(current->pid, frame->eax);
            (void)process_exit(current->pid, (int)frame->ebx);
            (void)process_activate(1U);
            frame->eax = 0U;
            return 0;
        case SYSCALL_GETPID:
            process_note_syscall(current->pid, frame->eax);
            frame->eax = current->pid;
            return 0;
        default:
            process_note_syscall(current->pid, frame->eax);
            frame->eax = 0xFFFFFFFFU;
            return -1;
    }
}

uint32_t syscall_last_number(void) {
    return g_last_syscall;
}

uint32_t syscall_total_calls(void) {
    return g_total_syscalls;
}
