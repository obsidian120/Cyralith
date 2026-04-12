#ifndef ACTIONLOG_H
#define ACTIONLOG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ACTIONLOG_OK = 0,
    ACTIONLOG_WARN = 1,
    ACTIONLOG_DENY = 2,
    ACTIONLOG_FAIL = 3
} actionlog_result_t;

typedef struct {
    uint32_t tick;
    char actor[24];
    char action[32];
    char detail[96];
    actionlog_result_t result;
} actionlog_entry_t;

void actionlog_init(void);
void actionlog_add(const char* actor, const char* action, const char* detail, actionlog_result_t result);
size_t actionlog_count(void);
const actionlog_entry_t* actionlog_get_recent(size_t recent_index);
const char* actionlog_result_name(actionlog_result_t result);

#endif
