#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGING_PAGE_SIZE 4096U
#define PAGING_KERNEL_DIRECTORY_POOL 10U

enum {
    PAGING_FLAG_PRESENT  = 1U << 0,
    PAGING_FLAG_WRITABLE = 1U << 1,
    PAGING_FLAG_USER     = 1U << 2
};

typedef struct {
    uint32_t enabled;
    uint32_t page_size;
    uint32_t total_frames;
    uint32_t reserved_frames;
    uint32_t used_frames;
    uint32_t free_frames;
    uint32_t mapped_pages;
    uint32_t fault_count;
    uint32_t last_fault_address;
    uint32_t last_fault_error;
    uint32_t directories_total;
    uint32_t directories_used;
    uint32_t current_directory;
} paging_status_t;

void paging_init(void);
int paging_enabled(void);
uint32_t paging_directory_address(void);
uint32_t paging_current_directory_address(void);
uint32_t paging_frame_alloc(void);
void paging_frame_free(uint32_t phys_addr);
int paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
int paging_restore_identity(uint32_t virt_addr);
uint32_t paging_directory_create(void);
void paging_directory_destroy(uint32_t directory_addr);
int paging_map_page_in_directory(uint32_t directory_addr, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
int paging_restore_identity_in_directory(uint32_t directory_addr, uint32_t virt_addr);
int paging_switch_directory(uint32_t directory_addr);
void paging_note_fault(uint32_t error_code);
uint32_t paging_fault_count(void);
uint32_t paging_last_fault_address(void);
uint32_t paging_last_fault_error(void);
void paging_get_status(paging_status_t* out);

#endif
