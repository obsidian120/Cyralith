#ifndef AUTOMATION_H
#define AUTOMATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned int id;
    unsigned int used;
    unsigned int done;
    uint32_t due_tick;
    char command[128];
} automation_job_t;

typedef void (*automation_run_fn)(const char* command);

void automation_init(void);
void automation_bind_runner(automation_run_fn fn);
int automation_schedule_in_seconds(unsigned int seconds, const char* command, unsigned int* id_out);
int automation_cancel(unsigned int id);
void automation_poll(void);
size_t automation_count(void);
const automation_job_t* automation_get(size_t index);

#endif
