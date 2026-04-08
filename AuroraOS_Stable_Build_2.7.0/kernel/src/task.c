#include "task.h"
#include "string.h"

static task_t tasks[] = {
    {"console.service", TASK_RUNNING, 1U, "Bildschirm und Texteingabe"},
    {"input.service", TASK_RUNNING, 1U, "Tastatur und Eingabe"},
    {"memory.service", TASK_RUNNING, 1U, "Speicherverwaltung"},
    {"users.service", TASK_RUNNING, 1U, "Benutzer und Rollen"},
    {"editor.service", TASK_RUNNING, 2U, "Lumen Texteditor im RAM"},
    {"ai.intent", TASK_RUNNING, 2U, "Natuerliche Sprache"},
    {"desktop.service", TASK_STOPPED, 3U, "Einfacher Desktop in Planung"},
    {"network.service", TASK_STOPPED, 3U, "Netzwerk in Planung"},
};

void task_init(void) {
    /* Statische Startkonfiguration ist bereits gesetzt. */
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

    return 0;
}

static task_t* task_find(const char* name) {
    for (size_t i = 0; i < task_count(); ++i) {
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
