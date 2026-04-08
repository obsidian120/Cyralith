#include "app.h"
#include "string.h"
#include "storage.h"

#define APP_PERSIST_MAGIC 0x31505041U
#define APP_PERSIST_VERSION 1U
#define APP_PERSIST_SECTORS 2U
#define APP_PERSIST_LBA 32U

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int count;
    unsigned int checksum;
} app_snapshot_header_t;

typedef struct {
    unsigned int installed[8];
} app_snapshot_payload_t;

static app_t g_apps[] = {
    {{'l','u','m','e','n','\0'}, {'L','u','m','e','n','\0'}, {'N','a','n','o','-','a','r','t','i','g','e','r',' ','T','e','x','t','e','d','i','t','o','r','\0'}, 1U, 1U, 1U},
    {{'f','i','l','e','s','\0'}, {'F','i','l','e','s','\0'}, {'E','i','n','f','a','c','h','e','r',' ','D','a','t','e','i','-','B','e','r','e','i','c','h','\0'}, 1U, 1U, 1U},
    {{'s','e','t','t','i','n','g','s','\0'}, {'S','e','t','t','i','n','g','s','\0'}, {'S','p','r','a','c','h','e',',',' ','L','a','y','o','u','t',' ','u','n','d',' ','G','r','u','n','d','o','p','t','i','o','n','e','n','\0'}, 1U, 1U, 1U},
    {{'n','e','t','w','o','r','k','\0'}, {'N','e','t','w','o','r','k','\0'}, {'N','e','t','z','w','e','r','k',',',' ','H','o','s','t','n','a','m','e',' ','u','n','d',' ','N','I','C','-','U','e','b','e','r','s','i','c','h','t','\0'}, 1U, 1U, 1U},
    {{'m','o','n','i','t','o','r','\0'}, {'M','o','n','i','t','o','r','\0'}, {'S','y','s','t','e','m','-','U','e','b','e','r','b','l','i','c','k',' ','u','n','d',' ','S','t','a','t','u','s','\0'}, 1U, 1U, 1U},
    {{'b','r','o','w','s','e','r','\0'}, {'B','r','o','w','s','e','r','\0'}, {'G','e','p','l','a','n','t','e','r',' ','W','e','b','-','B','e','r','e','i','c','h',' ','a','l','s',' ','P','l','a','t','z','h','a','l','t','e','r','\0'}, 0U, 0U, 1U},
    {{'d','e','s','k','t','o','p','\0'}, {'D','e','s','k','t','o','p','\0'}, {'E','i','n','f','a','c','h','e','r',' ','D','e','s','k','t','o','p',' ','f','u','e','r',' ','s','p','a','e','t','e','r','\0'}, 0U, 0U, 1U},
    {{'p','a','i','n','t','\0'}, {'P','a','i','n','t','\0'}, {'K','l','e','i','n','e','r',' ','Z','e','i','c','h','e','n','-','P','l','a','t','z','h','a','l','t','e','r','\0'}, 0U, 0U, 1U},
};

static unsigned char persist_buffer[APP_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];

static void zero_bytes(unsigned char* dst, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = 0U;
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

void app_init(void) {
    (void)app_load_persistent();
}

size_t app_count(void) {
    return sizeof(g_apps) / sizeof(g_apps[0]);
}

const app_t* app_get(size_t index) {
    if (index >= app_count()) {
        return (const app_t*)0;
    }
    return &g_apps[index];
}

const app_t* app_find(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return (const app_t*)0;
    }
    for (i = 0U; i < app_count(); ++i) {
        if (kstrcmp(g_apps[i].name, name) == 0) {
            return &g_apps[i];
        }
    }
    return (const app_t*)0;
}

static app_t* app_find_mut(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return (app_t*)0;
    }
    for (i = 0U; i < app_count(); ++i) {
        if (kstrcmp(g_apps[i].name, name) == 0) {
            return &g_apps[i];
        }
    }
    return (app_t*)0;
}

int app_is_installed(const char* name) {
    const app_t* app = app_find(name);
    if (app == (const app_t*)0) {
        return 0;
    }
    return app->installed != 0U ? 1 : 0;
}

int app_install(const char* name) {
    app_t* app = app_find_mut(name);
    if (app == (app_t*)0) {
        return -1;
    }
    if (app->installed != 0U) {
        return 1;
    }
    app->installed = 1U;
    (void)app_save_persistent();
    return 0;
}

int app_remove(const char* name) {
    app_t* app = app_find_mut(name);
    if (app == (app_t*)0) {
        return -1;
    }
    if (app->builtin != 0U) {
        return -2;
    }
    if (app->installed == 0U) {
        return 1;
    }
    app->installed = 0U;
    (void)app_save_persistent();
    return 0;
}

int app_persistence_available(void) {
    storage_init();
    return storage_available();
}

int app_save_persistent(void) {
    app_snapshot_header_t* header;
    app_snapshot_payload_t* payload;
    size_t i;
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (sizeof(app_snapshot_header_t) + sizeof(app_snapshot_payload_t) > sizeof(persist_buffer)) {
        return -1;
    }

    zero_bytes(persist_buffer, sizeof(persist_buffer));
    header = (app_snapshot_header_t*)persist_buffer;
    payload = (app_snapshot_payload_t*)(persist_buffer + sizeof(app_snapshot_header_t));

    header->magic = APP_PERSIST_MAGIC;
    header->version = APP_PERSIST_VERSION;
    header->count = (unsigned int)app_count();
    for (i = 0U; i < app_count() && i < 8U; ++i) {
        payload->installed[i] = g_apps[i].installed;
    }
    header->checksum = checksum_bytes((const unsigned char*)payload, sizeof(app_snapshot_payload_t));

    for (sector = 0U; sector < APP_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(APP_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int app_load_persistent(void) {
    app_snapshot_header_t* header = (app_snapshot_header_t*)persist_buffer;
    app_snapshot_payload_t* payload;
    unsigned int checksum;
    unsigned int sector;
    size_t i;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (sizeof(app_snapshot_header_t) + sizeof(app_snapshot_payload_t) > sizeof(persist_buffer)) {
        return -1;
    }

    for (sector = 0U; sector < APP_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(APP_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }

    if (header->magic != APP_PERSIST_MAGIC || header->version != APP_PERSIST_VERSION || header->count != (unsigned int)app_count()) {
        return -1;
    }

    payload = (app_snapshot_payload_t*)(persist_buffer + sizeof(app_snapshot_header_t));
    checksum = checksum_bytes((const unsigned char*)payload, sizeof(app_snapshot_payload_t));
    if (checksum != header->checksum) {
        return -1;
    }

    for (i = 0U; i < app_count() && i < 8U; ++i) {
        if (g_apps[i].builtin == 0U) {
            g_apps[i].installed = payload->installed[i] != 0U ? 1U : 0U;
        }
    }
    return 0;
}
