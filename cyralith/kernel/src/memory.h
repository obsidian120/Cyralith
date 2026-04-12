#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

void kmem_init(void);
void* kmalloc(size_t size);
size_t kmem_total_bytes(void);
size_t kmem_used_bytes(void);
size_t kmem_free_bytes(void);

#endif
