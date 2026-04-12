#include "recovery.h"
#include "storage.h"
#include "string.h"

#define RECOVERY_MAGIC 0x31564552U
#define RECOVERY_VERSION 2U
#define RECOVERY_PERSIST_SECTORS 2U
#define RECOVERY_PERSIST_LBA 42U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint32_t unexpected_shutdowns;
    uint32_t startup_failures;
    uint32_t auto_recovery_count;
    uint32_t safe_mode_enabled;
    uint32_t session_open;
    uint32_t startup_ready;
    char last_boot_stage[40];
    char last_issue[96];
    uint32_t checksum;
} recovery_snapshot_t;

static recovery_snapshot_t g_state;
static unsigned char g_persist_buffer[RECOVERY_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];

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

static unsigned int snapshot_checksum(const recovery_snapshot_t* snapshot) {
    recovery_snapshot_t temp = *snapshot;
    temp.checksum = 0U;
    return checksum_bytes((const unsigned char*)&temp, sizeof(temp));
}

static void recovery_defaults(void) {
    zero_bytes((unsigned char*)&g_state, sizeof(g_state));
    g_state.magic = RECOVERY_MAGIC;
    g_state.version = RECOVERY_VERSION;
    copy_limited(g_state.last_boot_stage, "cold boot", sizeof(g_state.last_boot_stage));
    g_state.last_issue[0] = '\0';
}

void recovery_init(void) {
    recovery_defaults();
    (void)recovery_load_persistent();
}

void recovery_boot_begin(void) {
    if (g_state.session_open != 0U) {
        if (g_state.startup_ready != 0U) {
            g_state.unexpected_shutdowns++;
            copy_limited(g_state.last_issue, "previous session ended without clean shutdown", sizeof(g_state.last_issue));
        } else {
            g_state.startup_failures++;
            copy_limited(g_state.last_issue, "previous boot stopped during startup", sizeof(g_state.last_issue));
            if (g_state.startup_failures >= 2U && g_state.safe_mode_enabled == 0U) {
                g_state.safe_mode_enabled = 1U;
                g_state.auto_recovery_count++;
                copy_limited(g_state.last_issue, "safe mode enabled after repeated startup failures", sizeof(g_state.last_issue));
            }
        }
    }
    g_state.boot_count++;
    g_state.session_open = 1U;
    g_state.startup_ready = 0U;
    copy_limited(g_state.last_boot_stage, "boot", sizeof(g_state.last_boot_stage));
    (void)recovery_save_persistent();
}

void recovery_boot_stage(const char* stage) {
    copy_limited(g_state.last_boot_stage, stage != (const char*)0 ? stage : "boot", sizeof(g_state.last_boot_stage));
    (void)recovery_save_persistent();
}

void recovery_boot_ready(void) {
    g_state.startup_ready = 1U;
    copy_limited(g_state.last_boot_stage, "shell ready", sizeof(g_state.last_boot_stage));
    (void)recovery_save_persistent();
}

void recovery_note_issue(const char* issue) {
    copy_limited(g_state.last_issue, issue, sizeof(g_state.last_issue));
    (void)recovery_save_persistent();
}

const char* recovery_last_issue(void) {
    return g_state.last_issue;
}

void recovery_clear_issue(void) {
    g_state.last_issue[0] = '\0';
    (void)recovery_save_persistent();
}

int recovery_safe_mode_enabled(void) {
    return g_state.safe_mode_enabled != 0U ? 1 : 0;
}

void recovery_set_safe_mode(int enabled) {
    g_state.safe_mode_enabled = enabled != 0 ? 1U : 0U;
    (void)recovery_save_persistent();
}

uint32_t recovery_boot_count(void) {
    return g_state.boot_count;
}

uint32_t recovery_unexpected_shutdown_count(void) {
    return g_state.unexpected_shutdowns;
}

uint32_t recovery_startup_failure_count(void) {
    return g_state.startup_failures;
}

uint32_t recovery_auto_recovery_count(void) {
    return g_state.auto_recovery_count;
}

const char* recovery_last_boot_stage(void) {
    return g_state.last_boot_stage;
}

int recovery_session_open(void) {
    return g_state.session_open != 0U ? 1 : 0;
}

int recovery_mark_clean_shutdown(const char* reason) {
    g_state.session_open = 0U;
    g_state.startup_ready = 1U;
    if (reason != (const char*)0 && reason[0] != '\0') {
        copy_limited(g_state.last_boot_stage, reason, sizeof(g_state.last_boot_stage));
    } else {
        copy_limited(g_state.last_boot_stage, "clean shutdown", sizeof(g_state.last_boot_stage));
    }
    return recovery_save_persistent();
}

int recovery_save_persistent(void) {
    size_t sector;
    recovery_snapshot_t snapshot = g_state;
    if (storage_available() == 0) {
        storage_init();
    }
    if (storage_available() == 0) {
        return -1;
    }
    zero_bytes(g_persist_buffer, sizeof(g_persist_buffer));
    snapshot.magic = RECOVERY_MAGIC;
    snapshot.version = RECOVERY_VERSION;
    snapshot.checksum = snapshot_checksum(&snapshot);
    copy_bytes(g_persist_buffer, (const unsigned char*)&snapshot, sizeof(snapshot));
    for (sector = 0U; sector < RECOVERY_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(RECOVERY_PERSIST_LBA + sector, g_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    g_state = snapshot;
    return 0;
}

int recovery_load_persistent(void) {
    recovery_snapshot_t snapshot;
    size_t sector;
    if (storage_available() == 0) {
        storage_init();
    }
    if (storage_available() == 0) {
        return -1;
    }
    for (sector = 0U; sector < RECOVERY_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(RECOVERY_PERSIST_LBA + sector, g_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    copy_bytes((unsigned char*)&snapshot, g_persist_buffer, sizeof(snapshot));
    if (snapshot.magic != RECOVERY_MAGIC || snapshot.version != RECOVERY_VERSION) {
        return -1;
    }
    if (snapshot.checksum != snapshot_checksum(&snapshot)) {
        return -1;
    }
    g_state = snapshot;
    return 0;
}
