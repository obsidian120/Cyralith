#ifndef USER_H
#define USER_H

#include <stddef.h>

typedef enum {
    USER_ROLE_USER = 0,
    USER_ROLE_SYSTEM = 1
} user_role_t;

typedef struct {
    char username[16];
    char display_name[32];
    char password[24];
    char home[48];
    char group[16];
    user_role_t role;
} user_t;

void user_init(void);
size_t user_count(void);
const user_t* user_get(size_t index);
const user_t* user_current(void);
const user_t* user_previous(void);
int user_login(const char* username, const char* password);
int user_elevate(const char* password);
void user_drop(void);
int user_is_master(void);
const char* user_role_name(user_role_t role);
int user_set_password(const char* username, const char* new_password);
int user_password_required(const char* username);
int user_exists(const char* username);
const char* user_current_group(void);
int user_persistence_available(void);
int user_save_persistent(void);
int user_load_persistent(void);

#endif
