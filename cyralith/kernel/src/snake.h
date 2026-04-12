#ifndef SNAKE_APP_H
#define SNAKE_APP_H

#include <stdint.h>

typedef struct {
    uint32_t score;
    uint32_t best_score;
    uint32_t last_score;
    uint32_t apples;
    uint32_t best_apples;
    uint32_t last_apples;
    uint32_t length;
    uint32_t best_length;
    uint32_t last_length;
    uint32_t level;
    uint32_t best_level;
    uint32_t last_level;
    uint32_t plays;
} snake_stats_t;

int snake_is_active(void);
void snake_open(void);
void snake_handle_key(int key);
void snake_poll(void);
void snake_get_stats(snake_stats_t* out);

#endif
