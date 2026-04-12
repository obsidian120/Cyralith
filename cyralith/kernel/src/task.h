#ifndef TASK_H
#define TASK_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TASK_STOPPED = 0,
    TASK_RUNNING = 1
} task_state_t;

typedef struct {
    const char* name;
    task_state_t state;
    unsigned int priority;
    const char* description;
    uint32_t runtime_ticks;
    uint32_t switches;
} task_t;

void task_init(void);
size_t task_count(void);
const task_t* task_get(size_t index);
int task_start(const char* name);
int task_stop(const char* name);
const char* task_state_name(task_state_t state);
void task_scheduler_tick(void);
const task_t* task_current(void);
uint32_t task_scheduler_ticks(void);
const char* task_scheduler_name(void);

#endif
