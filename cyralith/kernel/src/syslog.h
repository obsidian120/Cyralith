#ifndef CYRALITH_SYSLOG_H
#define CYRALITH_SYSLOG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SYSLOG_DEBUG = 0,
    SYSLOG_INFO = 1,
    SYSLOG_WARN = 2,
    SYSLOG_ERROR = 3
} syslog_level_t;

typedef struct {
    uint32_t sequence;
    uint32_t tick;
    uint8_t level;
    char source[20];
    char message[92];
} syslog_entry_t;

void syslog_init(void);
void syslog_write(syslog_level_t level, const char* source, const char* message);
void syslog_clear(void);
size_t syslog_count(void);
const syslog_entry_t* syslog_get_recent(size_t recent_index);
const char* syslog_level_name(syslog_level_t level);
int syslog_save_persistent(void);
int syslog_load_persistent(void);

#endif
