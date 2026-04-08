#include "user.h"
#include "string.h"
#include "storage.h"

#define USER_PERSIST_MAGIC 0x31525541U
#define USER_PERSIST_VERSION 1U
#define USER_PERSIST_SECTORS 4U
#define USER_PERSIST_LBA 24U

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int count;
    unsigned int current_index;
    unsigned int checksum;
} user_snapshot_header_t;

typedef struct {
    user_t users[1];
    char system_password[24];
    unsigned int reserved;
} user_snapshot_payload_t;

static user_t users[] = {
    {{'g','u','e','s','t','\0'}, {'G','u','e','s','t',' ','U','s','e','r','\0'}, {'g','u','e','s','t','\0'}, {'/','h','o','m','e','/','g','u','e','s','t','\0'}, {'g','u','e','s','t','\0'}, USER_ROLE_USER},
};

static char system_password[24] = {'a','u','r','o','r','a','\0'};
static size_t current_index = 0U;
static size_t previous_index = 0U;
static int system_mode = 0;
static unsigned char persist_buffer[USER_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];

static void copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (max == 0U) {
        return;
    }
    while (src[i] != '\0' && i + 1U < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void zero_bytes(unsigned char* dst, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = 0U;
    }
}

static void copy_bytes(unsigned char* dst, const unsigned char* src, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = src[i];
    }
}

static unsigned int checksum_bytes(const unsigned char* data, size_t count) {
    size_t i;
    unsigned int sum = 0U;
    for (i = 0U; i < count; ++i) {
        sum = (sum << 5U) - sum + (unsigned int)data[i];
    }
    return sum;
}

static int is_system_name(const char* username) {
    return kstrcmp(username, "system") == 0 || kstrcmp(username, "master") == 0 || kstrcmp(username, "root") == 0;
}

static int find_user_index(const char* username) {
    size_t i;
    for (i = 0; i < user_count(); ++i) {
        if (kstrcmp(users[i].username, username) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int user_persistence_available(void) {
    storage_init();
    return storage_available();
}

int user_save_persistent(void) {
    user_snapshot_header_t* header;
    user_snapshot_payload_t* payload;
    size_t header_size = sizeof(user_snapshot_header_t);
    size_t payload_size = sizeof(user_snapshot_payload_t);
    size_t offset = header_size;
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (offset + payload_size > sizeof(persist_buffer)) {
        return -1;
    }

    zero_bytes(persist_buffer, sizeof(persist_buffer));
    header = (user_snapshot_header_t*)persist_buffer;
    payload = (user_snapshot_payload_t*)(persist_buffer + offset);

    header->magic = USER_PERSIST_MAGIC;
    header->version = USER_PERSIST_VERSION;
    header->count = (unsigned int)user_count();
    header->current_index = (unsigned int)current_index;

    copy_bytes((unsigned char*)payload->users, (const unsigned char*)users, sizeof(users));
    copy_bytes((unsigned char*)payload->system_password, (const unsigned char*)system_password, sizeof(system_password));
    payload->reserved = 0U;
    header->checksum = checksum_bytes((const unsigned char*)payload, payload_size);

    for (sector = 0U; sector < USER_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(USER_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int user_load_persistent(void) {
    user_snapshot_header_t* header = (user_snapshot_header_t*)persist_buffer;
    user_snapshot_payload_t* payload;
    size_t payload_size = sizeof(user_snapshot_payload_t);
    size_t offset = sizeof(user_snapshot_header_t);
    unsigned int checksum;
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (offset + payload_size > sizeof(persist_buffer)) {
        return -1;
    }

    for (sector = 0U; sector < USER_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(USER_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }

    if (header->magic != USER_PERSIST_MAGIC || header->version != USER_PERSIST_VERSION || header->count != (unsigned int)user_count()) {
        return -1;
    }

    payload = (user_snapshot_payload_t*)(persist_buffer + offset);
    checksum = checksum_bytes((const unsigned char*)payload, payload_size);
    if (checksum != header->checksum) {
        return -1;
    }

    copy_bytes((unsigned char*)users, (const unsigned char*)payload->users, sizeof(users));
    copy_bytes((unsigned char*)system_password, (const unsigned char*)payload->system_password, sizeof(system_password));
    current_index = header->current_index < user_count() ? (size_t)header->current_index : 0U;
    previous_index = current_index;
    system_mode = 0;
    return 0;
}

void user_init(void) {
    current_index = 0U;
    previous_index = 0U;
    system_mode = 0;
    (void)user_load_persistent();
}

size_t user_count(void) {
    return sizeof(users) / sizeof(users[0]);
}

const user_t* user_get(size_t index) {
    if (index >= user_count()) {
        return (const user_t*)0;
    }
    return &users[index];
}

const user_t* user_current(void) {
    return user_get(current_index);
}

const user_t* user_previous(void) {
    return user_get(previous_index);
}

int user_exists(const char* username) {
    if (username == (const char*)0 || username[0] == '\0') {
        return 0;
    }
    if (is_system_name(username) != 0) {
        return 1;
    }
    return find_user_index(username) >= 0 ? 1 : 0;
}

const char* user_current_group(void) {
    const user_t* current = user_current();
    if (current == (const user_t*)0) {
        return "guest";
    }
    return current->group;
}

int user_password_required(const char* username) {
    int index;
    if (is_system_name(username) != 0) {
        return 1;
    }
    index = find_user_index(username);
    if (index < 0) {
        return 0;
    }
    return users[index].password[0] != '\0' ? 1 : 0;
}

int user_login(const char* username, const char* password) {
    int index;

    if (is_system_name(username) != 0) {
        return -3;
    }

    index = find_user_index(username);
    if (index < 0) {
        return -1;
    }

    if (users[index].password[0] != '\0') {
        if (password == (const char*)0 || password[0] == '\0') {
            return -2;
        }
        if (kstrcmp(users[index].password, password) != 0) {
            return -2;
        }
    }

    previous_index = current_index;
    current_index = (size_t)index;
    system_mode = 0;
    return 0;
}

int user_elevate(const char* password) {
    if (system_mode != 0) {
        return 1;
    }
    if (password == (const char*)0 || password[0] == '\0') {
        return -2;
    }
    if (kstrcmp(system_password, password) != 0) {
        return -2;
    }
    system_mode = 1;
    return 0;
}

void user_drop(void) {
    system_mode = 0;
}

int user_is_master(void) {
    return system_mode != 0 ? 1 : 0;
}

const char* user_role_name(user_role_t role) {
    if (role == USER_ROLE_SYSTEM) {
        return "system";
    }
    return "user";
}

int user_set_password(const char* username, const char* new_password) {
    int index;
    int rc;
    if (new_password == (const char*)0 || new_password[0] == '\0') {
        return -2;
    }

    if (is_system_name(username) != 0) {
        if (kstrlen(new_password) >= sizeof(system_password)) {
            return -2;
        }
        copy_limited(system_password, new_password, sizeof(system_password));
        rc = user_save_persistent();
        (void)rc;
        return 0;
    }

    if (kstrlen(new_password) >= sizeof(users[0].password)) {
        return -2;
    }

    index = find_user_index(username);
    if (index < 0) {
        return -1;
    }

    copy_limited(users[index].password, new_password, sizeof(users[index].password));
    rc = user_save_persistent();
    (void)rc;
    return 0;
}
