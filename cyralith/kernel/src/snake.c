#include "snake.h"
#include "arcade.h"
#include "console.h"
#include "keyboard.h"
#include <stdint.h>

#define SNAKE_BOARD_W 46
#define SNAKE_BOARD_H 20
#define SNAKE_MAX_LEN 128
#define SNAKE_BASE_POLL_DIV 150000U
#define SNAKE_MIN_POLL_DIV 70000U
#define SNAKE_POLL_STEP 7000U

typedef struct {
    int active;
    int started;
    int game_over;
    int session_recorded;
    int dir_x;
    int dir_y;
    int next_x;
    int next_y;
    int length;
    int score;
    int apples;
    int level;
    int food_x;
    int food_y;
    int body_x[SNAKE_MAX_LEN];
    int body_y[SNAKE_MAX_LEN];
    uint32_t rng;
    unsigned int poll_divider;
    unsigned int poll_goal;
} snake_state_t;

static snake_state_t g_snake;

static uint32_t snake_rand(void) {
    g_snake.rng = (g_snake.rng * 1664525U) + 1013904223U;
    return g_snake.rng;
}

static int snake_on_body(int x, int y, int include_head) {
    int i;
    int start = include_head != 0 ? 0 : 1;
    for (i = start; i < g_snake.length; ++i) {
        if (g_snake.body_x[i] == x && g_snake.body_y[i] == y) {
            return 1;
        }
    }
    return 0;
}

static unsigned int snake_compute_poll_goal(void) {
    unsigned int reduction = (unsigned int)g_snake.apples * SNAKE_POLL_STEP;
    if (reduction + SNAKE_MIN_POLL_DIV >= SNAKE_BASE_POLL_DIV) {
        return SNAKE_MIN_POLL_DIV;
    }
    return SNAKE_BASE_POLL_DIV - reduction;
}

static void snake_spawn_food(void) {
    int tries;
    for (tries = 0; tries < 256; ++tries) {
        int x = (int)(snake_rand() % (uint32_t)SNAKE_BOARD_W);
        int y = (int)(snake_rand() % (uint32_t)SNAKE_BOARD_H);
        if (snake_on_body(x, y, 1) == 0) {
            g_snake.food_x = x;
            g_snake.food_y = y;
            return;
        }
    }
    g_snake.food_x = 0;
    g_snake.food_y = 0;
}

static void snake_record_session_if_needed(void) {
    if (g_snake.session_recorded != 0) {
        return;
    }
    arcade_session_finish("snake",
                          (uint32_t)g_snake.score,
                          (uint32_t)g_snake.length,
                          (uint32_t)g_snake.apples,
                          (uint32_t)g_snake.level);
    g_snake.session_recorded = 1;
}

static void snake_render(void) {
    int x;
    int y;
    char line[80];
    const arcade_game_info_t* info = arcade_game_find("snake");

    console_clear();
    console_writeln("+------------------------------------------------------------------------------+");
    console_writeln("| Snake for Cyralith                                                           |");
    console_writeln("| Controls: arrows or WASD | q quit | r restart | Enter restart after crash    |");
    console_writeln("+------------------------------------------------------------------------------+");
    console_write("Score: ");
    console_write_dec((uint32_t)g_snake.score);
    console_write("   Best: ");
    console_write_dec(info != (const arcade_game_info_t*)0 ? info->best_score : 0U);
    console_write("   Last: ");
    console_write_dec(info != (const arcade_game_info_t*)0 ? info->last_score : 0U);
    console_putc('\n');
    console_write("Length: ");
    console_write_dec((uint32_t)g_snake.length);
    console_write("   Apples: ");
    console_write_dec((uint32_t)g_snake.apples);
    console_write("   Level: ");
    console_write_dec((uint32_t)g_snake.level);
    console_write("   Plays: ");
    console_write_dec(info != (const arcade_game_info_t*)0 ? info->plays : 0U);
    console_putc('\n');
    console_write("Field: ");
    console_write_dec((uint32_t)SNAKE_BOARD_W);
    console_write("x");
    console_write_dec((uint32_t)SNAKE_BOARD_H);
    console_write("   Pace: ");
    console_write_dec((uint32_t)g_snake.poll_goal);
    console_writeln(" loop ticks");

    line[0] = '+';
    for (x = 0; x < SNAKE_BOARD_W; ++x) {
        line[1 + x] = '-';
    }
    line[1 + SNAKE_BOARD_W] = '+';
    line[2 + SNAKE_BOARD_W] = '\0';
    console_writeln(line);

    for (y = 0; y < SNAKE_BOARD_H; ++y) {
        line[0] = '|';
        for (x = 0; x < SNAKE_BOARD_W; ++x) {
            char ch = ' ';
            if (x == g_snake.food_x && y == g_snake.food_y) {
                ch = '*';
            }
            if (snake_on_body(x, y, 1) != 0) {
                ch = 'o';
            }
            if (g_snake.body_x[0] == x && g_snake.body_y[0] == y) {
                ch = 'O';
            }
            line[1 + x] = ch;
        }
        line[1 + SNAKE_BOARD_W] = '|';
        line[2 + SNAKE_BOARD_W] = '\0';
        console_writeln(line);
    }

    line[0] = '+';
    for (x = 0; x < SNAKE_BOARD_W; ++x) {
        line[1 + x] = '-';
    }
    line[1 + SNAKE_BOARD_W] = '+';
    line[2 + SNAKE_BOARD_W] = '\0';
    console_writeln(line);

    if (g_snake.game_over != 0) {
        console_writeln("Crash. That wall came closer than expected. Press r or Enter to restart.");
    } else if (g_snake.started == 0) {
        console_writeln("Ready. Press an arrow key or WASD to start. Best score tracking is live now.");
    } else {
        console_writeln("Arcade mode active: apples speed you up. Survive longer, push the high score.");
    }
}

static void snake_reset(void) {
    int i;
    int center_x = SNAKE_BOARD_W / 2;
    int center_y = SNAKE_BOARD_H / 2;

    g_snake.active = 1;
    g_snake.started = 0;
    g_snake.game_over = 0;
    g_snake.session_recorded = 0;
    g_snake.dir_x = 1;
    g_snake.dir_y = 0;
    g_snake.next_x = 1;
    g_snake.next_y = 0;
    g_snake.length = 4;
    g_snake.score = 0;
    g_snake.apples = 0;
    g_snake.level = 1;
    g_snake.poll_divider = 0U;
    g_snake.rng ^= 0x9E3779B9U;
    g_snake.poll_goal = SNAKE_BASE_POLL_DIV;

    g_snake.body_x[0] = center_x;
    g_snake.body_y[0] = center_y;
    g_snake.body_x[1] = center_x - 1;
    g_snake.body_y[1] = center_y;
    g_snake.body_x[2] = center_x - 2;
    g_snake.body_y[2] = center_y;
    g_snake.body_x[3] = center_x - 3;
    g_snake.body_y[3] = center_y;

    for (i = 4; i < SNAKE_MAX_LEN; ++i) {
        g_snake.body_x[i] = 0;
        g_snake.body_y[i] = 0;
    }

    arcade_session_start("snake");
    snake_spawn_food();
    snake_render();
}

static void snake_step(void) {
    int new_x;
    int new_y;
    int i;
    int ate = 0;

    if (g_snake.game_over != 0 || g_snake.started == 0) {
        return;
    }

    g_snake.dir_x = g_snake.next_x;
    g_snake.dir_y = g_snake.next_y;
    new_x = g_snake.body_x[0] + g_snake.dir_x;
    new_y = g_snake.body_y[0] + g_snake.dir_y;

    if (new_x < 0 || new_x >= SNAKE_BOARD_W || new_y < 0 || new_y >= SNAKE_BOARD_H) {
        g_snake.game_over = 1;
        snake_record_session_if_needed();
        snake_render();
        return;
    }

    if (new_x == g_snake.food_x && new_y == g_snake.food_y) {
        ate = 1;
    }

    for (i = 0; i < (ate != 0 ? g_snake.length : g_snake.length - 1); ++i) {
        if (g_snake.body_x[i] == new_x && g_snake.body_y[i] == new_y) {
            g_snake.game_over = 1;
            snake_record_session_if_needed();
            snake_render();
            return;
        }
    }

    if (ate != 0 && g_snake.length < SNAKE_MAX_LEN) {
        g_snake.length++;
        g_snake.score += 10;
        g_snake.apples++;
        g_snake.level = 1 + (g_snake.apples / 3);
        g_snake.poll_goal = snake_compute_poll_goal();
    }

    for (i = g_snake.length - 1; i > 0; --i) {
        g_snake.body_x[i] = g_snake.body_x[i - 1];
        g_snake.body_y[i] = g_snake.body_y[i - 1];
    }
    g_snake.body_x[0] = new_x;
    g_snake.body_y[0] = new_y;

    if (ate != 0) {
        snake_spawn_food();
    }

    snake_render();
}

int snake_is_active(void) {
    return g_snake.active;
}

void snake_open(void) {
    if (g_snake.rng == 0U) {
        g_snake.rng = 0xC0FFEE11U;
    }
    snake_reset();
}

void snake_handle_key(int key) {
    if (g_snake.active == 0) {
        return;
    }
    if (key == KEY_CTRL_C || key == 'q' || key == 'Q') {
        snake_record_session_if_needed();
        g_snake.active = 0;
        console_clear();
        return;
    }
    if (key == 'r' || key == 'R' || (g_snake.game_over != 0 && key == KEY_ENTER)) {
        snake_reset();
        return;
    }
    if (key == KEY_UP || key == 'w' || key == 'W') {
        if (g_snake.dir_y != 1) {
            g_snake.next_x = 0;
            g_snake.next_y = -1;
            g_snake.started = 1;
            g_snake.poll_divider = 0U;
            snake_render();
        }
        return;
    }
    if (key == KEY_DOWN || key == 's' || key == 'S') {
        if (g_snake.dir_y != -1) {
            g_snake.next_x = 0;
            g_snake.next_y = 1;
            g_snake.started = 1;
            g_snake.poll_divider = 0U;
            snake_render();
        }
        return;
    }
    if (key == KEY_LEFT || key == 'a' || key == 'A') {
        if (g_snake.dir_x != 1) {
            g_snake.next_x = -1;
            g_snake.next_y = 0;
            g_snake.started = 1;
            g_snake.poll_divider = 0U;
            snake_render();
        }
        return;
    }
    if (key == KEY_RIGHT || key == 'd' || key == 'D') {
        if (g_snake.dir_x != -1) {
            g_snake.next_x = 1;
            g_snake.next_y = 0;
            g_snake.started = 1;
            g_snake.poll_divider = 0U;
            snake_render();
        }
        return;
    }
}

void snake_poll(void) {
    if (g_snake.active == 0 || g_snake.started == 0) {
        return;
    }
    g_snake.poll_divider++;
    if (g_snake.poll_divider < g_snake.poll_goal) {
        return;
    }
    g_snake.poll_divider = 0U;
    snake_step();
}

void snake_get_stats(snake_stats_t* out) {
    const arcade_game_info_t* info;
    if (out == (snake_stats_t*)0) {
        return;
    }
    info = arcade_game_find("snake");
    out->score = (uint32_t)g_snake.score;
    out->apples = (uint32_t)g_snake.apples;
    out->length = (uint32_t)g_snake.length;
    out->level = (uint32_t)g_snake.level;
    if (info != (const arcade_game_info_t*)0) {
        out->best_score = info->best_score;
        out->last_score = info->last_score;
        out->best_apples = info->best_apples;
        out->last_apples = info->last_apples;
        out->best_length = info->best_length;
        out->last_length = info->last_length;
        out->best_level = info->best_level;
        out->last_level = info->last_level;
        out->plays = info->plays;
    } else {
        out->best_score = 0U;
        out->last_score = 0U;
        out->best_apples = 0U;
        out->last_apples = 0U;
        out->best_length = 0U;
        out->last_length = 0U;
        out->best_level = 0U;
        out->last_level = 0U;
        out->plays = 0U;
    }
}
