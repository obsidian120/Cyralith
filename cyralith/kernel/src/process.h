#ifndef PROCESS_H
#define PROCESS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PROCESS_STATE_FREE = 0,
    PROCESS_STATE_READY = 1,
    PROCESS_STATE_RUNNING = 2,
    PROCESS_STATE_SLEEPING = 3,
    PROCESS_STATE_STOPPED = 4,
    PROCESS_STATE_EXITED = 5
} process_state_t;

typedef enum {
    PROCESS_KIND_SHELL = 0,
    PROCESS_KIND_APP = 1,
    PROCESS_KIND_PROGRAM = 2,
    PROCESS_KIND_COMMAND = 3,
    PROCESS_KIND_SERVICE = 4
} process_kind_t;

typedef enum {
    PROCESS_PRIVILEGE_KERNEL = 0,
    PROCESS_PRIVILEGE_USER = 1
} process_privilege_t;

typedef struct {
    unsigned int pid;
    unsigned int ppid;
    unsigned int used;
    process_state_t state;
    process_kind_t kind;
    process_privilege_t privilege;
    char name[32];
    char owner[16];
    char command[96];
    uint32_t runtime_ticks;
    uint32_t switches;
    uint32_t region_base;
    uint32_t region_size;
    uint32_t image_base;
    uint32_t heap_base;
    uint32_t stack_top;
    uint32_t page_directory;
    uint32_t kernel_stack_top;
    uint32_t last_syscall;
    unsigned int image_pages;
    unsigned int heap_pages;
    unsigned int stack_pages;
    unsigned int mapped_page_count;
    int exit_code;
} process_t;

void process_init(void);
void process_bootstrap_shell(const char* owner);
size_t process_count(void);
const process_t* process_get(size_t index);
const process_t* process_current(void);
const process_t* process_find(unsigned int pid);
const char* process_state_name(process_state_t state);
const char* process_kind_name(process_kind_t kind);
const char* process_privilege_name(process_privilege_t privilege);
int process_spawn(process_kind_t kind, const char* name, const char* command, const char* owner, unsigned int heap_pages, unsigned int stack_pages, unsigned int* pid_out);
int process_activate(unsigned int pid);
int process_stop(unsigned int pid);
int process_resume(unsigned int pid);
int process_exit(unsigned int pid, int exit_code);
int process_kill(unsigned int pid, int exit_code);
void process_note_syscall(unsigned int pid, uint32_t syscall_number);
void process_scheduler_tick(void);

#endif
