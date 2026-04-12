#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE_SELECTOR 0x08U
#define GDT_KERNEL_DATA_SELECTOR 0x10U
#define GDT_USER_CODE_SELECTOR   0x1BU
#define GDT_USER_DATA_SELECTOR   0x23U
#define GDT_TSS_SELECTOR         0x28U

void gdt_init(uint32_t kernel_stack_top);
void gdt_set_kernel_stack(uint32_t kernel_stack_top);
uint32_t gdt_kernel_stack_top(void);

#endif
