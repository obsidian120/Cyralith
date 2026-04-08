#ifndef TASK_H
#define TASK_H

#include <stddef.h>

typedef enum {
    TASK_STOPPED = 0,
    TASK_RUNNING = 1
} task_state_t;

typedef struct {
    const char* name;
    task_state_t state;
    unsigned int priority;
    const char* description;
} task_t;

void task_init(void);
size_t task_count(void);
const task_t* task_get(size_t index);
int task_start(const char* name);
int task_stop(const char* name);
const char* task_state_name(task_state_t state);

#endif
