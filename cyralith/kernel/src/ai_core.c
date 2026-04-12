#include "ai_core.h"
#include "cyralithfs.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#include "task.h"
#include "timer.h"
#include "keyboard.h"
#include "user.h"
#include "network.h"
#include "app.h"

static ai_command_runner_t g_runner = (ai_command_runner_t)0;

void ai_bind_runner(ai_command_runner_t runner) {
    g_runner = runner;
}

static int count_hits(const char* input, const char* const* words, size_t count) {
    size_t i;
    int score = 0;
    for (i = 0U; i < count; ++i) {
        if (kcontains_ci(input, words[i]) != 0) {
            score++;
        }
    }
    return score;
}

static void print_task_overview(void) {
    size_t i;
    console_writeln("[ai] Dienste:");
    for (i = 0U; i < task_count(); ++i) {
        const task_t* task = task_get(i);
        if (task == (const task_t*)0) {
            continue;
        }
        console_write("  - ");
        console_write(task->name);
        console_write(" [");
        console_write(task_state_name(task->state));
        console_writeln("]");
    }
}

static void print_app_overview(void) {
    size_t i;
    console_writeln("[ai] Apps:");
    for (i = 0U; i < app_count(); ++i) {
        const app_t* app = app_get(i);
        if (app == (const app_t*)0) {
            continue;
        }
        console_write("  - ");
        console_write(app->name);
        console_write(" [");
        console_write(app->installed != 0U ? "installed" : "optional");
        console_writeln("]");
    }
}

static int try_safe_nl_command(const char* input) {
    if (g_runner == (ai_command_runner_t)0) {
        return 0;
    }
    if ((kcontains_ci(input, "show") != 0 || kcontains_ci(input, "zeige") != 0 || kcontains_ci(input, "what is") != 0) &&
        (kcontains_ci(input, "status") != 0 || kcontains_ci(input, "zustand") != 0)) {
        console_writeln("[ai] Verstanden: Ich fuehre 'status' aus.");
        g_runner("status");
        return 1;
    }
    if ((kcontains_ci(input, "open") != 0 || kcontains_ci(input, "oeffne") != 0) && kcontains_ci(input, "settings") != 0) {
        console_writeln("[ai] Verstanden: Ich oeffne 'settings'.");
        g_runner("settings");
        return 1;
    }
    if ((kcontains_ci(input, "show") != 0 || kcontains_ci(input, "zeige") != 0) && kcontains_ci(input, "network") != 0) {
        console_writeln("[ai] Verstanden: Ich fuehre 'network' aus.");
        g_runner("network");
        return 1;
    }
    if ((kcontains_ci(input, "show") != 0 || kcontains_ci(input, "zeige") != 0) &&
        (kcontains_ci(input, "tasks") != 0 || kcontains_ci(input, "dienste") != 0 || kcontains_ci(input, "services") != 0)) {
        console_writeln("[ai] Verstanden: Ich fuehre 'tasks' aus.");
        g_runner("tasks");
        return 1;
    }
    if ((kcontains_ci(input, "who am i") != 0) || (kcontains_ci(input, "wer bin ich") != 0)) {
        console_writeln("[ai] Verstanden: Ich fuehre 'whoami' aus.");
        g_runner("whoami");
        return 1;
    }
    if ((kcontains_ci(input, "run") != 0 || kcontains_ci(input, "starte") != 0) && kcontains_ci(input, "diag") != 0) {
        console_writeln("[ai] Verstanden: Ich fuehre 'diag' aus.");
        g_runner("diag");
        return 1;
    }
    return 0;
}

void ai_route(const char* input) {
    static const char* help_words[] = {"hilfe", "help", "how", "was kann", "what can"};
    static const char* task_words[] = {"task", "dienst", "service", "laeuft", "running"};
    static const char* memory_words[] = {"speicher", "memory", "heap", "ram", "free"};
    static const char* layout_words[] = {"settings", "einstellung", "sprache", "language", "tastatur", "keyboard", "keymap", "hostname", "dhcp", "gateway"};
    static const char* user_words[] = {"benutzer", "user", "login", "whoami", "elevate", "rechte"};
    static const char* editor_words[] = {"editor", "notiz", "text", "lumen", "nano"};
    static const char* fs_words[] = {"dateisystem", "filesystem", "ordner", "datei", "file", "folder"};
    static const char* status_words[] = {"status", "zustand", "overview", "ueberblick", "diagnose", "diag"};
    static const char* greeting_words[] = {"hallo", "hello", "hi", "hey"};
    static const char* reboot_words[] = {"reboot", "neustart", "restart"};
    static const char* interrupt_words[] = {"ctrl", "abbrechen", "interrupt", "cancel"};
    static const char* lang_de_words[] = {"deutsch", "german", "deutsch einstellen"};
    static const char* lang_en_words[] = {"englisch", "english", "english settings"};
    static const char* banner_words[] = {"banner", "welcome", "willkommen"};
    static const char* version_words[] = {"version", "build"};
    static const char* about_words[] = {"cyralith", "about", "ueber", "uber"};
    static const char* network_words[] = {"netzwerk", "network", "internet", "ip", "dhcp", "gateway"};
    static const char* nic_words[] = {"nic", "adapter", "ethernet", "pci", "realtek", "intel", "karte"};
    static const char* driver_words[] = {"treiber", "driver", "e1000", "netup", "mac", "netzwerkkarte"};
    static const char* probe_words[] = {"probe", "test frame", "rohdaten", "paket", "packet", "netprobe"};
    static const char* app_words[] = {"app", "programm", "program", "install", "browser", "monitor"};
    static const char* save_words[] = {"save", "speichern", "persist", "platte", "disk", "loadfs"};
    static const char* copy_words[] = {"kopier", "copy", "move", "verschieb", "find", "suche"};
    static const char* custom_words[] = {"cmd", "befehl", "command", "skript", "script", "wrapper", "extern", "programm", "manifest"};
    static const char* security_words[] = {"sicherheit", "security", "sandbox", "rechte", "permissions", "trust", "freigabe", "approve"};
    static const char* automation_words[] = {"automation", "automatisierung", "schedule", "spaeter", "later", "job"};
    static const char* recovery_words[] = {"recovery", "recover", "doctor", "repair", "diagnose-assistent"};
    int best = 0;
    int category = 0;
    int score;

    console_writeln("[ai] Ich schaue kurz, was du meinst ...");

    if (try_safe_nl_command(input) != 0) {
        return;
    }

    score = count_hits(input, help_words, sizeof(help_words) / sizeof(help_words[0]));
    if (score > best) { best = score; category = 1; }
    score = count_hits(input, task_words, sizeof(task_words) / sizeof(task_words[0]));
    if (score > best) { best = score; category = 2; }
    score = count_hits(input, memory_words, sizeof(memory_words) / sizeof(memory_words[0]));
    if (score > best) { best = score; category = 3; }
    score = count_hits(input, layout_words, sizeof(layout_words) / sizeof(layout_words[0]));
    if (score > best) { best = score; category = 4; }
    score = count_hits(input, user_words, sizeof(user_words) / sizeof(user_words[0]));
    if (score > best) { best = score; category = 5; }
    score = count_hits(input, editor_words, sizeof(editor_words) / sizeof(editor_words[0]));
    if (score > best) { best = score; category = 6; }
    score = count_hits(input, fs_words, sizeof(fs_words) / sizeof(fs_words[0]));
    if (score > best) { best = score; category = 7; }
    score = count_hits(input, status_words, sizeof(status_words) / sizeof(status_words[0]));
    if (score > best) { best = score; category = 8; }
    score = count_hits(input, greeting_words, sizeof(greeting_words) / sizeof(greeting_words[0]));
    if (score > best) { best = score; category = 9; }
    score = count_hits(input, reboot_words, sizeof(reboot_words) / sizeof(reboot_words[0]));
    if (score > best) { best = score; category = 10; }
    score = count_hits(input, interrupt_words, sizeof(interrupt_words) / sizeof(interrupt_words[0]));
    if (score > best) { best = score; category = 11; }
    score = count_hits(input, lang_de_words, sizeof(lang_de_words) / sizeof(lang_de_words[0]));
    if (score > best) { best = score; category = 12; }
    score = count_hits(input, lang_en_words, sizeof(lang_en_words) / sizeof(lang_en_words[0]));
    if (score > best) { best = score; category = 13; }
    score = count_hits(input, banner_words, sizeof(banner_words) / sizeof(banner_words[0]));
    if (score > best) { best = score; category = 14; }
    score = count_hits(input, version_words, sizeof(version_words) / sizeof(version_words[0]));
    if (score > best) { best = score; category = 15; }
    score = count_hits(input, about_words, sizeof(about_words) / sizeof(about_words[0]));
    if (score > best) { best = score; category = 16; }
    score = count_hits(input, network_words, sizeof(network_words) / sizeof(network_words[0]));
    if (score > best) { best = score; category = 17; }
    score = count_hits(input, nic_words, sizeof(nic_words) / sizeof(nic_words[0]));
    if (score > best) { best = score; category = 18; }
    score = count_hits(input, app_words, sizeof(app_words) / sizeof(app_words[0]));
    if (score > best) { best = score; category = 19; }
    score = count_hits(input, save_words, sizeof(save_words) / sizeof(save_words[0]));
    if (score > best) { best = score; category = 20; }
    score = count_hits(input, driver_words, sizeof(driver_words) / sizeof(driver_words[0]));
    if (score > best) { best = score; category = 21; }
    score = count_hits(input, probe_words, sizeof(probe_words) / sizeof(probe_words[0]));
    if (score > best) { best = score; category = 22; }
    score = count_hits(input, copy_words, sizeof(copy_words) / sizeof(copy_words[0]));
    if (score > best) { best = score; category = 23; }
    score = count_hits(input, custom_words, sizeof(custom_words) / sizeof(custom_words[0]));
    if (score > best) { best = score; category = 24; }
    score = count_hits(input, security_words, sizeof(security_words) / sizeof(security_words[0]));
    if (score > best) { best = score; category = 25; }
    score = count_hits(input, automation_words, sizeof(automation_words) / sizeof(automation_words[0]));
    if (score > best) { best = score; category = 26; }
    score = count_hits(input, recovery_words, sizeof(recovery_words) / sizeof(recovery_words[0]));
    if (score > best) { best = score; category = 27; }

    switch (category) {
        case 1:
            console_writeln("[ai] Tipp: Nutze 'help' oder 'quickstart'.");
            return;
        case 2:
            console_writeln("[ai] Ich zeige dir die Dienste.");
            print_task_overview();
            return;
        case 3:
            console_write("[ai] Freier Speicher: ");
            console_write_dec((uint32_t)kmem_free_bytes());
            console_writeln(" bytes");
            return;
        case 4:
            console_write("[ai] Aktuelles Layout: ");
            console_writeln(keyboard_layout_name());
            console_writeln("[ai] Tipp: Nutze 'settings' oder 'app run settings'.");
            return;
        case 5: {
            const user_t* current = user_current();
            console_write("[ai] Aktiver Benutzer: ");
            console_write(current->username);
            console_write(" [");
            console_write(user_is_master() != 0 ? "system" : user_role_name(current->role));
            console_writeln("]");
            console_writeln("[ai] Tipp: Nutze 'users', 'whoami', 'login <name>' oder 'elevate'.");
            return;
        }
        case 6:
            console_writeln("[ai] Tipp: Nutze 'edit <name>' fuer Lumen oder 'app run lumen'.");
            return;
        case 7: {
            char path[96];
            afs_pwd(path, sizeof(path));
            console_write("[ai] CyralithFS ist aktiv. Aktueller Ort: ");
            console_writeln(path);
            console_writeln("[ai] Tipp: Nutze 'ls', 'pwd', 'cd', 'mkdir', 'touch', 'cat'.");
            return;
        }
        case 8:
            console_writeln("[ai] Fuer einen schnellen Gesundheitscheck passt 'diag'.");
            console_write("[ai] Ticks seit Start: ");
            console_write_dec(timer_ticks());
            console_putc('\n');
            return;
        case 9:
            console_writeln("[ai] Hallo. Cyralith ist bereit.");
            return;
        case 10:
            console_writeln("[ai] Tipp: Nutze 'reboot'.");
            return;
        case 11:
            console_writeln("[ai] Tipp: Ctrl+C bricht die aktuelle Eingabe ab.");
            return;
        case 12:
            console_writeln("[ai] Tipp: Nutze 'settings' und waehle dort Sprache.");
            return;
        case 13:
            console_writeln("[ai] Tipp: Use 'settings' and choose language there.");
            return;
        case 14:
            console_writeln("[ai] Tipp: Nutze 'welcome'.");
            return;
        case 15:
            console_writeln("[ai] Tipp: Nutze 'version'.");
            return;
        case 16:
            console_writeln("[ai] Cyralith will einfach, modular und zweisprachig sein.");
            console_writeln("[ai] Programmiert von Obsidian.");
            return;
        case 17:
            console_write("[ai] Netzwerk: ");
            console_write(network_hostname());
            console_write(" / ");
            console_writeln(network_ip());
            console_writeln("[ai] Tipp: Nutze 'settings' fuer Werte und 'network'/'nic' fuer Status.");
            return;
        case 18:
            if (network_nic_count() == 0U) {
                console_writeln("[ai] Noch keine NIC erkannt. In VirtualBox helfen oft Intel PRO/1000 oder PCnet.");
            } else {
                console_write("[ai] Erkannte NICs: ");
                console_write_dec((uint32_t)network_nic_count());
                console_putc('\n');
                console_write("[ai] Erste Karte: ");
                console_writeln(network_nic_name(0U));
            }
            console_writeln("[ai] Tipp: Nutze 'nic'.");
            return;
        case 19:
            print_app_overview();
            console_writeln("[ai] Tipp: Nutze 'app list', 'app info lumen' oder 'launch <name>'.");
            return;
        case 20:
            console_writeln("[ai] Tipp: Nutze 'savefs' fuer die virtuelle Platte, 'loadfs' zum Wiederladen oder 'disk' fuer den Backend-Status.");
            return;
        case 21:
            if (network_driver_active() != 0) {
                console_write("[ai] Aktiver Treiber: ");
                console_writeln(network_driver_name());
                console_write("[ai] MAC: ");
                console_writeln(network_mac_address());
            } else {
                console_writeln("[ai] Noch kein aktiver Netzwerktreiber. Bei Intel PRO/1000 hilft 'netup'.");
            }
            return;
        case 22:
            console_writeln("[ai] Fuer den ersten echten Rohdaten-Test passt 'netprobe'.");
            console_writeln("[ai] Vorher am besten 'netup' starten.");
            return;
        case 23:
            console_writeln("[ai] Fuer Dateien helfen jetzt 'cp', 'mv' und 'find'.");
            return;
        case 24:
            console_writeln("[ai] Fuer eigene Befehle nutze 'cmd new <name>' oder 'cmd add <name> <skriptpfad>'.");
            console_writeln("[ai] Danach zeigen 'prog info <name>' und 'which <name>' den Sicherheitsstatus.");
            return;
        case 25:
            console_writeln("[ai] Externe Programme laufen mit Manifesten, Rechten und Vertrauensstufen.");
            console_writeln("[ai] Tipp: 'prog info <name>', 'prog caps <name> fs-read,fs-write' und 'prog approve <name>'.");
            return;
        case 26:
            console_writeln("[ai] Fuer Zeitsteuerung helfen 'job add <sekunden> <befehl>', 'jobs' und 'job cancel <id>'.");
            return;
        case 27:
            console_writeln("[ai] Fuer Selbsthilfe helfen 'doctor' und 'recover'.");
            return;
        default:
            console_writeln("[ai] Ich bin noch lokal und regelbasiert. Versuch es mit 'help', 'status', 'network' oder 'settings'.");
            return;
    }
}
