#include "arcade.h"
#include "storage.h"
#include "string.h"

#define ARCADE_PERSIST_MAGIC 0x41435244U
#define ARCADE_PERSIST_VERSION 1U
#define ARCADE_PERSIST_SECTORS 2U
#define ARCADE_PERSIST_LBA 40U
#define ARCADE_MAX_GAMES 6U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t checksum;
} arcade_snapshot_header_t;

typedef struct {
    arcade_game_info_t games[ARCADE_MAX_GAMES];
} arcade_snapshot_payload_t;

static arcade_game_info_t g_arcade_games[ARCADE_MAX_GAMES] = {
    {{'s','n','a','k','e','\0'}, {'S','n','a','k','e','\0'}, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    {{'p','o','n','g','\0'}, {'P','o','n','g','\0'}, 0U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    {{'t','e','t','r','i','s','\0'}, {'T','e','t','r','i','s','\0'}, 0U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    {{'b','r','e','a','k','o','u','t','\0'}, {'B','r','e','a','k','o','u','t','\0'}, 0U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    {{'m','i','n','e','s','\0'}, {'M','i','n','e','s','\0'}, 0U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
    {{'r','a','c','e','r','\0'}, {'R','a','c','e','r','\0'}, 0U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U}
};

static unsigned char g_arcade_persist_buffer[ARCADE_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];

static void arcade_zero_bytes(unsigned char* dst, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = 0U;
    }
}

static void arcade_copy_bytes(unsigned char* dst, const unsigned char* src, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = src[i];
    }
}

static unsigned int arcade_checksum_bytes(const unsigned char* data, size_t count) {
    size_t i;
    unsigned int sum = 0U;
    for (i = 0U; i < count; ++i) {
        sum = (sum << 5U) - sum + (unsigned int)data[i];
    }
    return sum;
}

void arcade_init(void) {
    (void)arcade_load_persistent();
}

size_t arcade_game_count(void) {
    return ARCADE_MAX_GAMES;
}

const arcade_game_info_t* arcade_game_get(size_t index) {
    if (index >= ARCADE_MAX_GAMES) {
        return (const arcade_game_info_t*)0;
    }
    return &g_arcade_games[index];
}

const arcade_game_info_t* arcade_game_find(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return (const arcade_game_info_t*)0;
    }
    for (i = 0U; i < ARCADE_MAX_GAMES; ++i) {
        if (kstrcmp(g_arcade_games[i].name, name) == 0) {
            return &g_arcade_games[i];
        }
    }
    return (const arcade_game_info_t*)0;
}

static arcade_game_info_t* arcade_game_find_mut(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return (arcade_game_info_t*)0;
    }
    for (i = 0U; i < ARCADE_MAX_GAMES; ++i) {
        if (kstrcmp(g_arcade_games[i].name, name) == 0) {
            return &g_arcade_games[i];
        }
    }
    return (arcade_game_info_t*)0;
}

void arcade_session_start(const char* name) {
    arcade_game_info_t* game = arcade_game_find_mut(name);
    if (game == (arcade_game_info_t*)0) {
        return;
    }
    if (game->available == 0U) {
        game->available = 1U;
    }
}

void arcade_session_finish(const char* name, uint32_t score, uint32_t length, uint32_t apples, uint32_t level) {
    arcade_game_info_t* game = arcade_game_find_mut(name);
    if (game == (arcade_game_info_t*)0) {
        return;
    }
    game->plays++;
    game->last_score = score;
    game->last_length = length;
    game->last_apples = apples;
    game->last_level = level;
    if (score > game->best_score) {
        game->best_score = score;
    }
    if (length > game->best_length) {
        game->best_length = length;
    }
    if (apples > game->best_apples) {
        game->best_apples = apples;
    }
    if (level > game->best_level) {
        game->best_level = level;
    }
    (void)arcade_save_persistent();
}

int arcade_persistence_available(void) {
    storage_init();
    return storage_available();
}

int arcade_save_persistent(void) {
    arcade_snapshot_header_t* header;
    arcade_snapshot_payload_t* payload;
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (sizeof(arcade_snapshot_header_t) + sizeof(arcade_snapshot_payload_t) > sizeof(g_arcade_persist_buffer)) {
        return -1;
    }

    arcade_zero_bytes(g_arcade_persist_buffer, sizeof(g_arcade_persist_buffer));
    header = (arcade_snapshot_header_t*)g_arcade_persist_buffer;
    payload = (arcade_snapshot_payload_t*)(g_arcade_persist_buffer + sizeof(arcade_snapshot_header_t));
    header->magic = ARCADE_PERSIST_MAGIC;
    header->version = ARCADE_PERSIST_VERSION;
    header->count = ARCADE_MAX_GAMES;
    arcade_copy_bytes((unsigned char*)payload->games, (const unsigned char*)g_arcade_games, sizeof(g_arcade_games));
    header->checksum = arcade_checksum_bytes((const unsigned char*)payload, sizeof(arcade_snapshot_payload_t));

    for (sector = 0U; sector < ARCADE_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(ARCADE_PERSIST_LBA + sector, g_arcade_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int arcade_load_persistent(void) {
    arcade_snapshot_header_t* header = (arcade_snapshot_header_t*)g_arcade_persist_buffer;
    arcade_snapshot_payload_t* payload;
    unsigned int checksum;
    unsigned int sector;
    size_t i;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (sizeof(arcade_snapshot_header_t) + sizeof(arcade_snapshot_payload_t) > sizeof(g_arcade_persist_buffer)) {
        return -1;
    }

    for (sector = 0U; sector < ARCADE_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(ARCADE_PERSIST_LBA + sector, g_arcade_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }

    if (header->magic != ARCADE_PERSIST_MAGIC || header->version != ARCADE_PERSIST_VERSION || header->count != ARCADE_MAX_GAMES) {
        return -1;
    }

    payload = (arcade_snapshot_payload_t*)(g_arcade_persist_buffer + sizeof(arcade_snapshot_header_t));
    checksum = arcade_checksum_bytes((const unsigned char*)payload, sizeof(arcade_snapshot_payload_t));
    if (checksum != header->checksum) {
        return -1;
    }

    for (i = 0U; i < ARCADE_MAX_GAMES; ++i) {
        if (kstrcmp(g_arcade_games[i].name, payload->games[i].name) == 0) {
            g_arcade_games[i].plays = payload->games[i].plays;
            g_arcade_games[i].best_score = payload->games[i].best_score;
            g_arcade_games[i].last_score = payload->games[i].last_score;
            g_arcade_games[i].best_length = payload->games[i].best_length;
            g_arcade_games[i].last_length = payload->games[i].last_length;
            g_arcade_games[i].best_apples = payload->games[i].best_apples;
            g_arcade_games[i].last_apples = payload->games[i].last_apples;
            g_arcade_games[i].best_level = payload->games[i].best_level;
            g_arcade_games[i].last_level = payload->games[i].last_level;
        }
    }
    return 0;
}
