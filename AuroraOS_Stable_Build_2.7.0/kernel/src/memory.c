#include "memory.h"

#define KERNEL_HEAP_SIZE (64U * 1024U)

static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static size_t heap_offset = 0;

static size_t align8(size_t value) {
    return (value + 7U) & ~((size_t)7U);
}

void kmem_init(void) {
    heap_offset = 0;
}

void* kmalloc(size_t size) {
    size_t aligned = align8(size);
    if (aligned == 0) {
        return (void*)0;
    }

    if (heap_offset + aligned > KERNEL_HEAP_SIZE) {
        return (void*)0;
    }

    void* ptr = &kernel_heap[heap_offset];
    heap_offset += aligned;
    return ptr;
}

size_t kmem_total_bytes(void) {
    return KERNEL_HEAP_SIZE;
}

size_t kmem_used_bytes(void) {
    return heap_offset;
}

size_t kmem_free_bytes(void) {
    return KERNEL_HEAP_SIZE - heap_offset;
}
