#ifndef CYRALITH_ARCADE_H
#define CYRALITH_ARCADE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[16];
    char title[24];
    uint32_t available;
    uint32_t planned;
    uint32_t plays;
    uint32_t best_score;
    uint32_t last_score;
    uint32_t best_length;
    uint32_t last_length;
    uint32_t best_apples;
    uint32_t last_apples;
    uint32_t best_level;
    uint32_t last_level;
} arcade_game_info_t;

void arcade_init(void);
size_t arcade_game_count(void);
const arcade_game_info_t* arcade_game_get(size_t index);
const arcade_game_info_t* arcade_game_find(const char* name);
void arcade_session_start(const char* name);
void arcade_session_finish(const char* name, uint32_t score, uint32_t length, uint32_t apples, uint32_t level);
int arcade_persistence_available(void);
int arcade_save_persistent(void);
int arcade_load_persistent(void);

#endif
