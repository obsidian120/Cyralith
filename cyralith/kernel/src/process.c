#include "process.h"
#include "paging.h"
#include "string.h"

#define PROCESS_MAX            8U
#define PROCESS_MAX_MAPPINGS   16U
#define PROCESS_REGION_BASE    0x00800000U
#define PROCESS_REGION_SIZE    0x00080000U
#define PROCESS_DEFAULT_HEAP   2U
#define PROCESS_DEFAULT_STACK  2U
#define PROCESS_IMAGE_PAGES    1U

typedef struct {
    process_t meta;
    uint32_t mapped_frames[PROCESS_MAX_MAPPINGS];
    uint32_t mapped_vaddrs[PROCESS_MAX_MAPPINGS];
} process_slot_t;

static process_slot_t g_slots[PROCESS_MAX];
static unsigned int g_next_pid = 1U;
static int g_current_slot = -1;

static void copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (dst == (char*)0 || max == 0U) {
        return;
    }
    if (src == (const char*)0) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1U < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static int slot_from_pid(unsigned int pid) {
    size_t i;
    for (i = 0U; i < PROCESS_MAX; ++i) {
        if (g_slots[i].meta.used != 0U && g_slots[i].meta.pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static void reset_slot(process_slot_t* slot) {
    size_t i;
    if (slot == (process_slot_t*)0) {
        return;
    }
    for (i = 0U; i < PROCESS_MAX_MAPPINGS; ++i) {
        slot->mapped_frames[i] = 0U;
        slot->mapped_vaddrs[i] = 0U;
    }
    slot->meta.pid = 0U;
    slot->meta.ppid = 0U;
    slot->meta.used = 0U;
    slot->meta.state = PROCESS_STATE_FREE;
    slot->meta.kind = PROCESS_KIND_APP;
    slot->meta.name[0] = '\0';
    slot->meta.owner[0] = '\0';
    slot->meta.command[0] = '\0';
    slot->meta.runtime_ticks = 0U;
    slot->meta.switches = 0U;
    slot->meta.region_base = 0U;
    slot->meta.region_size = 0U;
    slot->meta.image_base = 0U;
    slot->meta.heap_base = 0U;
    slot->meta.stack_top = 0U;
    slot->meta.page_directory = paging_directory_address();
    slot->meta.image_pages = 0U;
    slot->meta.heap_pages = 0U;
    slot->meta.stack_pages = 0U;
    slot->meta.mapped_page_count = 0U;
    slot->meta.exit_code = 0;
}

static void cleanup_slot_memory(process_slot_t* slot) {
    unsigned int i;
    if (slot == (process_slot_t*)0) {
        return;
    }
    for (i = 0U; i < slot->meta.mapped_page_count && i < PROCESS_MAX_MAPPINGS; ++i) {
        if (slot->mapped_frames[i] != 0U) {
            paging_frame_free(slot->mapped_frames[i]);
        }
        if (slot->mapped_vaddrs[i] != 0U) {
            (void)paging_restore_identity(slot->mapped_vaddrs[i]);
        }
        slot->mapped_frames[i] = 0U;
        slot->mapped_vaddrs[i] = 0U;
    }
    slot->meta.mapped_page_count = 0U;
}

static int reserve_page(process_slot_t* slot, uint32_t virt_addr) {
    uint32_t phys;
    unsigned int index;
    if (slot == (process_slot_t*)0) {
        return -1;
    }
    index = slot->meta.mapped_page_count;
    if (index >= PROCESS_MAX_MAPPINGS) {
        return -1;
    }
    phys = paging_frame_alloc();
    if (phys == 0U) {
        return -1;
    }
    if (paging_map_page(virt_addr, phys, PAGING_FLAG_WRITABLE | PAGING_FLAG_USER) != 0) {
        paging_frame_free(phys);
        return -1;
    }
    slot->mapped_frames[index] = phys;
    slot->mapped_vaddrs[index] = virt_addr;
    slot->meta.mapped_page_count++;
    return 0;
}

static int configure_process_memory(process_slot_t* slot, size_t slot_index, unsigned int heap_pages, unsigned int stack_pages) {
    uint32_t base;
    unsigned int i;
    if (slot == (process_slot_t*)0) {
        return -1;
    }
    if (heap_pages == 0U) {
        heap_pages = PROCESS_DEFAULT_HEAP;
    }
    if (stack_pages == 0U) {
        stack_pages = PROCESS_DEFAULT_STACK;
    }
    if (PROCESS_IMAGE_PAGES + heap_pages + stack_pages > PROCESS_MAX_MAPPINGS) {
        return -1;
    }

    base = PROCESS_REGION_BASE + ((uint32_t)slot_index * PROCESS_REGION_SIZE);
    slot->meta.region_base = base;
    slot->meta.region_size = PROCESS_REGION_SIZE;
    slot->meta.image_base = base;
    slot->meta.heap_base = base + (PROCESS_IMAGE_PAGES * PAGING_PAGE_SIZE);
    slot->meta.stack_top = base + PROCESS_REGION_SIZE;
    slot->meta.image_pages = PROCESS_IMAGE_PAGES;
    slot->meta.heap_pages = heap_pages;
    slot->meta.stack_pages = stack_pages;

    for (i = 0U; i < PROCESS_IMAGE_PAGES; ++i) {
        if (reserve_page(slot, slot->meta.image_base + (i * PAGING_PAGE_SIZE)) != 0) {
            cleanup_slot_memory(slot);
            return -1;
        }
    }
    for (i = 0U; i < heap_pages; ++i) {
        if (reserve_page(slot, slot->meta.heap_base + (i * PAGING_PAGE_SIZE)) != 0) {
            cleanup_slot_memory(slot);
            return -1;
        }
    }
    for (i = 0U; i < stack_pages; ++i) {
        uint32_t virt_addr = slot->meta.stack_top - ((stack_pages - i) * PAGING_PAGE_SIZE);
        if (reserve_page(slot, virt_addr) != 0) {
            cleanup_slot_memory(slot);
            return -1;
        }
    }
    return 0;
}

static process_slot_t* allocate_slot(size_t* slot_index_out) {
    size_t i;
    for (i = 1U; i < PROCESS_MAX; ++i) {
        if (g_slots[i].meta.used == 0U || g_slots[i].meta.state == PROCESS_STATE_EXITED) {
            cleanup_slot_memory(&g_slots[i]);
            reset_slot(&g_slots[i]);
            if (slot_index_out != (size_t*)0) {
                *slot_index_out = i;
            }
            return &g_slots[i];
        }
    }
    return (process_slot_t*)0;
}

void process_init(void) {
    size_t i;
    for (i = 0U; i < PROCESS_MAX; ++i) {
        reset_slot(&g_slots[i]);
    }
    g_next_pid = 1U;
    g_current_slot = -1;
}

void process_bootstrap_shell(const char* owner) {
    process_slot_t* shell = &g_slots[0];
    reset_slot(shell);
    shell->meta.used = 1U;
    shell->meta.pid = g_next_pid++;
    shell->meta.ppid = 0U;
    shell->meta.state = PROCESS_STATE_RUNNING;
    shell->meta.kind = PROCESS_KIND_SHELL;
    copy_limited(shell->meta.name, "shell", sizeof(shell->meta.name));
    copy_limited(shell->meta.owner, owner != (const char*)0 ? owner : "guest", sizeof(shell->meta.owner));
    copy_limited(shell->meta.command, "interactive shell", sizeof(shell->meta.command));
    shell->meta.page_directory = paging_directory_address();
    g_current_slot = 0;
}

size_t process_count(void) {
    size_t i;
    size_t count = 0U;
    for (i = 0U; i < PROCESS_MAX; ++i) {
        if (g_slots[i].meta.used != 0U) {
            ++count;
        }
    }
    return count;
}

const process_t* process_get(size_t index) {
    size_t i;
    size_t current = 0U;
    for (i = 0U; i < PROCESS_MAX; ++i) {
        if (g_slots[i].meta.used != 0U) {
            if (current == index) {
                return &g_slots[i].meta;
            }
            ++current;
        }
    }
    return (const process_t*)0;
}

const process_t* process_current(void) {
    if (g_current_slot < 0) {
        return (const process_t*)0;
    }
    return &g_slots[g_current_slot].meta;
}

const process_t* process_find(unsigned int pid) {
    int slot = slot_from_pid(pid);
    if (slot < 0) {
        return (const process_t*)0;
    }
    return &g_slots[slot].meta;
}

const char* process_state_name(process_state_t state) {
    switch (state) {
        case PROCESS_STATE_READY: return "ready";
        case PROCESS_STATE_RUNNING: return "running";
        case PROCESS_STATE_SLEEPING: return "sleeping";
        case PROCESS_STATE_STOPPED: return "stopped";
        case PROCESS_STATE_EXITED: return "exited";
        default: return "free";
    }
}

const char* process_kind_name(process_kind_t kind) {
    switch (kind) {
        case PROCESS_KIND_SHELL: return "shell";
        case PROCESS_KIND_APP: return "app";
        case PROCESS_KIND_PROGRAM: return "program";
        case PROCESS_KIND_COMMAND: return "command";
        case PROCESS_KIND_SERVICE: return "service";
        default: return "unknown";
    }
}

const char* process_privilege_name(process_privilege_t privilege) {
    switch (privilege) {
        case PROCESS_PRIVILEGE_USER: return "user";
        default: return "kernel";
    }
}

void process_note_syscall(unsigned int pid, uint32_t syscall_number) {
    int slot = slot_from_pid(pid);
    if (slot < 0) {
        return;
    }
    g_slots[slot].meta.last_syscall = syscall_number;
}

int process_spawn(process_kind_t kind, const char* name, const char* command, const char* owner, unsigned int heap_pages, unsigned int stack_pages, unsigned int* pid_out) {
    size_t slot_index;
    process_slot_t* slot = allocate_slot(&slot_index);
    const process_t* current = process_current();
    if (slot == (process_slot_t*)0) {
        return -1;
    }
    slot->meta.used = 1U;
    slot->meta.pid = g_next_pid++;
    slot->meta.ppid = current != (const process_t*)0 ? current->pid : 0U;
    slot->meta.state = PROCESS_STATE_READY;
    slot->meta.kind = kind;
    copy_limited(slot->meta.name, name != (const char*)0 && name[0] != '\0' ? name : "process", sizeof(slot->meta.name));
    copy_limited(slot->meta.owner, owner != (const char*)0 && owner[0] != '\0' ? owner : "guest", sizeof(slot->meta.owner));
    copy_limited(slot->meta.command, command != (const char*)0 ? command : slot->meta.name, sizeof(slot->meta.command));
    slot->meta.page_directory = paging_directory_address();
    if (configure_process_memory(slot, slot_index, heap_pages, stack_pages) != 0) {
        reset_slot(slot);
        return -1;
    }
    if (pid_out != (unsigned int*)0) {
        *pid_out = slot->meta.pid;
    }
    return 0;
}

int process_activate(unsigned int pid) {
    int slot = slot_from_pid(pid);
    if (slot < 0) {
        return -1;
    }
    if (g_slots[slot].meta.state == PROCESS_STATE_STOPPED || g_slots[slot].meta.state == PROCESS_STATE_EXITED) {
        return -1;
    }
    if (g_current_slot >= 0 && g_slots[g_current_slot].meta.used != 0U && g_current_slot != slot && g_slots[g_current_slot].meta.state == PROCESS_STATE_RUNNING) {
        g_slots[g_current_slot].meta.state = PROCESS_STATE_READY;
    }
    if (g_current_slot != slot) {
        g_slots[slot].meta.switches++;
    }
    g_current_slot = slot;
    g_slots[slot].meta.state = PROCESS_STATE_RUNNING;
    return 0;
}

int process_stop(unsigned int pid) {
    int slot = slot_from_pid(pid);
    if (slot <= 0) {
        return -1;
    }
    if (g_slots[slot].meta.state == PROCESS_STATE_EXITED) {
        return -1;
    }
    g_slots[slot].meta.state = PROCESS_STATE_STOPPED;
    if (g_current_slot == slot) {
        g_current_slot = 0;
        if (g_slots[0].meta.used != 0U) {
            g_slots[0].meta.state = PROCESS_STATE_RUNNING;
        }
    }
    return 0;
}

int process_resume(unsigned int pid) {
    int slot = slot_from_pid(pid);
    if (slot < 0) {
        return -1;
    }
    if (g_slots[slot].meta.state != PROCESS_STATE_STOPPED && g_slots[slot].meta.state != PROCESS_STATE_SLEEPING) {
        return -1;
    }
    g_slots[slot].meta.state = PROCESS_STATE_READY;
    return 0;
}

int process_exit(unsigned int pid, int exit_code) {
    int slot = slot_from_pid(pid);
    if (slot < 0) {
        return -1;
    }
    cleanup_slot_memory(&g_slots[slot]);
    g_slots[slot].meta.exit_code = exit_code;
    g_slots[slot].meta.state = PROCESS_STATE_EXITED;
    if (g_current_slot == slot) {
        g_current_slot = 0;
        if (g_slots[0].meta.used != 0U) {
            g_slots[0].meta.state = PROCESS_STATE_RUNNING;
        }
    }
    return 0;
}

int process_kill(unsigned int pid, int exit_code) {
    return process_exit(pid, exit_code);
}

void process_scheduler_tick(void) {
    size_t i;
    int next = -1;
    if (g_current_slot >= 0 && g_slots[g_current_slot].meta.used != 0U && g_slots[g_current_slot].meta.state == PROCESS_STATE_RUNNING) {
        g_slots[g_current_slot].meta.runtime_ticks++;
    }
    for (i = 1U; i < PROCESS_MAX; ++i) {
        size_t probe = (size_t)((((g_current_slot >= 1 ? g_current_slot : 0) + (int)i) % ((int)PROCESS_MAX - 1)) + 1);
        if (g_slots[probe].meta.used != 0U && (g_slots[probe].meta.state == PROCESS_STATE_READY || g_slots[probe].meta.state == PROCESS_STATE_RUNNING)) {
            next = (int)probe;
            break;
        }
    }
    if (next >= 0 && next != g_current_slot) {
        if (g_current_slot > 0 && g_slots[g_current_slot].meta.state == PROCESS_STATE_RUNNING) {
            g_slots[g_current_slot].meta.state = PROCESS_STATE_READY;
        }
        g_current_slot = next;
        g_slots[g_current_slot].meta.state = PROCESS_STATE_RUNNING;
        g_slots[g_current_slot].meta.switches++;
    } else if (next < 0 && g_slots[0].meta.used != 0U) {
        g_current_slot = 0;
        g_slots[0].meta.state = PROCESS_STATE_RUNNING;
    }
}
