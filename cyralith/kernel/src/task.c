#include "task.h"
#include "string.h"

static task_t tasks[] = {
    {"console.service", TASK_RUNNING, 1U, "Bildschirm und Texteingabe", 0U, 0U},
    {"input.service", TASK_RUNNING, 1U, "Tastatur und Eingabe", 0U, 0U},
    {"memory.service", TASK_RUNNING, 1U, "Speicherverwaltung", 0U, 0U},
    {"users.service", TASK_RUNNING, 1U, "Benutzer und Rollen", 0U, 0U},
    {"editor.service", TASK_RUNNING, 2U, "Lumen Texteditor im RAM", 0U, 0U},
    {"ai.intent", TASK_RUNNING, 2U, "Natuerliche Sprache", 0U, 0U},
    {"automation.service", TASK_RUNNING, 2U, "Zeitgesteuerte Shell-Aufgaben", 0U, 0U},
    {"recovery.service", TASK_RUNNING, 2U, "Diagnose und Recovery-Hinweise", 0U, 0U},
    {"desktop.service", TASK_STOPPED, 3U, "Einfacher Desktop in Planung", 0U, 0U},
    {"network.service", TASK_STOPPED, 3U, "Netzwerkdienst und Treiberstatus", 0U, 0U},
};

static uint32_t g_scheduler_ticks = 0U;
static int g_current_index = -1;

void task_init(void) {
    size_t i;
    g_scheduler_ticks = 0U;
    g_current_index = -1;
    for (i = 0U; i < task_count(); ++i) {
        tasks[i].runtime_ticks = 0U;
        tasks[i].switches = 0U;
    }
}

size_t task_count(void) {
    return sizeof(tasks) / sizeof(tasks[0]);
}

const task_t* task_get(size_t index) {
    if (index >= task_count()) {
        return (const task_t*)0;
    }
    return &tasks[index];
}

static int task_name_matches(const char* query, const char* full_name) {
    if (kstrcmp(query, full_name) == 0) {
        return 1;
    }

    if (kstrcmp(query, "console") == 0 && kstrcmp(full_name, "console.service") == 0) return 1;
    if (kstrcmp(query, "input") == 0 && kstrcmp(full_name, "input.service") == 0) return 1;
    if (kstrcmp(query, "memory") == 0 && kstrcmp(full_name, "memory.service") == 0) return 1;
    if (kstrcmp(query, "users") == 0 && kstrcmp(full_name, "users.service") == 0) return 1;
    if (kstrcmp(query, "editor") == 0 && kstrcmp(full_name, "editor.service") == 0) return 1;
    if (kstrcmp(query, "desktop") == 0 && kstrcmp(full_name, "desktop.service") == 0) return 1;
    if (kstrcmp(query, "network") == 0 && kstrcmp(full_name, "network.service") == 0) return 1;
    if (kstrcmp(query, "ai") == 0 && kstrcmp(full_name, "ai.intent") == 0) return 1;
    if (kstrcmp(query, "automation") == 0 && kstrcmp(full_name, "automation.service") == 0) return 1;
    if (kstrcmp(query, "recovery") == 0 && kstrcmp(full_name, "recovery.service") == 0) return 1;

    return 0;
}

static task_t* task_find(const char* name) {
    size_t i;
    for (i = 0; i < task_count(); ++i) {
        if (task_name_matches(name, tasks[i].name)) {
            return &tasks[i];
        }
    }
    return (task_t*)0;
}

int task_start(const char* name) {
    task_t* task = task_find(name);
    if (task == (task_t*)0) {
        return -1;
    }
    task->state = TASK_RUNNING;
    return 0;
}

int task_stop(const char* name) {
    task_t* task = task_find(name);
    if (task == (task_t*)0) {
        return -1;
    }
    task->state = TASK_STOPPED;
    return 0;
}

const char* task_state_name(task_state_t state) {
    if (state == TASK_RUNNING) {
        return "running";
    }
    return "stopped";
}

void task_scheduler_tick(void) {
    size_t i;
    int next = -1;
    g_scheduler_ticks++;

    for (i = 0U; i < task_count(); ++i) {
        size_t probe = (size_t)((g_current_index + 1 + (int)i + (int)task_count()) % (int)task_count());
        if (tasks[probe].state == TASK_RUNNING) {
            next = (int)probe;
            break;
        }
    }

    if (next >= 0) {
        if (next != g_current_index) {
            tasks[next].switches++;
        }
        g_current_index = next;
        tasks[g_current_index].runtime_ticks++;
    }
}

const task_t* task_current(void) {
    if (g_current_index < 0) {
        return (const task_t*)0;
    }
    return &tasks[g_current_index];
}

uint32_t task_scheduler_ticks(void) {
    return g_scheduler_ticks;
}

const char* task_scheduler_name(void) {
    return "cooperative round-robin";
}
