#include "syslog.h"
#include "string.h"
#include "timer.h"
#include "storage.h"

#define SYSLOG_MAX 24U
#define SYSLOG_PERSIST_MAGIC 0x31474F4CU
#define SYSLOG_PERSIST_VERSION 1U
#define SYSLOG_PERSIST_SECTORS 6U
#define SYSLOG_PERSIST_LBA 44U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t head;
    uint32_t count;
    uint32_t next_sequence;
    uint32_t checksum;
} syslog_snapshot_header_t;

static syslog_entry_t g_entries[SYSLOG_MAX];
static size_t g_head = 0U;
static size_t g_count = 0U;
static uint32_t g_next_sequence = 1U;
static unsigned char g_persist_buffer[SYSLOG_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];

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

static unsigned int checksum_bytes(const unsigned char* data, size_t count) {
    size_t i;
    unsigned int sum = 0U;
    for (i = 0U; i < count; ++i) {
        sum = (sum << 5U) - sum + (unsigned int)data[i];
    }
    return sum;
}

void syslog_init(void) {
    g_head = 0U;
    g_count = 0U;
    g_next_sequence = 1U;
    (void)syslog_load_persistent();
}

void syslog_write(syslog_level_t level, const char* source, const char* message) {
    syslog_entry_t* entry = &g_entries[g_head];
    entry->sequence = g_next_sequence++;
    entry->tick = timer_ticks();
    entry->level = (uint8_t)level;
    copy_limited(entry->source, source != (const char*)0 ? source : "system", sizeof(entry->source));
    copy_limited(entry->message, message != (const char*)0 ? message : "", sizeof(entry->message));

    g_head = (g_head + 1U) % SYSLOG_MAX;
    if (g_count < SYSLOG_MAX) {
        g_count++;
    }
}

void syslog_clear(void) {
    g_head = 0U;
    g_count = 0U;
    g_next_sequence = 1U;
    zero_bytes((unsigned char*)g_entries, sizeof(g_entries));
}

size_t syslog_count(void) {
    return g_count;
}

const syslog_entry_t* syslog_get_recent(size_t recent_index) {
    size_t real_index;
    if (recent_index >= g_count) {
        return (const syslog_entry_t*)0;
    }
    real_index = (g_head + SYSLOG_MAX - 1U - recent_index) % SYSLOG_MAX;
    return &g_entries[real_index];
}

const char* syslog_level_name(syslog_level_t level) {
    switch (level) {
        case SYSLOG_DEBUG: return "debug";
        case SYSLOG_INFO: return "info";
        case SYSLOG_WARN: return "warn";
        case SYSLOG_ERROR: return "error";
        default: return "unknown";
    }
}

int syslog_save_persistent(void) {
    syslog_snapshot_header_t header;
    size_t sector;
    size_t offset;
    if (storage_available() == 0) {
        storage_init();
    }
    if (storage_available() == 0) {
        return -1;
    }

    zero_bytes(g_persist_buffer, sizeof(g_persist_buffer));
    header.magic = SYSLOG_PERSIST_MAGIC;
    header.version = SYSLOG_PERSIST_VERSION;
    header.head = (uint32_t)g_head;
    header.count = (uint32_t)g_count;
    header.next_sequence = g_next_sequence;
    header.checksum = 0U;

    copy_bytes(g_persist_buffer, (const unsigned char*)&header, sizeof(header));
    offset = sizeof(header);
    copy_bytes(g_persist_buffer + offset, (const unsigned char*)g_entries, sizeof(g_entries));
    header.checksum = checksum_bytes(g_persist_buffer + sizeof(header), sizeof(g_persist_buffer) - sizeof(header));
    copy_bytes(g_persist_buffer, (const unsigned char*)&header, sizeof(header));

    for (sector = 0U; sector < SYSLOG_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(SYSLOG_PERSIST_LBA + sector, g_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int syslog_load_persistent(void) {
    syslog_snapshot_header_t header;
    size_t sector;
    size_t offset;
    if (storage_available() == 0) {
        storage_init();
    }
    if (storage_available() == 0) {
        return -1;
    }

    for (sector = 0U; sector < SYSLOG_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(SYSLOG_PERSIST_LBA + sector, g_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }

    copy_bytes((unsigned char*)&header, g_persist_buffer, sizeof(header));
    if (header.magic != SYSLOG_PERSIST_MAGIC || header.version != SYSLOG_PERSIST_VERSION) {
        return -1;
    }
    if (header.checksum != checksum_bytes(g_persist_buffer + sizeof(header), sizeof(g_persist_buffer) - sizeof(header))) {
        return -1;
    }
    if (header.head >= SYSLOG_MAX || header.count > SYSLOG_MAX || header.next_sequence == 0U) {
        return -1;
    }

    offset = sizeof(header);
    copy_bytes((unsigned char*)g_entries, g_persist_buffer + offset, sizeof(g_entries));
    g_head = (size_t)header.head;
    g_count = (size_t)header.count;
    g_next_sequence = header.next_sequence;
    return 0;
}
