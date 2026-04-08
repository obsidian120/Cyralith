#ifndef APP_H
#define APP_H

#include <stddef.h>

typedef struct {
    char name[16];
    char title[24];
    char description[72];
    unsigned int installed;
    unsigned int builtin;
    unsigned int runnable;
} app_t;

void app_init(void);
size_t app_count(void);
const app_t* app_get(size_t index);
const app_t* app_find(const char* name);
int app_install(const char* name);
int app_remove(const char* name);
int app_is_installed(const char* name);
int app_persistence_available(void);
int app_save_persistent(void);
int app_load_persistent(void);

#endif
