#ifndef AI_CORE_H
#define AI_CORE_H

typedef void (*ai_command_runner_t)(const char* command);

void ai_route(const char* input);
void ai_bind_runner(ai_command_runner_t runner);

#endif
