#include "actionlog.h"
#include "string.h"
#include "timer.h"

#define ACTIONLOG_MAX 32U

static actionlog_entry_t g_entries[ACTIONLOG_MAX];
static size_t g_head = 0U;
static size_t g_count = 0U;

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

void actionlog_init(void) {
    g_head = 0U;
    g_count = 0U;
}

void actionlog_add(const char* actor, const char* action, const char* detail, actionlog_result_t result) {
    actionlog_entry_t* entry = &g_entries[g_head];
    entry->tick = timer_ticks();
    copy_limited(entry->actor, actor != (const char*)0 ? actor : "system", sizeof(entry->actor));
    copy_limited(entry->action, action != (const char*)0 ? action : "action", sizeof(entry->action));
    copy_limited(entry->detail, detail != (const char*)0 ? detail : "", sizeof(entry->detail));
    entry->result = result;

    g_head = (g_head + 1U) % ACTIONLOG_MAX;
    if (g_count < ACTIONLOG_MAX) {
        g_count++;
    }
}

size_t actionlog_count(void) {
    return g_count;
}

const actionlog_entry_t* actionlog_get_recent(size_t recent_index) {
    size_t real_index;
    if (recent_index >= g_count) {
        return (const actionlog_entry_t*)0;
    }
    real_index = (g_head + ACTIONLOG_MAX - 1U - recent_index) % ACTIONLOG_MAX;
    return &g_entries[real_index];
}

const char* actionlog_result_name(actionlog_result_t result) {
    switch (result) {
        case ACTIONLOG_OK: return "ok";
        case ACTIONLOG_WARN: return "warn";
        case ACTIONLOG_DENY: return "deny";
        case ACTIONLOG_FAIL: return "fail";
        default: return "unknown";
    }
}
