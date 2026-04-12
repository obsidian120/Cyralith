#include "paging.h"

#define PAGING_IDENTITY_BYTES   (16U * 1024U * 1024U)
#define PAGING_TABLE_COUNT      (PAGING_IDENTITY_BYTES / (1024U * PAGING_PAGE_SIZE))
#define PAGING_TOTAL_FRAMES     (PAGING_IDENTITY_BYTES / PAGING_PAGE_SIZE)
#define PAGING_RESERVED_FRAMES  (4U * 1024U * 1024U / PAGING_PAGE_SIZE)

enum {
    FRAME_FREE = 0,
    FRAME_RESERVED = 1,
    FRAME_USED = 2
};

static uint32_t g_page_directory_pool[PAGING_KERNEL_DIRECTORY_POOL][1024] __attribute__((aligned(4096)));
static uint32_t g_page_tables_pool[PAGING_KERNEL_DIRECTORY_POOL][PAGING_TABLE_COUNT][1024] __attribute__((aligned(4096)));
static uint8_t g_directory_used[PAGING_KERNEL_DIRECTORY_POOL];
static uint8_t g_frame_state[PAGING_TOTAL_FRAMES];
static uint32_t g_fault_count = 0U;
static uint32_t g_last_fault_address = 0U;
static uint32_t g_last_fault_error = 0U;
static uint32_t g_enabled = 0U;
static uint32_t g_current_directory = 0U;

static void paging_enable_hw(uint32_t directory) {
    uint32_t cr0;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(directory));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

static void paging_load_directory(uint32_t directory) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(directory));
}

static void paging_invalidate(uint32_t virt_addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"((void*)virt_addr) : "memory");
}

static int directory_index_from_address(uint32_t directory_addr) {
    uint32_t i;
    for (i = 0U; i < PAGING_KERNEL_DIRECTORY_POOL; ++i) {
        if ((uint32_t)&g_page_directory_pool[i][0] == directory_addr) {
            return (int)i;
        }
    }
    return -1;
}

static void clone_kernel_identity_into(uint32_t slot) {
    uint32_t table;
    uint32_t page;

    for (table = 0U; table < PAGING_TABLE_COUNT; ++table) {
        for (page = 0U; page < 1024U; ++page) {
            uint32_t phys = ((table * 1024U) + page) * PAGING_PAGE_SIZE;
            g_page_tables_pool[slot][table][page] = phys | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE;
        }
        g_page_directory_pool[slot][table] = ((uint32_t)&g_page_tables_pool[slot][table][0]) | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE;
    }

    for (table = PAGING_TABLE_COUNT; table < 1024U; ++table) {
        g_page_directory_pool[slot][table] = 0U;
    }
}

void paging_init(void) {
    uint32_t i;
    uint32_t slot;

    for (slot = 0U; slot < PAGING_KERNEL_DIRECTORY_POOL; ++slot) {
        g_directory_used[slot] = 0U;
        clone_kernel_identity_into(slot);
    }
    g_directory_used[0] = 1U;

    for (i = 0U; i < PAGING_TOTAL_FRAMES; ++i) {
        g_frame_state[i] = (i < PAGING_RESERVED_FRAMES) ? FRAME_RESERVED : FRAME_FREE;
    }

    g_fault_count = 0U;
    g_last_fault_address = 0U;
    g_last_fault_error = 0U;
    g_current_directory = (uint32_t)&g_page_directory_pool[0][0];
    paging_enable_hw(g_current_directory);
    g_enabled = 1U;
}

int paging_enabled(void) {
    return g_enabled != 0U ? 1 : 0;
}

uint32_t paging_directory_address(void) {
    return (uint32_t)&g_page_directory_pool[0][0];
}

uint32_t paging_current_directory_address(void) {
    return g_current_directory;
}

uint32_t paging_frame_alloc(void) {
    uint32_t i;
    for (i = PAGING_RESERVED_FRAMES; i < PAGING_TOTAL_FRAMES; ++i) {
        if (g_frame_state[i] == FRAME_FREE) {
            g_frame_state[i] = FRAME_USED;
            return i * PAGING_PAGE_SIZE;
        }
    }
    return 0U;
}

void paging_frame_free(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PAGING_PAGE_SIZE;
    if ((phys_addr % PAGING_PAGE_SIZE) != 0U) {
        return;
    }
    if (frame >= PAGING_TOTAL_FRAMES) {
        return;
    }
    if (g_frame_state[frame] == FRAME_USED) {
        g_frame_state[frame] = FRAME_FREE;
    }
}

int paging_map_page_in_directory(uint32_t directory_addr, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    int slot;
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t entry_flags = (flags | PAGING_FLAG_PRESENT) & 0xFFFU;

    if ((virt_addr % PAGING_PAGE_SIZE) != 0U || (phys_addr % PAGING_PAGE_SIZE) != 0U) {
        return -1;
    }
    if (virt_addr >= PAGING_IDENTITY_BYTES || phys_addr >= PAGING_IDENTITY_BYTES) {
        return -1;
    }

    slot = directory_index_from_address(directory_addr);
    if (slot < 0) {
        return -1;
    }

    directory_index = virt_addr >> 22U;
    table_index = (virt_addr >> 12U) & 0x3FFU;
    if (directory_index >= PAGING_TABLE_COUNT) {
        return -1;
    }

    g_page_tables_pool[slot][directory_index][table_index] = (phys_addr & 0xFFFFF000U) | entry_flags;
    if (g_current_directory == directory_addr) {
        paging_invalidate(virt_addr);
    }
    return 0;
}

int paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    return paging_map_page_in_directory(g_current_directory, virt_addr, phys_addr, flags);
}

int paging_restore_identity_in_directory(uint32_t directory_addr, uint32_t virt_addr) {
    return paging_map_page_in_directory(directory_addr, virt_addr, virt_addr, PAGING_FLAG_WRITABLE);
}

int paging_restore_identity(uint32_t virt_addr) {
    return paging_restore_identity_in_directory(g_current_directory, virt_addr);
}

uint32_t paging_directory_create(void) {
    uint32_t slot;
    for (slot = 1U; slot < PAGING_KERNEL_DIRECTORY_POOL; ++slot) {
        if (g_directory_used[slot] == 0U) {
            clone_kernel_identity_into(slot);
            g_directory_used[slot] = 1U;
            return (uint32_t)&g_page_directory_pool[slot][0];
        }
    }
    return 0U;
}

void paging_directory_destroy(uint32_t directory_addr) {
    int slot = directory_index_from_address(directory_addr);
    if (slot <= 0) {
        return;
    }
    clone_kernel_identity_into((uint32_t)slot);
    g_directory_used[slot] = 0U;
    if (g_current_directory == directory_addr) {
        g_current_directory = paging_directory_address();
        paging_load_directory(g_current_directory);
    }
}

int paging_switch_directory(uint32_t directory_addr) {
    int slot = directory_index_from_address(directory_addr);
    if (slot < 0 || g_directory_used[slot] == 0U) {
        return -1;
    }
    g_current_directory = directory_addr;
    paging_load_directory(directory_addr);
    return 0;
}

void paging_note_fault(uint32_t error_code) {
    uint32_t fault_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
    g_fault_count++;
    g_last_fault_address = fault_addr;
    g_last_fault_error = error_code;
}

uint32_t paging_fault_count(void) {
    return g_fault_count;
}

uint32_t paging_last_fault_address(void) {
    return g_last_fault_address;
}

uint32_t paging_last_fault_error(void) {
    return g_last_fault_error;
}

void paging_get_status(paging_status_t* out) {
    uint32_t free_count = 0U;
    uint32_t used_count = 0U;
    uint32_t reserved_count = 0U;
    uint32_t dir_used = 0U;
    uint32_t i;

    if (out == (paging_status_t*)0) {
        return;
    }

    for (i = 0U; i < PAGING_TOTAL_FRAMES; ++i) {
        if (g_frame_state[i] == FRAME_FREE) {
            free_count++;
        } else if (g_frame_state[i] == FRAME_USED) {
            used_count++;
        } else {
            reserved_count++;
        }
    }

    for (i = 0U; i < PAGING_KERNEL_DIRECTORY_POOL; ++i) {
        if (g_directory_used[i] != 0U) {
            dir_used++;
        }
    }

    out->enabled = g_enabled;
    out->page_size = PAGING_PAGE_SIZE;
    out->total_frames = PAGING_TOTAL_FRAMES;
    out->reserved_frames = reserved_count;
    out->used_frames = used_count;
    out->free_frames = free_count;
    out->mapped_pages = PAGING_TOTAL_FRAMES;
    out->fault_count = g_fault_count;
    out->last_fault_address = g_last_fault_address;
    out->last_fault_error = g_last_fault_error;
    out->directories_total = PAGING_KERNEL_DIRECTORY_POOL;
    out->directories_used = dir_used;
    out->current_directory = g_current_directory;
}
