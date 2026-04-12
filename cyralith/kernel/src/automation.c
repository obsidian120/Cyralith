#include "automation.h"
#include "string.h"
#include "timer.h"

#define AUTOMATION_MAX 8U

static automation_job_t g_jobs[AUTOMATION_MAX];
static unsigned int g_next_id = 1U;
static automation_run_fn g_runner = (automation_run_fn)0;

static void copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (max == 0U) {
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

void automation_init(void) {
    size_t i;
    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        g_jobs[i].id = 0U;
        g_jobs[i].used = 0U;
        g_jobs[i].done = 0U;
        g_jobs[i].due_tick = 0U;
        g_jobs[i].command[0] = '\0';
    }
    g_next_id = 1U;
}

void automation_bind_runner(automation_run_fn fn) {
    g_runner = fn;
}

int automation_schedule_in_seconds(unsigned int seconds, const char* command, unsigned int* id_out) {
    size_t i;
    uint32_t now = timer_ticks();
    uint32_t delay = seconds * timer_frequency();

    if (command == (const char*)0 || command[0] == '\0') {
        return -1;
    }
    if (seconds == 0U) {
        seconds = 1U;
        delay = timer_frequency();
    }

    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        if (g_jobs[i].used == 0U || g_jobs[i].done != 0U) {
            g_jobs[i].used = 1U;
            g_jobs[i].done = 0U;
            g_jobs[i].id = g_next_id++;
            g_jobs[i].due_tick = now + delay;
            copy_limited(g_jobs[i].command, command, sizeof(g_jobs[i].command));
            if (id_out != (unsigned int*)0) {
                *id_out = g_jobs[i].id;
            }
            return 0;
        }
    }
    return -1;
}

int automation_cancel(unsigned int id) {
    size_t i;
    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        if (g_jobs[i].used != 0U && g_jobs[i].done == 0U && g_jobs[i].id == id) {
            g_jobs[i].done = 1U;
            return 0;
        }
    }
    return -1;
}

void automation_poll(void) {
    size_t i;
    uint32_t now = timer_ticks();
    if (g_runner == (automation_run_fn)0) {
        return;
    }
    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        if (g_jobs[i].used != 0U && g_jobs[i].done == 0U && now >= g_jobs[i].due_tick) {
            g_jobs[i].done = 1U;
            g_runner(g_jobs[i].command);
        }
    }
}

size_t automation_count(void) {
    size_t count = 0U;
    size_t i;
    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        if (g_jobs[i].used != 0U && g_jobs[i].done == 0U) {
            ++count;
        }
    }
    return count;
}

const automation_job_t* automation_get(size_t index) {
    size_t i;
    size_t current = 0U;
    for (i = 0U; i < AUTOMATION_MAX; ++i) {
        if (g_jobs[i].used != 0U && g_jobs[i].done == 0U) {
            if (current == index) {
                return &g_jobs[i];
            }
            ++current;
        }
    }
    return (const automation_job_t*)0;
}
