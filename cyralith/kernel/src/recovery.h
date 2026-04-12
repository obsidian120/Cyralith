#ifndef RECOVERY_H
#define RECOVERY_H

#include <stdint.h>

void recovery_init(void);
void recovery_boot_begin(void);
void recovery_boot_stage(const char* stage);
void recovery_boot_ready(void);
void recovery_note_issue(const char* issue);
const char* recovery_last_issue(void);
void recovery_clear_issue(void);
int recovery_safe_mode_enabled(void);
void recovery_set_safe_mode(int enabled);
uint32_t recovery_boot_count(void);
uint32_t recovery_unexpected_shutdown_count(void);
uint32_t recovery_startup_failure_count(void);
uint32_t recovery_auto_recovery_count(void);
const char* recovery_last_boot_stage(void);
int recovery_session_open(void);
int recovery_mark_clean_shutdown(const char* reason);
int recovery_save_persistent(void);
int recovery_load_persistent(void);

#endif
