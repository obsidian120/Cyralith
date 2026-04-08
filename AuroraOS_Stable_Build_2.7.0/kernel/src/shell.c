#include "shell.h"
#include "aurorafs.h"
#include "console.h"
#include "string.h"
#include "ai_core.h"
#include "memory.h"
#include "task.h"
#include "timer.h"
#include "keyboard.h"
#include "editor.h"
#include "user.h"
#include "network.h"
#include "app.h"
#include "extprog.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

#define INPUT_MAX 256
#define HISTORY_MAX 16
#define VERSION_TEXT "AuroraOS Stable Build 2.7.0"

typedef enum {
    LANG_DE = 0,
    LANG_EN = 1
} language_t;

static char input_buffer[INPUT_MAX];
static size_t input_len = 0;
static language_t g_language = LANG_DE;
static char history[HISTORY_MAX][INPUT_MAX];
static size_t history_count = 0;
static int history_view = -1;
static char history_draft[INPUT_MAX];


static unsigned int g_program_caps = 0U;
static int g_program_active = 0;
static char g_program_name[32];
static char g_program_trust[16];

typedef enum {
    SETTINGS_VIEW_HOME = 0,
    SETTINGS_VIEW_GENERAL = 1,
    SETTINGS_VIEW_NETWORK = 2,
    SETTINGS_VIEW_SECURITY = 3,
    SETTINGS_VIEW_EXPERT = 4
} settings_view_t;

enum {
    SETTINGS_EDIT_NONE = 0,
    SETTINGS_EDIT_HOSTNAME = 1,
    SETTINGS_EDIT_IP = 2,
    SETTINGS_EDIT_GATEWAY = 3
};

enum {
    SETTINGS_HOME_GENERAL = 0,
    SETTINGS_HOME_NETWORK = 1,
    SETTINGS_HOME_SECURITY = 2,
    SETTINGS_HOME_EXPERT_MODE = 3,
    SETTINGS_HOME_EXPERT_PAGE = 4,
    SETTINGS_HOME_SAVE = 5,
    SETTINGS_HOME_EXIT = 6,
    SETTINGS_HOME_COUNT = 7
};

enum {
    SETTINGS_GENERAL_LANGUAGE = 0,
    SETTINGS_GENERAL_KEYBOARD = 1,
    SETTINGS_GENERAL_AI = 2,
    SETTINGS_GENERAL_HINTS = 3,
    SETTINGS_GENERAL_BACK = 4,
    SETTINGS_GENERAL_COUNT = 5
};

enum {
    SETTINGS_NETWORK_HOSTNAME = 0,
    SETTINGS_NETWORK_DHCP = 1,
    SETTINGS_NETWORK_IP = 2,
    SETTINGS_NETWORK_GATEWAY = 3,
    SETTINGS_NETWORK_BACK = 4,
    SETTINGS_NETWORK_COUNT = 5
};

enum {
    SETTINGS_SECURITY_APPROVAL = 0,
    SETTINGS_SECURITY_LEGACY = 1,
    SETTINGS_SECURITY_AUTOAPPROVE = 2,
    SETTINGS_SECURITY_BACK = 3,
    SETTINGS_SECURITY_COUNT = 4
};

enum {
    SETTINGS_EXPERT_AUTONET = 0,
    SETTINGS_EXPERT_DRIVER_DEBUG = 1,
    SETTINGS_EXPERT_AUTOSAVE = 2,
    SETTINGS_EXPERT_VERBOSE_DIAG = 3,
    SETTINGS_EXPERT_BACK = 4,
    SETTINGS_EXPERT_COUNT = 5
};

static int g_settings_active = 0;
static settings_view_t g_settings_view = SETTINGS_VIEW_HOME;
static int g_settings_selected = 0;
static int g_settings_edit_field = SETTINGS_EDIT_NONE;
static char g_settings_input[64];
static size_t g_settings_input_len = 0U;
static char g_settings_notice[128];
static char g_settings_edit_title[40];

static int g_setting_ai_smart = 1;
static int g_setting_startup_hints = 1;
static int g_setting_require_program_approval = 1;
static int g_setting_allow_legacy_commands = 1;
static int g_setting_default_autoapprove = 0;
static int g_setting_expert_mode = 0;
static int g_setting_driver_autostart = 0;
static int g_setting_driver_debug = 0;
static int g_setting_autosave = 0;
static int g_setting_verbose_diag = 0;

static void run_command(const char* cmd);
static void print_afs_result(int rc, const char* ok_de, const char* ok_en, const char* fail_de, const char* fail_en);
static void settings_open(void);
static void settings_handle_key(int key);
static int save_system_state(void);
static int load_system_state(void);
static const char* tr(const char* de, const char* en) {
    return g_language == LANG_EN ? en : de;
}

static void prompt_path_suffix(char* out, size_t max) {
    char cwd[96];
    const char* home = user_current()->home;
    size_t home_len = kstrlen(home);

    afs_pwd(cwd, sizeof(cwd));
    if (kstrcmp(cwd, home) == 0) {
        out[0] = '\0';
        return;
    }
    if (kstrncmp(cwd, home, home_len) == 0 && cwd[home_len] == '/') {
        kstrcpy(out, cwd + home_len);
        return;
    }
    kstrcpy(out, cwd);
    if (max > 0U) {
        out[max - 1U] = '\0';
    }
}

static void prompt(void) {
    char suffix[96];
    prompt_path_suffix(suffix, sizeof(suffix));
    console_write("aurora(");
    console_write(g_language == LANG_EN ? "en" : "de");
    console_write(")@");
    console_write(user_current()->username);
    console_write(suffix);
    console_write(user_is_master() != 0 ? "# " : "> ");
}

void shell_show_welcome(void) {
    console_writeln("============================================================");
    console_writeln("AuroraOS Stable Build 2.7.0");
    console_writeln("Thank you for using AuroraOS!");
    console_writeln("Type 'help' for help.");
    console_writeln("Programmiert von Obsidian.");
    console_writeln("============================================================");
}

static void print_apps(void) {
    size_t i;
    console_writeln(tr("Apps und Bereiche:", "Apps and areas:"));
    console_writeln(tr("  system    - Shell, Benutzer, AuroraFS und Grunddienste.", "  system    - Shell, users, AuroraFS and core services."));
    for (i = 0U; i < app_count(); ++i) {
        const app_t* app = app_get(i);
        if (app == (const app_t*)0) {
            continue;
        }
        console_write("  - ");
        console_write(app->name);
        console_write(" [");
        console_write(app->installed != 0U ? tr("installiert", "installed") : tr("optional", "optional"));
        console_write(app->builtin != 0U ? tr(", intern] ", ", built-in] ") : "] ");
        console_writeln(app->description);
    }
    console_writeln(tr("  Hinweis: Nutze 'app list', 'app install <name>', 'app remove <name>' und 'app run <name>'.", "  Hint: use 'app list', 'app install <name>', 'app remove <name>' and 'app run <name>'."));
}

static void print_memory_status(void) {
    console_writeln(tr("Speicher:", "Memory:"));
    console_write(tr("  Insgesamt: ", "  Total:      "));
    console_write_dec((uint32_t)kmem_total_bytes());
    console_writeln(" bytes");
    console_write(tr("  Belegt:    ", "  Used:       "));
    console_write_dec((uint32_t)kmem_used_bytes());
    console_writeln(" bytes");
    console_write(tr("  Frei:      ", "  Free:       "));
    console_write_dec((uint32_t)kmem_free_bytes());
    console_writeln(" bytes");
}

static void print_tasks(void) {
    console_writeln(tr("Aktive Systembereiche:", "Active system areas:"));
    for (size_t i = 0; i < task_count(); ++i) {
        const task_t* task = task_get(i);
        if (task == (const task_t*)0) {
            continue;
        }

        console_write("  - ");
        console_write(task->name);
        console_write(" [");
        console_write(task_state_name(task->state));
        console_write("] ");
        console_writeln(task->description);
    }
}

static void print_history(void) {
    console_writeln(tr("Letzte Befehle:", "Recent commands:"));
    if (history_count == 0U) {
        console_writeln(tr("  Noch nichts gespeichert.", "  Nothing saved yet."));
        return;
    }

    for (size_t i = 0; i < history_count; ++i) {
        console_write("  ");
        console_write_dec((uint32_t)(i + 1U));
        console_write(". ");
        console_writeln(history[i]);
    }
}

static void print_users(void) {
    console_writeln(tr("Bekannte Benutzer und Rollen:", "Known users and roles:"));
    for (size_t i = 0; i < user_count(); ++i) {
        const user_t* user = user_get(i);
        if (user == (const user_t*)0) {
            continue;
        }
        console_write("  - ");
        console_write(user->username);
        console_write(" [");
        console_write(user_role_name(user->role));
        console_write("] ");
        console_write(user->display_name);
        console_write("  group=");
        console_write(user->group);
        console_write("  home=");
        console_writeln(user->home);
    }
    console_writeln(tr("  - system-mode  [special] Root-aehnlicher Sondermodus per 'elevate <passwort>'.", "  - system-mode  [special] Root-like special mode via 'elevate <password>'."));
}

static void print_whoami(void) {
    const user_t* current = user_current();
    console_write(tr("Aktiver Benutzer: ", "Current user: "));
    console_write(current->username);
    console_write(" [");
    console_write(user_is_master() != 0 ? tr("system", "system") : user_role_name(current->role));
    console_writeln("]");
    console_write(tr("Home: ", "Home: "));
    console_writeln(current->home);
    console_write(tr("Gruppe: ", "Group: "));
    console_writeln(user_current_group());
    console_write(tr("Sitzung: ", "Session: "));
    console_writeln(user_is_master() != 0 ? tr("System-Modus aktiv (#)", "System mode active (#)") : tr("Normale Rechte (>)", "Normal rights (>)"));
}

static void print_layout(void) {
    console_write(tr("Tastatur-Layout: ", "Keyboard layout: "));
    console_writeln(keyboard_layout_name());
}

static void print_working_path(void) {
    char path[96];
    afs_pwd(path, sizeof(path));
    console_write(tr("Aktueller Ort: ", "Current location: "));
    console_writeln(path);
}

static void print_disk_status(void) {
    console_writeln(tr("AuroraFS Speicher:", "AuroraFS storage:"));
    console_write(tr("  Backend: ", "  Backend: "));
    console_writeln(afs_persistence_name());
    if (afs_persistence_available() != 0) {
        console_writeln(tr("  Status: virtuelle ATA-Platte erkannt. savefs/loadfs sind aktiv.", "  Status: virtual ATA disk detected. savefs/loadfs are active."));
    } else {
        console_writeln(tr("  Status: keine virtuelle Platte erkannt. AuroraFS bleibt nur im RAM.", "  Status: no virtual disk detected. AuroraFS stays in RAM only."));
        console_writeln(tr("  Tipp: Fuege in VirtualBox oder QEMU eine kleine virtuelle Festplatte hinzu.", "  Tip: add a small virtual hard disk in VirtualBox or QEMU."));
    }
}

static void print_network_status(void) {
    console_writeln(tr("AuroraNet im Ueberblick:", "AuroraNet overview:"));
    console_write(tr("  Backend: ", "  Backend: "));
    console_writeln(network_backend_name());
    console_write(tr("  Hostname: ", "  Hostname: "));
    console_writeln(network_hostname());
    console_write(tr("  Adresse: ", "  Address: "));
    console_writeln(network_ip());
    console_write(tr("  Gateway: ", "  Gateway: "));
    console_writeln(network_gateway());
    console_write(tr("  DHCP: ", "  DHCP: "));
    console_writeln(network_dhcp_enabled() != 0 ? tr("aktiv", "enabled") : tr("aus", "off"));
    console_write(tr("  Erkannte NICs: ", "  Detected NICs: "));
    console_write_dec((uint32_t)network_nic_count());
    console_putc('\n');
    console_write(tr("  Treiber: ", "  Driver: "));
    console_writeln(network_driver_active() != 0 ? network_driver_name() : tr("noch aus", "off"));
    console_write(tr("  Verbindung: ", "  Link: "));
    console_writeln(network_driver_active() != 0 ? (network_link_up() != 0 ? tr("aktiv", "up") : tr("Treiber aktiv, Link unklar", "driver active, link not confirmed")) : tr("noch nicht gestartet", "not started yet"));
    console_write(tr("  MAC: ", "  MAC: "));
    console_writeln(network_mac_address());
    if (g_setting_expert_mode != 0) {
        console_write(tr("  Expertenmodus: ", "  Expert mode: "));
        console_writeln(g_setting_driver_debug != 0 ? tr("Treiberdiagnose aktiv", "driver diagnostics active") : tr("kompakt", "compact"));
    }
    console_writeln(tr("  Tipp: 'netup' startet den e1000-Treiber. 'netprobe' sendet einen kleinen Rohdaten-Test.", "  Tip: 'netup' starts the e1000 driver. 'netprobe' sends a small raw packet test."));
}


static void print_nic_status(void) {
    size_t i;
    char location[32];
    console_writeln(tr("Erkannte Netzwerkadapter:", "Detected network adapters:"));
    if (network_nic_count() == 0U) {
        console_writeln(tr("  Keine PCI-Netzwerkkarte erkannt. In VirtualBox hilft oft Intel PRO/1000 oder PCnet.", "  No PCI network card detected. In VirtualBox, Intel PRO/1000 or PCnet often helps."));
        return;
    }
    for (i = 0U; i < network_nic_count(); ++i) {
        network_nic_location(i, location, sizeof(location));
        console_write("  - ");
        console_write(network_nic_name(i));
        console_write(" (");
        console_write(location);
        console_writeln(")");
        console_write("    ");
        console_writeln(network_nic_driver_hint(i));
    }
    if (network_driver_active() == 0) {
        console_writeln(tr("  Noch kein Treiber aktiv. Nutze 'netup'.", "  No driver active yet. Use 'netup'."));
    }
}


static void print_fs(void) {
    char path[96];
    afs_pwd(path, sizeof(path));
    console_writeln(tr("AuroraFS im Ueberblick:", "AuroraFS overview:"));
    console_write(tr("  Name: ", "  Name: "));
    console_writeln(afs_name());
    console_writeln(tr("  Hauptordner: /system, /home, /apps, /temp", "  Main folders: /system, /home, /apps, /temp"));
    console_writeln(tr("  /system und /apps sind system-eigene Bereiche.", "  /system and /apps are system-owned areas."));
    console_writeln(tr("  /home/<user> gehoert dem jeweiligen Benutzer und ist privat.", "  /home/<user> belongs to that user and is private."));
    console_writeln(tr("  /temp ist ein gemeinsamer Bereich fuer schnelle Tests.", "  /temp is a shared area for quick tests."));
    console_writeln(tr("  Nutze stat/protect/owner fuer Rechte und Besitz.", "  Use stat/protect/owner for rights and ownership."));
    console_write(tr("  Speicher-Art: ", "  Storage mode: "));
    console_writeln(afs_persistence_available() != 0 ? tr("persistenter ATA-Datentraeger aktiv", "persistent ATA disk active") : tr("nur RAM (keine Platte gefunden)", "RAM only (no disk found)"));
    console_write(tr("  Aktueller Ort: ", "  Current location: "));
    console_writeln(path);
    console_write(tr("  Eintraege: ", "  Entries: "));
    console_write_dec((uint32_t)afs_node_count());
    console_putc('\n');
}

static void print_status(void) {
    char path[96];
    afs_pwd(path, sizeof(path));
    console_writeln(tr("Kurzer Systemueberblick:", "Quick system overview:"));
    console_writeln(tr("  AuroraOS laeuft im stabilen Startmodus.", "  AuroraOS is running in stable mode."));
    print_whoami();
    print_layout();
    console_write(tr("  Freier Speicher: ", "  Free memory: "));
    console_write_dec((uint32_t)kmem_free_bytes());
    console_writeln(" bytes");
    console_write(tr("  AuroraFS Ort: ", "  AuroraFS path: "));
    console_writeln(path);
    console_write(tr("  AuroraFS Speicher: ", "  AuroraFS storage: "));
    console_writeln(afs_persistence_name());
    console_write(tr("  Netzwerk: ", "  Network: "));
    console_write(network_hostname());
    console_write(" / ");
    console_write(network_ip());
    console_write(" / NICs=");
    console_write_dec((uint32_t)network_nic_count());
    console_write(tr(" / Treiber=", " / Driver="));
    console_writeln(network_driver_active() != 0 ? network_driver_name() : tr("noch aus", "off"));
    console_write(tr("  Expertenmodus: ", "  Expert mode: "));
    console_writeln(g_setting_expert_mode != 0 ? tr("aktiv", "enabled") : tr("aus", "off"));
    console_write(tr("  Installierte Apps: ", "  Installed apps: "));
    { uint32_t installed = 0U; size_t i; for (i = 0U; i < app_count(); ++i) { const app_t* app = app_get(i); if (app != (const app_t*)0 && app->installed != 0U) { installed++; } } console_write_dec(installed); console_putc('\n'); }
    console_write(tr("  Offene Lumen-Dateien: ", "  Open Lumen files: "));
    console_write_dec((uint32_t)editor_document_count());
    console_putc('\n');
    console_write(tr("  Ticks seit Start: ", "  Ticks since boot: "));
    console_write_dec(timer_ticks());
    console_putc('\n');
}


static void print_version(void) {
    console_writeln(VERSION_TEXT);
}

static void print_quickstart(void) {
    console_writeln(tr("Schnellstart:", "Quick start:"));
    console_writeln(tr("  1. help              - Zeigt alle wichtigen Befehle.", "  1. help              - Shows the main commands."));
    console_writeln(tr("  2. settings          - Oeffnet die uebersichtlichen Einstellungen.", "  2. settings          - Opens the clear settings screen."));
    console_writeln(tr("  3. whoami            - Zeigt, wer gerade angemeldet ist.", "  3. whoami            - Shows who is currently signed in."));
    console_writeln(tr("  4. cd ~              - Springt in deinen Home-Ordner.", "  4. cd ~              - Jumps to your home folder."));
    console_writeln(tr("  5. app list          - Zeigt installierte und optionale Apps.", "  5. app list          - Shows installed and optional apps."));
    console_writeln(tr("  6. edit notes.txt    - Oeffnet Lumen.", "  6. edit notes.txt    - Opens Lumen."));
    console_writeln(tr("  7. nic               - Zeigt erkannte Netzwerkadapter.", "  7. nic               - Shows detected network adapters."));
    console_writeln(tr("  8. netup             - Startet den ersten e1000-Treiber.", "  8. netup             - Starts the first e1000 driver."));
    console_writeln(tr("  9. elevate aurora    - Aktiviert den root-aehnlichen System-Modus.", "  9. elevate aurora    - Activates the root-like system mode."));
    console_writeln(tr(" 10. savefs            - Speichert Dateien, Nutzer, Netzwerk und Apps auf die virtuelle Platte.", " 10. savefs            - Saves files, users, network and apps to the virtual disk."));
    console_writeln(tr(" 11. cmd new hallo     - Erstellt einen eigenen Befehl.", " 11. cmd new hallo     - Creates your own command."));
    console_writeln(tr(" 12. prog info hallo   - Zeigt Rechte, Vertrauen und Freigabe.", " 12. prog info hallo   - Shows permissions, trust and approval."));
    console_writeln(tr(" 13. which hallo       - Zeigt, woher ein Befehl kommt.", " 13. which hallo       - Shows where a command comes from."));
}

static void set_language(const char* code) {
    if (kstrcmp(code, "de") == 0) {
        g_language = LANG_DE;
        console_writeln("Sprache gesetzt: Deutsch");
        return;
    }

    if (kstrcmp(code, "en") == 0) {
        g_language = LANG_EN;
        console_writeln("Language set: English");
        return;
    }

    console_writeln(tr("Nutze: lang <de|en>", "Use: lang <de|en>"));
}

static void set_layout(const char* code) {
    if (kstrcmp(code, "de") == 0) {
        keyboard_set_layout(KEYBOARD_LAYOUT_DE);
        console_writeln(tr("Layout gesetzt: Deutsch (QWERTZ)", "Layout set: German (QWERTZ)"));
        return;
    }

    if (kstrcmp(code, "us") == 0 || kstrcmp(code, "en") == 0) {
        keyboard_set_layout(KEYBOARD_LAYOUT_US);
        console_writeln(tr("Layout gesetzt: US (QWERTY)", "Layout set: US (QWERTY)"));
        return;
    }

    console_writeln(tr("Nutze: layout <de|us>", "Use: layout <de|us>"));
}


static const char* settings_view_title(settings_view_t view) {
    switch (view) {
        case SETTINGS_VIEW_HOME: return tr("Einstellungen", "Settings");
        case SETTINGS_VIEW_GENERAL: return tr("Allgemein", "General");
        case SETTINGS_VIEW_NETWORK: return tr("Netzwerk", "Network");
        case SETTINGS_VIEW_SECURITY: return tr("Sicherheit", "Security");
        case SETTINGS_VIEW_EXPERT: return tr("Expertenmodus", "Expert mode");
        default: return "?";
    }
}

static int settings_view_count(settings_view_t view) {
    switch (view) {
        case SETTINGS_VIEW_HOME: return SETTINGS_HOME_COUNT;
        case SETTINGS_VIEW_GENERAL: return SETTINGS_GENERAL_COUNT;
        case SETTINGS_VIEW_NETWORK: return SETTINGS_NETWORK_COUNT;
        case SETTINGS_VIEW_SECURITY: return SETTINGS_SECURITY_COUNT;
        case SETTINGS_VIEW_EXPERT: return SETTINGS_EXPERT_COUNT;
        default: return 0;
    }
}

static const char* settings_item_title(settings_view_t view, int index) {
    switch (view) {
        case SETTINGS_VIEW_HOME:
            switch (index) {
                case SETTINGS_HOME_GENERAL: return tr("Allgemein", "General");
                case SETTINGS_HOME_NETWORK: return tr("Netzwerk", "Network");
                case SETTINGS_HOME_SECURITY: return tr("Sicherheit", "Security");
                case SETTINGS_HOME_EXPERT_MODE: return tr("Expertenmodus", "Expert mode");
                case SETTINGS_HOME_EXPERT_PAGE: return tr("Treiber & System tief", "Drivers & low-level");
                case SETTINGS_HOME_SAVE: return tr("Jetzt speichern", "Save now");
                case SETTINGS_HOME_EXIT: return tr("Einstellungen schliessen", "Close settings");
                default: return "?";
            }
        case SETTINGS_VIEW_GENERAL:
            switch (index) {
                case SETTINGS_GENERAL_LANGUAGE: return tr("Sprache", "Language");
                case SETTINGS_GENERAL_KEYBOARD: return tr("Tastatur-Layout", "Keyboard layout");
                case SETTINGS_GENERAL_AI: return tr("KI-Hilfe", "AI help");
                case SETTINGS_GENERAL_HINTS: return tr("Start-Hinweise", "Startup hints");
                case SETTINGS_GENERAL_BACK: return tr("Zurueck", "Back");
                default: return "?";
            }
        case SETTINGS_VIEW_NETWORK:
            switch (index) {
                case SETTINGS_NETWORK_HOSTNAME: return tr("Computername", "Computer name");
                case SETTINGS_NETWORK_DHCP: return "DHCP";
                case SETTINGS_NETWORK_IP: return "IP";
                case SETTINGS_NETWORK_GATEWAY: return tr("Gateway", "Gateway");
                case SETTINGS_NETWORK_BACK: return tr("Zurueck", "Back");
                default: return "?";
            }
        case SETTINGS_VIEW_SECURITY:
            switch (index) {
                case SETTINGS_SECURITY_APPROVAL: return tr("Programm-Freigaben", "Program approvals");
                case SETTINGS_SECURITY_LEGACY: return tr("Alte Wrapper ohne Manifest", "Legacy wrappers without manifest");
                case SETTINGS_SECURITY_AUTOAPPROVE: return tr("Neue lokale Befehle auto-freigeben", "Auto-approve new local commands");
                case SETTINGS_SECURITY_BACK: return tr("Zurueck", "Back");
                default: return "?";
            }
        case SETTINGS_VIEW_EXPERT:
            switch (index) {
                case SETTINGS_EXPERT_AUTONET: return tr("Netzwerktreiber beim Start versuchen", "Try network driver at startup");
                case SETTINGS_EXPERT_DRIVER_DEBUG: return tr("Treiber-Diagnose erweitern", "Driver diagnostics");
                case SETTINGS_EXPERT_AUTOSAVE: return tr("Aenderungen automatisch speichern", "Auto-save changes");
                case SETTINGS_EXPERT_VERBOSE_DIAG: return tr("Ausfuehrlichere Diagnose", "Verbose diagnostics");
                case SETTINGS_EXPERT_BACK: return tr("Zurueck", "Back");
                default: return "?";
            }
        default:
            return "?";
    }
}

static void settings_item_value(settings_view_t view, int index, char* out, size_t max) {
    out[0] = '\0';
    switch (view) {
        case SETTINGS_VIEW_HOME:
            switch (index) {
                case SETTINGS_HOME_GENERAL:
                    kstrcpy(out, g_language == LANG_DE ? "Deutsch / DE" : "English / EN");
                    break;
                case SETTINGS_HOME_NETWORK:
                    kstrcpy(out, network_hostname());
                    break;
                case SETTINGS_HOME_SECURITY:
                    kstrcpy(out, g_setting_require_program_approval != 0 ? tr("streng", "strict") : tr("locker", "relaxed"));
                    break;
                case SETTINGS_HOME_EXPERT_MODE:
                    kstrcpy(out, g_setting_expert_mode != 0 ? tr("aktiv", "enabled") : tr("aus", "off"));
                    break;
                case SETTINGS_HOME_EXPERT_PAGE:
                    kstrcpy(out, g_setting_expert_mode != 0 ? tr("verfuegbar", "available") : tr("erst aktivieren", "enable first"));
                    break;
                case SETTINGS_HOME_SAVE:
                    kstrcpy(out, afs_persistence_available() != 0 ? tr("virtuelle Platte", "virtual disk") : tr("nur RAM", "RAM only"));
                    break;
                case SETTINGS_HOME_EXIT:
                    kstrcpy(out, tr("zur Shell", "to shell"));
                    break;
                default: break;
            }
            break;
        case SETTINGS_VIEW_GENERAL:
            switch (index) {
                case SETTINGS_GENERAL_LANGUAGE:
                    kstrcpy(out, g_language == LANG_DE ? tr("Deutsch", "German") : "English");
                    break;
                case SETTINGS_GENERAL_KEYBOARD:
                    kstrcpy(out, keyboard_get_layout() == KEYBOARD_LAYOUT_DE ? "DE / QWERTZ" : "US / QWERTY");
                    break;
                case SETTINGS_GENERAL_AI:
                    kstrcpy(out, g_setting_ai_smart != 0 ? tr("smarter Helfer", "smarter helper") : tr("einfacher Helfer", "basic helper"));
                    break;
                case SETTINGS_GENERAL_HINTS:
                    kstrcpy(out, g_setting_startup_hints != 0 ? tr("anzeigen", "show") : tr("ausblenden", "hide"));
                    break;
                case SETTINGS_GENERAL_BACK:
                    kstrcpy(out, tr("zur Uebersicht", "to overview"));
                    break;
                default: break;
            }
            break;
        case SETTINGS_VIEW_NETWORK:
            switch (index) {
                case SETTINGS_NETWORK_HOSTNAME:
                    kstrcpy(out, network_hostname());
                    break;
                case SETTINGS_NETWORK_DHCP:
                    kstrcpy(out, network_dhcp_enabled() != 0 ? tr("aktiv", "enabled") : tr("aus", "off"));
                    break;
                case SETTINGS_NETWORK_IP:
                    kstrcpy(out, network_ip());
                    break;
                case SETTINGS_NETWORK_GATEWAY:
                    kstrcpy(out, network_gateway());
                    break;
                case SETTINGS_NETWORK_BACK:
                    kstrcpy(out, tr("zur Uebersicht", "to overview"));
                    break;
                default: break;
            }
            break;
        case SETTINGS_VIEW_SECURITY:
            switch (index) {
                case SETTINGS_SECURITY_APPROVAL:
                    kstrcpy(out, g_setting_require_program_approval != 0 ? tr("Pflicht", "required") : tr("optional lokal", "optional local"));
                    break;
                case SETTINGS_SECURITY_LEGACY:
                    kstrcpy(out, g_setting_allow_legacy_commands != 0 ? tr("erlaubt", "allowed") : tr("gesperrt", "blocked"));
                    break;
                case SETTINGS_SECURITY_AUTOAPPROVE:
                    kstrcpy(out, g_setting_default_autoapprove != 0 ? tr("ja", "yes") : tr("nein", "no"));
                    break;
                case SETTINGS_SECURITY_BACK:
                    kstrcpy(out, tr("zur Uebersicht", "to overview"));
                    break;
                default: break;
            }
            break;
        case SETTINGS_VIEW_EXPERT:
            switch (index) {
                case SETTINGS_EXPERT_AUTONET:
                    kstrcpy(out, g_setting_driver_autostart != 0 ? tr("an", "on") : tr("aus", "off"));
                    break;
                case SETTINGS_EXPERT_DRIVER_DEBUG:
                    kstrcpy(out, g_setting_driver_debug != 0 ? tr("mehr Details", "more details") : tr("kompakt", "compact"));
                    break;
                case SETTINGS_EXPERT_AUTOSAVE:
                    kstrcpy(out, g_setting_autosave != 0 ? tr("an", "on") : tr("aus", "off"));
                    break;
                case SETTINGS_EXPERT_VERBOSE_DIAG:
                    kstrcpy(out, g_setting_verbose_diag != 0 ? tr("ausfuehrlich", "verbose") : tr("normal", "normal"));
                    break;
                case SETTINGS_EXPERT_BACK:
                    kstrcpy(out, tr("zur Uebersicht", "to overview"));
                    break;
                default: break;
            }
            break;
        default: break;
    }
    if (max > 0U) {
        out[max - 1U] = '\0';
    }
}

static int settings_item_requires_system(settings_view_t view, int index) {
    if (view == SETTINGS_VIEW_HOME) {
        return index == SETTINGS_HOME_SAVE;
    }
    if (view == SETTINGS_VIEW_NETWORK) {
        return index == SETTINGS_NETWORK_HOSTNAME || index == SETTINGS_NETWORK_DHCP || index == SETTINGS_NETWORK_IP || index == SETTINGS_NETWORK_GATEWAY;
    }
    if (view == SETTINGS_VIEW_SECURITY || view == SETTINGS_VIEW_EXPERT) {
        return 1;
    }
    return 0;
}

static void settings_set_notice(const char* de, const char* en) {
    kstrcpy(g_settings_notice, tr(de, en));
}

static void settings_draw_list(void) {
    int i;
    int count = settings_view_count(g_settings_view);
    console_clear();
    console_writeln("+==============================================================================+");
    console_writeln("|                              AuroraOS Settings                               |");
    console_writeln("+==============================================================================+");
    console_write("| ");
    console_write(settings_view_title(g_settings_view));
    console_writeln(g_settings_view == SETTINGS_VIEW_HOME ? tr(" - Uebersicht wie menuconfig, nur leichter.", " - overview like menuconfig, only easier.") : tr(" - Pfeile waehlen, Enter aendert.", " - arrows select, Enter changes."));
    console_writeln("+------------------------------------------------------------------------------+");
    for (i = 0; i < count; ++i) {
        char value[96];
        settings_item_value(g_settings_view, i, value, sizeof(value));
        console_write(i == g_settings_selected ? "> " : "  ");
        console_write(settings_item_title(g_settings_view, i));
        console_write(" : ");
        console_write(value);
        if (settings_item_requires_system(g_settings_view, i) != 0 && user_is_master() == 0) {
            console_write(" ");
            console_write(tr("[system]", "[system]"));
        }
        console_putc('\n');
    }
    console_writeln("+------------------------------------------------------------------------------+");
    console_write(tr(" Sprache=", " Language="));
    console_write(g_language == LANG_DE ? tr("Deutsch", "German") : "English");
    console_write(tr("  Layout=", "  Layout="));
    console_write(keyboard_get_layout() == KEYBOARD_LAYOUT_DE ? "DE" : "US");
    console_write(tr("  KI=", "  AI="));
    console_write(g_setting_ai_smart != 0 ? tr("smart", "smart") : tr("einfach", "basic"));
    console_write(tr("  Expert=", "  Expert="));
    console_writeln(g_setting_expert_mode != 0 ? tr("an", "on") : tr("aus", "off"));
    console_writeln(tr(" Steuerung: Pfeil hoch/runter = waehlen, Enter = aendern, q oder Ctrl+C = zurueck.", " Controls: Up/Down = select, Enter = change, q or Ctrl+C = back."));
    if (g_settings_notice[0] != '\0') {
        console_write(tr(" Meldung: ", " Message: "));
        console_writeln(g_settings_notice);
    } else {
        console_writeln(tr(" Meldung: bereit.", " Message: ready."));
    }
    console_writeln("+==============================================================================+");
}

static void settings_begin_text_edit(int field, const char* title, const char* initial) {
    char copy[64];
    copy[0] = '\0';
    if (initial != (const char*)0) {
        kstrcpy(copy, initial);
    }
    g_settings_edit_field = field;
    g_settings_input[0] = '\0';
    g_settings_input_len = 0U;
    g_settings_edit_title[0] = '\0';
    if (title != (const char*)0) {
        kstrcpy(g_settings_edit_title, title);
    }
    if (initial != (const char*)0) {
        kstrcpy(g_settings_input, copy);
        g_settings_input_len = kstrlen(g_settings_input);
    }
    console_clear();
    console_writeln("+==============================================================================+");
    console_writeln("|                        AuroraOS Settings / Bearbeiten                         |");
    console_writeln("+==============================================================================+");
    console_write(tr(" Feld: ", " Field: "));
    console_writeln(g_settings_edit_title);
    console_writeln(tr(" Gib den neuen Wert ein und druecke Enter. Ctrl+C verwirft die Aenderung.", " Enter the new value and press Enter. Ctrl+C discards the change."));
    console_writeln(" ");
    console_write(tr(" Neuer Wert: ", " New value: "));
    console_write(g_settings_input);
}

static void settings_go_home(void) {
    g_settings_view = SETTINGS_VIEW_HOME;
    g_settings_selected = 0;
    settings_draw_list();
}

static void settings_apply_text_value(void) {
    int ok = -1;
    if (g_settings_input[0] == '\0') {
        settings_set_notice("Leerer Wert wurde verworfen.", "Empty value was ignored.");
        g_settings_edit_field = SETTINGS_EDIT_NONE;
        settings_draw_list();
        return;
    }
    if (g_settings_edit_field == SETTINGS_EDIT_HOSTNAME) {
        ok = network_set_hostname(g_settings_input);
        settings_set_notice(ok == 0 ? "Computername aktualisiert." : "Computername konnte nicht gesetzt werden.", ok == 0 ? "Computer name updated." : "Could not update computer name.");
    } else if (g_settings_edit_field == SETTINGS_EDIT_IP) {
        ok = network_set_ip(g_settings_input);
        settings_set_notice(ok == 0 ? "IP-Adresse aktualisiert." : "IP-Adresse konnte nicht gesetzt werden.", ok == 0 ? "IP address updated." : "Could not update IP address.");
    } else if (g_settings_edit_field == SETTINGS_EDIT_GATEWAY) {
        ok = network_set_gateway(g_settings_input);
        settings_set_notice(ok == 0 ? "Gateway aktualisiert." : "Gateway konnte nicht gesetzt werden.", ok == 0 ? "Gateway updated." : "Could not update gateway.");
    }
    g_settings_edit_field = SETTINGS_EDIT_NONE;
    settings_draw_list();
    (void)ok;
}

static void settings_activate_selected(void) {
    int rc;
    if (settings_item_requires_system(g_settings_view, g_settings_selected) != 0 && user_is_master() == 0) {
        settings_set_notice("Dafuer brauchst du System-Rechte. Nutze elevate aurora.", "You need system rights for this. Use elevate aurora.");
        settings_draw_list();
        return;
    }

    if (g_settings_view == SETTINGS_VIEW_HOME) {
        switch (g_settings_selected) {
            case SETTINGS_HOME_GENERAL: g_settings_view = SETTINGS_VIEW_GENERAL; g_settings_selected = 0; settings_draw_list(); return;
            case SETTINGS_HOME_NETWORK: g_settings_view = SETTINGS_VIEW_NETWORK; g_settings_selected = 0; settings_draw_list(); return;
            case SETTINGS_HOME_SECURITY: g_settings_view = SETTINGS_VIEW_SECURITY; g_settings_selected = 0; settings_draw_list(); return;
            case SETTINGS_HOME_EXPERT_MODE:
                g_setting_expert_mode = g_setting_expert_mode == 0 ? 1 : 0;
                settings_set_notice(g_setting_expert_mode != 0 ? "Expertenmodus aktiviert." : "Expertenmodus deaktiviert.", g_setting_expert_mode != 0 ? "Expert mode enabled." : "Expert mode disabled.");
                settings_draw_list();
                return;
            case SETTINGS_HOME_EXPERT_PAGE:
                if (g_setting_expert_mode == 0) {
                    settings_set_notice("Schalte zuerst den Expertenmodus ein.", "Enable expert mode first.");
                    settings_draw_list();
                    return;
                }
                g_settings_view = SETTINGS_VIEW_EXPERT;
                g_settings_selected = 0;
                settings_draw_list();
                return;
            case SETTINGS_HOME_SAVE:
                rc = save_system_state();
                settings_set_notice(rc == 0 ? "Einstellungen gespeichert." : "Speichern fehlgeschlagen. Wahrscheinlich keine virtuelle Platte.", rc == 0 ? "Settings saved." : "Save failed. Probably no virtual disk attached.");
                settings_draw_list();
                return;
            case SETTINGS_HOME_EXIT:
                g_settings_active = 0;
                console_clear();
                return;
            default: return;
        }
    }

    if (g_settings_view == SETTINGS_VIEW_GENERAL) {
        switch (g_settings_selected) {
            case SETTINGS_GENERAL_LANGUAGE:
                set_language(g_language == LANG_DE ? "en" : "de");
                settings_set_notice("Sprache umgeschaltet.", "Language switched.");
                settings_draw_list();
                return;
            case SETTINGS_GENERAL_KEYBOARD:
                set_layout(keyboard_get_layout() == KEYBOARD_LAYOUT_DE ? "us" : "de");
                settings_set_notice("Tastatur-Layout umgeschaltet.", "Keyboard layout switched.");
                settings_draw_list();
                return;
            case SETTINGS_GENERAL_AI:
                g_setting_ai_smart = g_setting_ai_smart == 0 ? 1 : 0;
                settings_set_notice("KI-Hilfe umgeschaltet.", "AI help switched.");
                settings_draw_list();
                return;
            case SETTINGS_GENERAL_HINTS:
                g_setting_startup_hints = g_setting_startup_hints == 0 ? 1 : 0;
                settings_set_notice("Start-Hinweise umgeschaltet.", "Startup hints switched.");
                settings_draw_list();
                return;
            case SETTINGS_GENERAL_BACK:
                settings_go_home();
                return;
            default: return;
        }
    }

    if (g_settings_view == SETTINGS_VIEW_NETWORK) {
        switch (g_settings_selected) {
            case SETTINGS_NETWORK_HOSTNAME: settings_begin_text_edit(SETTINGS_EDIT_HOSTNAME, settings_item_title(g_settings_view, g_settings_selected), network_hostname()); return;
            case SETTINGS_NETWORK_DHCP:
                rc = network_set_dhcp(network_dhcp_enabled() != 0 ? 0 : 1);
                settings_set_notice(rc == 0 ? "DHCP umgeschaltet." : "DHCP konnte nicht geaendert werden.", rc == 0 ? "DHCP switched." : "Could not change DHCP.");
                settings_draw_list();
                return;
            case SETTINGS_NETWORK_IP: settings_begin_text_edit(SETTINGS_EDIT_IP, settings_item_title(g_settings_view, g_settings_selected), network_ip()); return;
            case SETTINGS_NETWORK_GATEWAY: settings_begin_text_edit(SETTINGS_EDIT_GATEWAY, settings_item_title(g_settings_view, g_settings_selected), network_gateway()); return;
            case SETTINGS_NETWORK_BACK: settings_go_home(); return;
            default: return;
        }
    }

    if (g_settings_view == SETTINGS_VIEW_SECURITY) {
        switch (g_settings_selected) {
            case SETTINGS_SECURITY_APPROVAL:
                g_setting_require_program_approval = g_setting_require_program_approval == 0 ? 1 : 0;
                settings_set_notice("Programm-Freigaben umgeschaltet.", "Program approvals switched.");
                settings_draw_list();
                return;
            case SETTINGS_SECURITY_LEGACY:
                g_setting_allow_legacy_commands = g_setting_allow_legacy_commands == 0 ? 1 : 0;
                settings_set_notice("Legacy-Befehle umgeschaltet.", "Legacy command policy switched.");
                settings_draw_list();
                return;
            case SETTINGS_SECURITY_AUTOAPPROVE:
                g_setting_default_autoapprove = g_setting_default_autoapprove == 0 ? 1 : 0;
                settings_set_notice("Auto-Freigabe fuer neue lokale Befehle umgeschaltet.", "Auto-approval for new local commands switched.");
                settings_draw_list();
                return;
            case SETTINGS_SECURITY_BACK:
                settings_go_home();
                return;
            default: return;
        }
    }

    if (g_settings_view == SETTINGS_VIEW_EXPERT) {
        switch (g_settings_selected) {
            case SETTINGS_EXPERT_AUTONET:
                g_setting_driver_autostart = g_setting_driver_autostart == 0 ? 1 : 0;
                settings_set_notice("Netzwerk-Autostart umgeschaltet.", "Network auto-start switched.");
                settings_draw_list();
                return;
            case SETTINGS_EXPERT_DRIVER_DEBUG:
                g_setting_driver_debug = g_setting_driver_debug == 0 ? 1 : 0;
                settings_set_notice("Treiber-Diagnose umgeschaltet.", "Driver diagnostics switched.");
                settings_draw_list();
                return;
            case SETTINGS_EXPERT_AUTOSAVE:
                g_setting_autosave = g_setting_autosave == 0 ? 1 : 0;
                settings_set_notice("Auto-Speichern umgeschaltet.", "Auto-save switched.");
                settings_draw_list();
                return;
            case SETTINGS_EXPERT_VERBOSE_DIAG:
                g_setting_verbose_diag = g_setting_verbose_diag == 0 ? 1 : 0;
                settings_set_notice("Ausfuehrliche Diagnose umgeschaltet.", "Verbose diagnostics switched.");
                settings_draw_list();
                return;
            case SETTINGS_EXPERT_BACK:
                settings_go_home();
                return;
            default: return;
        }
    }
}

static void settings_open(void) {
    g_settings_active = 1;
    g_settings_view = SETTINGS_VIEW_HOME;
    g_settings_selected = 0;
    g_settings_edit_field = SETTINGS_EDIT_NONE;
    g_settings_input_len = 0U;
    g_settings_input[0] = '\0';
    g_settings_edit_title[0] = '\0';
    settings_set_notice("Bereit. Waehle einen Bereich aus.", "Ready. Pick a category.");
    settings_draw_list();
}

static void settings_handle_key(int key) {
    int count;
    if (g_settings_active == 0) {
        return;
    }
    if (g_settings_edit_field != SETTINGS_EDIT_NONE) {
        if (key == KEY_CTRL_C) {
            g_settings_edit_field = SETTINGS_EDIT_NONE;
            settings_set_notice("Aenderung verworfen.", "Change discarded.");
            settings_draw_list();
            return;
        }
        if (key == KEY_BACKSPACE) {
            if (g_settings_input_len > 0U) {
                g_settings_input_len--;
                g_settings_input[g_settings_input_len] = '\0';
            }
            settings_begin_text_edit(g_settings_edit_field, g_settings_edit_title, g_settings_input);
            return;
        }
        if (key == KEY_ENTER) {
            settings_apply_text_value();
            return;
        }
        if (key > 0 && key < 256 && g_settings_input_len + 1U < sizeof(g_settings_input)) {
            g_settings_input[g_settings_input_len++] = (char)key;
            g_settings_input[g_settings_input_len] = '\0';
            settings_begin_text_edit(g_settings_edit_field, g_settings_edit_title, g_settings_input);
        }
        return;
    }
    if (key == KEY_CTRL_C || key == 'q' || key == 'Q') {
        if (g_settings_view == SETTINGS_VIEW_HOME) {
            g_settings_active = 0;
            console_clear();
        } else {
            settings_go_home();
        }
        return;
    }
    count = settings_view_count(g_settings_view);
    if (key == KEY_UP) {
        g_settings_selected = g_settings_selected > 0 ? g_settings_selected - 1 : count - 1;
        settings_draw_list();
        return;
    }
    if (key == KEY_DOWN) {
        g_settings_selected = (g_settings_selected + 1) % count;
        settings_draw_list();
        return;
    }
    if (key == KEY_ENTER) {
        settings_activate_selected();
        return;
    }
}

static int needs_master(const char* action) {
    if (user_is_master() != 0) {
        return 0;
    }
    console_write(tr("Dafuer brauchst du System-Rechte. Nutze 'elevate <passwort>': ", "You need system rights for that. Use 'elevate <password>': "));
    console_writeln(action);
    return 1;
}

static void show_help(void) {
    console_writeln(tr("AuroraOS Hilfe", "AuroraOS help"));
    console_writeln(tr("Die wichtigsten Befehle:", "The most useful commands:"));
    console_writeln(tr("  help                 - Zeigt diese Hilfe.", "  help                 - Shows this help."));
    console_writeln(tr("  quickstart           - Einfache erste Schritte.", "  quickstart           - Simple first steps."));
    console_writeln(tr("  status               - Zeigt den aktuellen Zustand.", "  status               - Shows the current status."));
    console_writeln(tr("  apps                 - Zeigt die wichtigsten Teile des Systems.", "  apps                 - Shows the main parts of the system."));
    console_writeln(tr("  tasks                - Zeigt laufende und gestoppte Bereiche.", "  tasks                - Shows running and stopped areas."));
    console_writeln(tr("  memory               - Zeigt freien und belegten Speicher.", "  memory               - Shows free and used memory."));
    console_writeln(tr("  history              - Zeigt deine letzten Befehle.", "  history              - Shows your recent commands."));
    console_writeln(tr("  settings             - Oeffnet die neue Einstellungszentrale mit Kategorien und Expertenmodus.", "  settings             - Opens the new settings center with categories and expert mode."));
    console_writeln(tr("  whoami               - Zeigt Benutzername, Rolle und Home-Ordner.", "  whoami               - Shows user, role and home folder."));
    console_writeln(tr("  users                - Zeigt bekannte Benutzer, Gruppen und den System-Modus.", "  users                - Shows known users, groups and system mode."));
    console_writeln(tr("  login <n> [pw]       - Meldet einen normalen Benutzer an.", "  login <n> [pw]       - Signs in a regular user."));
    console_writeln(tr("  elevate <pw>         - Aktiviert den root-aehnlichen System-Modus.", "  elevate <pw>         - Activates the root-like system mode."));
    console_writeln(tr("  drop                 - Schaltet den System-Modus wieder aus.", "  drop                 - Turns system mode off again."));
    console_writeln(tr("  passwd <neu>         - Aendert dein Passwort.", "  passwd <new>         - Changes your password."));
    console_writeln(tr("  passwd <u> <neu>     - Aendert ein Passwort im System-Modus.", "  passwd <u> <new>     - Changes a password in system mode."));
    console_writeln(tr("  pwd                  - Zeigt deinen aktuellen Ort.", "  pwd                  - Shows your current location."));
    console_writeln(tr("  cd [pfad]            - Wechselt den Ordner. 'cd ~' geht nach Hause.", "  cd [path]            - Changes folder. 'cd ~' goes home."));
    console_writeln(tr("  ls [pfad]            - Zeigt Dateien und Ordner.", "  ls [path]            - Shows files and folders."));
    console_writeln(tr("  mkdir <name>         - Erstellt einen neuen Ordner.", "  mkdir <name>         - Creates a new folder."));
    console_writeln(tr("  touch <name>         - Erstellt eine leere Datei.", "  touch <name>         - Creates an empty file."));
    console_writeln(tr("  rm <pfad>            - Loescht eine Datei oder einen leeren Ordner.", "  rm <path>            - Deletes a file or an empty folder."));
    console_writeln(tr("  rm -r <pfad>         - Loescht einen Ordner mit Inhalt.", "  rm -r <path>         - Deletes a folder with its contents."));
    console_writeln(tr("  rmdir <pfad>         - Loescht einen leeren Ordner gezielt.", "  rmdir <path>         - Deletes an empty folder directly."));
    console_writeln(tr("  cat <datei>          - Zeigt den Inhalt einer Datei.", "  cat <file>           - Shows the content of a file."));
    console_writeln(tr("  write <d> <text>     - Schreibt Text in eine Datei.", "  write <f> <text>     - Writes text into a file."));
    console_writeln(tr("  append <d> <text>    - Haengt Text an eine Datei an.", "  append <f> <text>    - Appends text to a file."));
    console_writeln(tr("  edit <name>          - Oeffnet Lumen, den Texteditor.", "  edit <name>          - Opens Lumen, the text editor."));
    console_writeln(tr("  notes                - Zeigt offene Lumen-Dateien.", "  notes                - Shows open Lumen files."));
    console_writeln(tr("  fs                   - Erklaert AuroraFS kurz.", "  fs                   - Explains AuroraFS briefly."));
    console_writeln(tr("  stat <pfad>          - Zeigt Besitzer, Gruppe und Rechte eines Eintrags.", "  stat <path>          - Shows owner, group and rights of an entry."));
    console_writeln(tr("  protect <art> <p>    - Setzt Rechte einfach: private, team, public, shared.", "  protect <kind> <p>   - Sets rights simply: private, team, public, shared."));
    console_writeln(tr("  chmod <modus> <p>    - Linux-aehnliche Kurzform wie 600 oder 644.", "  chmod <mode> <p>     - Linux-like short form such as 600 or 644."));
    console_writeln(tr("  savefs               - Speichert Dateien, Nutzer, Netzwerk und Apps auf die virtuelle ATA-Platte.", "  savefs               - Saves files, users, network and apps to the virtual ATA disk."));
    console_writeln(tr("  loadfs               - Laedt Dateien, Nutzer, Netzwerk und Apps von der virtuellen ATA-Platte.", "  loadfs               - Reloads files, users, network and apps from the virtual ATA disk."));
    console_writeln(tr("  disk                 - Zeigt, ob eine Platte fuer Persistenz gefunden wurde.", "  disk                 - Shows whether a disk for persistence was found."));
    console_writeln(tr("  network              - Zeigt Netzwerk-Status und Konfiguration.", "  network              - Shows network status and configuration."));
    console_writeln(tr("  open settings        - Zweiter Weg in die Einstellungen.", "  open settings        - Second way to open settings."));
    console_writeln(tr("  network              - Zeigt Netzwerk-Status und aktive Werte.", "  network              - Shows network status and active values."));
    console_writeln(tr("  ip [set <addr>]      - Zeigt oder setzt die IPv4-Adresse (System-Modus).", "  ip [set <addr>]      - Shows or sets the IPv4 address (system mode)."));
    console_writeln(tr("  netprobe             - Sendet einen kleinen Rohdaten-Test.", "  netprobe             - Sends a small raw packet test."));
    console_writeln(tr("  ping <ziel>          - Testet Loopback sofort und zeigt den Treiberstatus.", "  ping <target>        - Tests loopback now and shows driver status."));
    console_writeln(tr("  nic                  - Zeigt erkannte PCI-Netzwerkadapter.", "  nic                  - Shows detected PCI network adapters."));
    console_writeln(tr("  netup                - Startet den e1000-Pilottreiber, falls die Karte passt.", "  netup                - Starts the e1000 pilot driver if the card fits."));
    console_writeln(tr("  netprobe             - Sendet einen kleinen Rohdaten-Test ueber e1000.", "  netprobe             - Sends a small raw packet test over e1000."));
    console_writeln(tr("  mac                  - Zeigt die erkannte MAC-Adresse des aktiven Treibers.", "  mac                  - Shows the detected MAC address of the active driver."));
    console_writeln(tr("  diag                 - Zeigt eine kurze Diagnose fuer System, Platte und Netzwerk.", "  diag                 - Shows a short diagnosis for system, disk and network."));
    console_writeln(tr("  app list             - Zeigt eingebaute und optionale Apps.", "  app list             - Shows built-in and optional apps."));
    console_writeln(tr("  app run <name>       - Startet eine App oder ihren Platzhalter.", "  app run <name>       - Starts an app or its placeholder."));
    console_writeln(tr("  app install <name>   - Installiert eine optionale App im System-Modus.", "  app install <name>   - Installs an optional app in system mode."));
    console_writeln(tr("  app remove <name>    - Entfernt eine optionale App im System-Modus.", "  app remove <name>    - Removes an optional app in system mode."));
    console_writeln(tr("  app info <name>      - Zeigt Titel, Status und Beschreibung einer App.", "  app info <name>      - Shows title, state and description of an app."));
    console_writeln(tr("  cmd list             - Zeigt eigene Befehle aus /apps/commands.", "  cmd list             - Shows custom commands from /apps/commands."));
    console_writeln(tr("  cmd add <n> <pfad>   - Registriert einen eigenen Befehl fuer ein Skript.", "  cmd add <n> <path>   - Registers your own command for a script."));
    console_writeln(tr("  cmd new <name>       - Erstellt Skript + Befehl in einem Schritt.", "  cmd new <name>       - Creates script + command in one step."));
    console_writeln(tr("  cmd show <name>      - Zeigt, auf welches Skript der Befehl zeigt.", "  cmd show <name>      - Shows which script the command points to."));
    console_writeln(tr("  cmd remove <name>    - Entfernt einen eigenen Befehl wieder.", "  cmd remove <name>    - Removes a custom command again."));
    console_writeln(tr("  prog list            - Zeigt externe Programme mit Manifesten.", "  prog list            - Shows external programs with manifests."));
    console_writeln(tr("  prog info <name>     - Zeigt Einstieg, Rechte, Vertrauen und Freigabe.", "  prog info <name>     - Shows entry, permissions, trust and approval."));
    console_writeln(tr("  prog caps <n> <r>    - Setzt Rechte wie fs-read, fs-write, network, apps.", "  prog caps <n> <r>    - Sets permissions such as fs-read, fs-write, network, apps."));
    console_writeln(tr("  prog trust <n> <t>   - Setzt local oder trusted.", "  prog trust <n> <t>   - Sets local or trusted."));
    console_writeln(tr("  prog approve <name>  - Gibt ein Programm zum Start frei.", "  prog approve <name>  - Approves a program for launch."));
    console_writeln(tr("  prog run <name>      - Startet ein externes Programm ueber sein Manifest.", "  prog run <name>      - Runs an external program through its manifest."));
    console_writeln(tr("  which <name>         - Zeigt, ob ein Name App, Shell-Befehl oder externes Programm ist.", "  which <name>         - Shows whether a name is an app, shell command or external program."));
    console_writeln(tr("  owner <u> <pfad>     - Aendert den Besitzer im System-Modus.", "  owner <u> <path>     - Changes the owner in system mode."));
    console_writeln(tr("  chown <u> <pfad>     - Alias fuer owner.", "  chown <u> <path>     - Alias for owner."));
    console_writeln(tr("  alloc <zahl>         - Reserviert testweise Speicher.", "  alloc <number>       - Reserves memory for testing."));
    console_writeln(tr("  start <name>         - Startet einen Bereich (System-Modus).", "  start <name>         - Starts an area (system mode)."));
    console_writeln(tr("  stop <name>          - Stoppt einen Bereich (System-Modus).", "  stop <name>          - Stops an area (system mode)."));
    console_writeln(tr("  reboot               - Startet AuroraOS neu.", "  reboot               - Restarts AuroraOS."));
    console_writeln(tr("  open <ziel>          - Oeffnet settings, desktop, network, files oder monitor.", "  open <target>        - Opens settings, desktop, network, files or monitor."));
    console_writeln(tr("  app run settings     - Startet die Settings-App.", "  app run settings     - Starts the settings app."));
    console_writeln(tr("  version              - Zeigt die Build-Version.", "  version              - Shows the build version."));
    console_writeln(tr("  about                - Erklaert kurz, was AuroraOS werden soll.", "  about                - Briefly explains what AuroraOS should become."));
    console_writeln(tr("  ai <text>            - Du kannst auch normal schreiben.", "  ai <text>            - You can also write naturally."));
    console_writeln(tr("  clear                - Leert den Bildschirm.", "  clear                - Clears the screen."));
    console_writeln(tr("  echo <text>          - Gibt Text wieder aus.", "  echo <text>          - Prints text back."));
    console_writeln("");
    console_writeln(tr("Mehr Uebersicht:", "More overview:"));
    console_writeln(tr("  Pfeil hoch / runter  - Springt durch alte Befehle.", "  Up / Down arrows     - Browse older commands."));
    console_writeln(tr("  Bild auf / ab        - Scrollt durch den Bildschirm-Verlauf.", "  Page Up / Down       - Scrolls through screen history."));
    console_writeln(tr("  Ctrl+C               - Bricht die aktuelle Eingabe ab.", "  Ctrl+C               - Cancels the current input."));
    console_writeln(tr("  In Lumen             - Ctrl+S speichert, Ctrl+Q beendet, .help zeigt Hilfe.", "  In Lumen             - Ctrl+S saves, Ctrl+Q quits, .help shows help."));
    console_writeln(tr("  Externe Programme    - Bekommen nur die Rechte aus ihrem Manifest.", "  External programs    - Only get the permissions from their manifest."));
}

static void show_about(void) {
    console_writeln(tr("AuroraOS ist ein eigenes, noch junges Betriebssystem.", "AuroraOS is its own young operating system."));
    console_writeln(tr("Die Idee: leicht zu bedienen wie Windows und anpassbar wie Linux.", "The idea: easy to use like Windows and customizable like Linux."));
    console_writeln(tr("Aktuell gibt es eine stabile Shell, Nutzer- und Rechte-Persistenz, AuroraFS mit cp/mv/find, erste Platten-Speicherung, PCI-Netzwerkerkennung, einen e1000-Pilottreiber, ein kleines App-Modell, eine groessere Settings-Zentrale mit Kategorien und Expertenmodus, eigene Skript-Befehle, sichere externe Programme mit Manifesten und Lumen.", "Right now it has a stable shell, persistent users and rights, AuroraFS with cp/mv/find, first disk persistence, PCI network detection, an e1000 pilot driver, a small app model, a larger categorized settings center with expert mode, custom script commands and Lumen."));
    console_writeln(tr("Lumen ist jetzt optisch an klassische Terminal-Editoren wie nano angelehnt. Die Settings-App erinnert an menueartige Kernel-Tools, ist aber bewusst leichter verstaendlich. Externe Programme laufen in einer einfachen Sandbox mit Rechten und Vertrauensstufen.", "Lumen is now visually inspired by classic terminal editors like nano."));
    console_writeln(tr("Der System-Modus ist root-aehnlich: kein normaler Nutzer, sondern eine Rechte-Erweiterung.", "System mode is root-like: not a normal user, but a privilege elevation."));
    console_writeln(tr("Programmiert von Obsidian.", "Programmiert von Obsidian."));
}

static void open_target(const char* target) {
    if (kstrcmp(target, "settings") == 0) {
        settings_open();
        return;
    }
    if (kstrcmp(target, "desktop") == 0) {
        console_writeln(tr("Desktop-Platzhalter geoeffnet.", "Desktop placeholder opened."));
        return;
    }
    if (kstrcmp(target, "network") == 0) {
        print_network_status();
        return;
    }
    if (kstrcmp(target, "files") == 0) {
        afs_ls("");
        return;
    }
    if (kstrcmp(target, "monitor") == 0) {
        print_status();
        return;
    }
    console_writeln(tr("Nutze: open <settings|desktop|network|files|monitor>", "Use: open <settings|desktop|network|files|monitor>"));
}

static void history_store(const char* cmd) {
    if (kstrlen(cmd) == 0U) {
        return;
    }
    if (history_count > 0U && kstrcmp(history[history_count - 1U], cmd) == 0) {
        return;
    }
    if (history_count < HISTORY_MAX) {
        kstrcpy(history[history_count], cmd);
        history_count++;
        return;
    }
    for (size_t i = 1; i < HISTORY_MAX; ++i) {
        kstrcpy(history[i - 1U], history[i]);
    }
    kstrcpy(history[HISTORY_MAX - 1U], cmd);
}

static void replace_input_line(const char* text) {
    while (input_len > 0U) {
        input_len--;
        input_buffer[input_len] = '\0';
        console_backspace();
    }

    for (size_t i = 0U; text[i] != '\0' && i < INPUT_MAX - 1U; ++i) {
        input_buffer[i] = text[i];
        console_putc(text[i]);
        input_len = i + 1U;
    }
    input_buffer[input_len] = '\0';
}

static void history_up(void) {
    if (history_count == 0U) {
        return;
    }
    if (history_view < 0) {
        kstrcpy(history_draft, input_buffer);
        history_view = (int)history_count - 1;
    } else if (history_view > 0) {
        history_view--;
    }
    replace_input_line(history[history_view]);
}

static void history_down(void) {
    if (history_view < 0) {
        return;
    }
    if ((size_t)history_view < history_count - 1U) {
        history_view++;
        replace_input_line(history[history_view]);
        return;
    }
    history_view = -1;
    replace_input_line(history_draft);
}

static void shell_cancel_input(void) {
    input_len = 0U;
    input_buffer[0] = '\0';
    history_draft[0] = '\0';
    history_view = -1;
    console_writeln("^C");
    prompt();
}

static void shell_reboot(void) {
    console_writeln(tr("Starte neu ...", "Rebooting ..."));
    while ((inb(0x64) & 0x02U) != 0U) {
    }
    outb(0x64, 0xFEU);
    for (;;) {
        __asm__ volatile ("pause");
    }
}

static void print_file_contents(const char* path) {
    char buffer[1024];
    int rc = afs_read_file(path, buffer, sizeof(buffer));
    if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Zugriff verweigert.", "Access denied."));
        return;
    }
    if (rc < 0) {
        console_writeln(tr("Datei nicht gefunden oder zu gross.", "File not found or too large."));
        return;
    }
    if (buffer[0] == '\0') {
        console_writeln(tr("(leer)", "(empty)"));
        return;
    }
    console_writeln(buffer);
}

static void print_entry_status(const char* path) {
    char buffer[256];
    int rc = afs_stat(path, buffer, sizeof(buffer));
    if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Zugriff verweigert.", "Access denied."));
        return;
    }
    if (rc < 0) {
        console_writeln(tr("Eintrag nicht gefunden.", "Entry not found."));
        return;
    }
    console_writeln(buffer);
}

static int split_name_and_text(const char* input, char* name, size_t name_max, const char** text_out) {
    size_t i = 0U;
    while (*input == ' ') {
        input++;
    }
    while (*input != '\0' && *input != ' ' && i + 1U < name_max) {
        name[i++] = *input++;
    }
    name[i] = '\0';
    while (*input == ' ') {
        input++;
    }
    *text_out = input;
    return name[0] != '\0' && input[0] != '\0' ? 0 : -1;
}


static int split_first_arg(const char* input, char* out, size_t max) {
    size_t i = 0U;
    while (*input == ' ') {
        input++;
    }
    while (*input != '\0' && *input != ' ' && i + 1U < max) {
        out[i++] = *input++;
    }
    out[i] = '\0';
    return out[0] != '\0' ? 0 : -1;
}

static int split_two_args(const char* input, char* a, size_t amax, char* b, size_t bmax) {
    size_t i = 0U;
    size_t j = 0U;
    while (*input == ' ') {
        input++;
    }
    while (*input != '\0' && *input != ' ' && i + 1U < amax) {
        a[i++] = *input++;
    }
    a[i] = '\0';
    while (*input == ' ') {
        input++;
    }
    while (*input != '\0' && *input != ' ' && j + 1U < bmax) {
        b[j++] = *input++;
    }
    b[j] = '\0';
    return (a[0] != '\0' && b[0] != '\0') ? 0 : -1;
}

static void trim_line_end(char* text) {
    size_t len = kstrlen(text);
    while (len > 0U && (text[len - 1U] == '\n' || text[len - 1U] == '\r' || text[len - 1U] == ' ' || text[len - 1U] == '\t')) {
        text[len - 1U] = '\0';
        len--;
    }
}

static void append_limited_local(char* dst, size_t max, const char* src) {
    size_t pos = kstrlen(dst);
    size_t i = 0U;
    if (pos >= max) {
        return;
    }
    while (src[i] != '\0' && pos + 1U < max) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static void program_context_enter(const char* name, unsigned int caps, const char* trust) {
    g_program_active = 1;
    g_program_caps = caps;
    g_program_name[0] = '\0';
    g_program_trust[0] = '\0';
    if (name != (const char*)0) {
        kstrcpy(g_program_name, name);
    }
    if (trust != (const char*)0 && trust[0] != '\0') {
        kstrcpy(g_program_trust, trust);
    } else {
        kstrcpy(g_program_trust, "local");
    }
}

static void program_context_leave(void) {
    g_program_active = 0;
    g_program_caps = 0U;
    g_program_name[0] = '\0';
    g_program_trust[0] = '\0';
}

static int program_has_cap(unsigned int cap) {
    if (g_program_active == 0) {
        return 1;
    }
    return (g_program_caps & cap) != 0U ? 1 : 0;
}

static void program_deny(const char* de, const char* en) {
    console_write(tr("[schutz] Programm blockiert: ", "[security] Program blocked: "));
    if (g_program_name[0] != '\0') {
        console_write(g_program_name);
        console_write(" - ");
    }
    console_writeln(tr(de, en));
}

static void resolve_shell_path(const char* path, char* out, size_t max) {
    char cwd[96];
    if (out == (char*)0 || max == 0U) {
        return;
    }
    out[0] = '\0';
    if (path == (const char*)0 || path[0] == '\0') {
        return;
    }
    if (path[0] == '~') {
        append_limited_local(out, max, user_current()->home);
        if (path[1] == '/' && path[2] != '\0') {
            append_limited_local(out, max, path + 1);
        }
        return;
    }
    if (path[0] == '/') {
        append_limited_local(out, max, path);
        return;
    }
    afs_pwd(cwd, sizeof(cwd));
    append_limited_local(out, max, cwd);
    if (kstrlen(out) > 0U && out[kstrlen(out) - 1U] != '/') {
        append_limited_local(out, max, "/");
    }
    append_limited_local(out, max, path);
}

static int program_safe_write_path(const char* path) {
    char resolved[128];
    size_t home_len;
    if (g_program_active == 0 || program_has_cap(EXTPROG_CAP_SYSTEM) != 0) {
        return 1;
    }
    resolve_shell_path(path, resolved, sizeof(resolved));
    home_len = kstrlen(user_current()->home);
    if (kstrncmp(resolved, user_current()->home, home_len) == 0 && (resolved[home_len] == '\0' || resolved[home_len] == '/')) {
        return 1;
    }
    if (kstrncmp(resolved, "/temp", 5U) == 0 && (resolved[5] == '\0' || resolved[5] == '/')) {
        return 1;
    }
    return 0;
}

static int program_guard_path_write(const char* path) {
    if (g_program_active == 0) {
        return 1;
    }
    if (program_has_cap(EXTPROG_CAP_FS_WRITE) == 0) {
        program_deny("dieses Programm hat keine Schreibrechte.", "this program has no write permission.");
        return 0;
    }
    if (program_safe_write_path(path) == 0) {
        program_deny("Schreiben ist nur im eigenen Home oder in /temp erlaubt.", "writes are only allowed in the user's home or in /temp.");
        return 0;
    }
    return 1;
}

static int program_guard_path_read(const char* path) {
    (void)path;
    if (g_program_active == 0) {
        return 1;
    }
    if (program_has_cap(EXTPROG_CAP_FS_READ) == 0) {
        program_deny("dieses Programm hat keine Leserechte fuer Dateien.", "this program has no file read permission.");
        return 0;
    }
    return 1;
}

static int program_guard_command(const char* cmd) {
    char first[24];
    char second[32];
    if (g_program_active == 0) {
        return 1;
    }
    if (split_first_arg(cmd, first, sizeof(first)) != 0) {
        return 1;
    }
    if (kstrcmp(first, "help") == 0 || kstrcmp(first, "quickstart") == 0 || kstrcmp(first, "status") == 0 ||
        kstrcmp(first, "history") == 0 || kstrcmp(first, "whoami") == 0 || kstrcmp(first, "users") == 0 ||
        kstrcmp(first, "version") == 0 || kstrcmp(first, "about") == 0 || kstrcmp(first, "diag") == 0 ||
        kstrcmp(first, "settings") == 0 ||
        kstrcmp(first, "pwd") == 0 || kstrcmp(first, "fs") == 0 || kstrcmp(first, "network") == 0 ||
        kstrcmp(first, "nic") == 0 || kstrcmp(first, "mac") == 0 || kstrcmp(first, "echo") == 0 ||
        kstrcmp(first, "clear") == 0 || kstrcmp(first, "ai") == 0 || kstrcmp(first, "which") == 0) {
        return 1;
    }
    if (kstrcmp(first, "ls") == 0 || kstrcmp(first, "cat") == 0 || kstrcmp(first, "find") == 0 || kstrcmp(first, "stat") == 0) {
        return program_guard_path_read("");
    }
    if (kstrcmp(first, "mkdir") == 0 || kstrcmp(first, "touch") == 0 || kstrcmp(first, "edit") == 0 || kstrcmp(first, "rmdir") == 0) {
        return program_guard_path_write(cmd + kstrlen(first) + 1U);
    }
    if (kstrcmp(first, "rm") == 0 || kstrcmp(first, "delete") == 0) {
        const char* arg = cmd + (kstrcmp(first, "rm") == 0 ? 2 : 6);
        while (*arg == ' ') { arg++; }
        if (kstarts_with(arg, "-r ") != 0) { arg += 3; }
        else if (kstarts_with(arg, "-rf ") != 0) { arg += 4; }
        while (*arg == ' ') { arg++; }
        return program_guard_path_write(arg);
    }
    if (kstrcmp(first, "write") == 0 || kstrcmp(first, "append") == 0) {
        char name[64];
        const char* text = (const char*)0;
        if (split_name_and_text(cmd + kstrlen(first) + 1U, name, sizeof(name), &text) == 0) {
            return program_guard_path_write(name);
        }
        return 1;
    }
    if (kstrcmp(first, "cp") == 0 || kstrcmp(first, "copy") == 0 || kstrcmp(first, "mv") == 0 || kstrcmp(first, "move") == 0) {
        char a[64];
        char b[64];
        const char* args = cmd + kstrlen(first) + 1U;
        while (*args == ' ') { args++; }
        if (kstarts_with(args, "-r ") != 0) { args += 3; }
        if (split_two_args(args, a, sizeof(a), b, sizeof(b)) == 0) {
            if (program_guard_path_read(a) == 0) { return 0; }
            return program_guard_path_write(b);
        }
        return 1;
    }
    if (kstrcmp(first, "app") == 0) {
        if (split_first_arg(cmd + 4, second, sizeof(second)) == 0) {
            if (kstrcmp(second, "list") == 0 || kstrcmp(second, "info") == 0) {
                return 1;
            }
            if (kstrcmp(second, "run") == 0) {
                if (program_has_cap(EXTPROG_CAP_APPS) != 0) { return 1; }
                program_deny("dieses Programm darf keine Apps starten.", "this program may not launch apps.");
                return 0;
            }
        }
        program_deny("dieses Programm darf keine Apps installieren oder entfernen.", "this program may not install or remove apps.");
        return 0;
    }
    if (kstrcmp(first, "ping") == 0) {
        char target[64];
        if (split_first_arg(cmd + 5, target, sizeof(target)) == 0 && kstrcmp(target, "127.0.0.1") == 0) {
            return 1;
        }
        if (program_has_cap(EXTPROG_CAP_NETWORK) != 0) {
            return 1;
        }
        program_deny("dieses Programm darf das Netzwerk nicht benutzen.", "this program may not use the network.");
        return 0;
    }
    if (kstrcmp(first, "netup") == 0 || kstrcmp(first, "netprobe") == 0 || kstrcmp(first, "netscan") == 0) {
        if (program_has_cap(EXTPROG_CAP_NETWORK) != 0) {
            return 1;
        }
        program_deny("dieses Programm darf keine Netzwerkaktionen ausloesen.", "this program may not trigger network actions.");
        return 0;
    }
    if (kstrcmp(first, "savefs") == 0 || kstrcmp(first, "loadfs") == 0 || kstrcmp(first, "disk") == 0 ||
        kstrcmp(first, "protect") == 0 || kstrcmp(first, "chmod") == 0 || kstrcmp(first, "owner") == 0 || kstrcmp(first, "chown") == 0 ||
        kstrcmp(first, "login") == 0 || kstrcmp(first, "passwd") == 0 || kstrcmp(first, "elevate") == 0 || kstrcmp(first, "drop") == 0 ||
        kstrcmp(first, "start") == 0 || kstrcmp(first, "stop") == 0 || kstrcmp(first, "reboot") == 0 || kstrcmp(first, "restart") == 0 ||
        kstrcmp(first, "cmd") == 0 || kstrcmp(first, "prog") == 0 || kstrcmp(first, "alloc") == 0) {
        if (program_has_cap(EXTPROG_CAP_SYSTEM) != 0) {
            return 1;
        }
        program_deny("dieses Programm darf keine System-Befehle ausfuehren.", "this program may not execute system commands.");
        return 0;
    }
    return 1;
}

static int command_name_valid(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return 0;
    }
    for (i = 0U; name[i] != '\0'; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

static void build_command_wrapper_path(const char* name, char* out, size_t max) {
    out[0] = '\0';
    append_limited_local(out, max, "/apps/commands/");
    append_limited_local(out, max, name);
    append_limited_local(out, max, ".cmd");
}

static int read_custom_command_script(const char* name, char* script_path, size_t max) {
    char wrapper_path[96];
    int rc;
    if (command_name_valid(name) == 0) {
        return 0;
    }
    build_command_wrapper_path(name, wrapper_path, sizeof(wrapper_path));
    rc = afs_read_file(wrapper_path, script_path, max);
    if (rc < 0) {
        return 0;
    }
    trim_line_end(script_path);
    return script_path[0] != '\0' ? 1 : 0;
}

static void copy_nth_arg(const char* args, int index, char* out, size_t max) {
    int current = 1;
    size_t pos = 0U;
    while (*args == ' ') {
        args++;
    }
    while (*args != '\0') {
        if (current == index) {
            while (*args != '\0' && *args != ' ' && pos + 1U < max) {
                out[pos++] = *args++;
            }
            break;
        }
        while (*args != '\0' && *args != ' ') {
            args++;
        }
        while (*args == ' ') {
            args++;
        }
        current++;
    }
    out[pos] = '\0';
}

static void expand_script_line(const char* line, const char* args, char* out, size_t max) {
    size_t pos = 0U;
    size_t i;
    out[0] = '\0';
    for (i = 0U; line[i] != '\0' && pos + 1U < max; ++i) {
        if (line[i] == '$' && line[i + 1U] == '@') {
            size_t j = 0U;
            while (args[j] != '\0' && pos + 1U < max) {
                out[pos++] = args[j++];
            }
            i++;
            continue;
        }
        if (line[i] == '$' && line[i + 1U] >= '1' && line[i + 1U] <= '9') {
            char value[64];
            size_t j = 0U;
            copy_nth_arg(args, (int)(line[i + 1U] - '0'), value, sizeof(value));
            while (value[j] != '\0' && pos + 1U < max) {
                out[pos++] = value[j++];
            }
            i++;
            continue;
        }
        out[pos++] = line[i];
    }
    out[pos] = '\0';
}

static void execute_script_file(const char* script_path, const char* args) {
    static int script_depth = 0;
    char script_text[1024];
    size_t i;
    size_t line_start = 0U;
    int rc;
    if (script_depth >= 6) {
        console_writeln(tr("Skript-Abbruch: zu viele verschachtelte Aufrufe.", "Script stopped: too many nested calls."));
        return;
    }
    rc = afs_read_file(script_path, script_text, sizeof(script_text));
    if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Skript-Zugriff verweigert.", "Script access denied."));
        return;
    }
    if (rc < 0) {
        console_writeln(tr("Skript nicht gefunden.", "Script not found."));
        return;
    }
    script_depth++;
    for (i = 0U;; ++i) {
        char ch = script_text[i];
        if (ch == '\r') {
            script_text[i] = '\n';
            ch = '\n';
        }
        if (ch == '\n' || ch == '\0') {
            char expanded[INPUT_MAX];
            script_text[i] = '\0';
            {
                char* line = &script_text[line_start];
                while (*line == ' ' || *line == '\t') {
                    line++;
                }
                if (*line != '\0' && *line != '#') {
                    expand_script_line(line, args, expanded, sizeof(expanded));
                    trim_line_end(expanded);
                    if (expanded[0] != '\0') {
                        run_command(expanded);
                    }
                }
            }
            if (ch == '\0') {
                break;
            }
            line_start = i + 1U;
        }
    }
    script_depth--;
}

static int try_custom_command(const char* cmd) {
    char name[32];
    char script_path[128];
    extprog_manifest_t manifest;
    const char* args = cmd;
    size_t i = 0U;
    while (*args == ' ') {
        args++;
    }
    while (*args != '\0' && *args != ' ' && i + 1U < sizeof(name)) {
        name[i++] = *args++;
    }
    name[i] = '\0';
    while (*args == ' ') {
        args++;
    }
    if (read_custom_command_script(name, script_path, sizeof(script_path)) == 0) {
        return 0;
    }
    if (extprog_load(name, &manifest) == 0) {
        if (g_setting_require_program_approval != 0 && manifest.approved == 0U) {
            console_writeln(tr("Programm ist noch nicht freigegeben. Nutze 'prog approve <name>' oder lockere die Regel in Settings > Sicherheit.", "Program is not approved yet. Use 'prog approve <name>' or relax the rule in Settings > Security."));
            return 1;
        }
        program_context_enter(name, manifest.caps, manifest.trust);
        execute_script_file(manifest.entry[0] != '\0' ? manifest.entry : script_path, args);
        program_context_leave();
        return 1;
    }
    if (g_setting_allow_legacy_commands == 0) {
        console_writeln(tr("Legacy-Befehle ohne Manifest sind gesperrt. Nutze Settings > Sicherheit.", "Legacy commands without a manifest are blocked. Use Settings > Security."));
        return 1;
    }
    program_context_enter(name, EXTPROG_CAP_FS_READ, "legacy");
    execute_script_file(script_path, args);
    program_context_leave();
    return 1;
}

static void print_program_info(const char* name) {
    extprog_manifest_t manifest;
    char caps[96];
    if (extprog_load(name, &manifest) != 0) {
        console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
        return;
    }
    extprog_caps_to_text(manifest.caps, caps, sizeof(caps));
    console_write(tr("Programm: ", "Program: "));
    console_writeln(name);
    console_write(tr("  Einstieg: ", "  Entry: "));
    console_writeln(manifest.entry);
    console_write(tr("  Vertrauen: ", "  Trust: "));
    console_writeln(manifest.trust);
    console_write(tr("  Rechte: ", "  Permissions: "));
    console_writeln(caps);
    console_write(tr("  Freigabe: ", "  Approval: "));
    console_writeln(manifest.approved != 0U ? tr("ja", "yes") : tr("nein", "no"));
    console_write(tr("  Besitzer: ", "  Owner: "));
    console_writeln(manifest.owner);
}

static int can_manage_program(const extprog_manifest_t* manifest) {
    if (manifest == (const extprog_manifest_t*)0) {
        return 0;
    }
    if (user_is_master() != 0) {
        return 1;
    }
    return kstrcmp(manifest->owner, user_current()->username) == 0 ? 1 : 0;
}

static void command_cmd(const char* args) {
    char action[16];
    char name[32];
    char value[96];
    char wrapper_path[96];
    char script_path[96];
    if (split_first_arg(args, action, sizeof(action)) != 0) {
        console_writeln(tr("Nutze: cmd <list|add|new|show|remove|run> ...", "Use: cmd <list|add|new|show|remove|run> ..."));
        return;
    }
    if (kstrcmp(action, "list") == 0) {
        afs_ls("/apps/commands");
        return;
    }
    if (kstrcmp(action, "show") == 0) {
        if (split_first_arg(args + 4, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: cmd show <name>", "Use: cmd show <name>"));
            return;
        }
        if (read_custom_command_script(name, value, sizeof(value)) == 0) {
            console_writeln(tr("Diesen eigenen Befehl kenne ich nicht.", "I do not know that custom command."));
            return;
        }
        console_write(tr("Befehl ", "Command "));
        console_write(name);
        console_write(tr(" zeigt auf ", " points to "));
        console_writeln(value);
        if (extprog_load(name, &(extprog_manifest_t){0}) == 0) {
            print_program_info(name);
        }
        return;
    }
    if (kstrcmp(action, "run") == 0) {
        const char* rest = args + 3;
        extprog_manifest_t manifest;
        while (*rest == ' ') { rest++; }
        while (*rest != '\0' && *rest != ' ') { rest++; }
        while (*rest == ' ') { rest++; }
        if (split_first_arg(args + 3, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: cmd run <name> [args]", "Use: cmd run <name> [args]"));
            return;
        }
        if (read_custom_command_script(name, value, sizeof(value)) == 0) {
            console_writeln(tr("Diesen eigenen Befehl kenne ich nicht.", "I do not know that custom command."));
            return;
        }
        if (extprog_load(name, &manifest) == 0) {
            if (g_setting_require_program_approval != 0 && manifest.approved == 0U) {
                console_writeln(tr("Programm ist noch nicht freigegeben. Nutze 'prog approve <name>' oder lockere die Regel in Settings > Sicherheit.", "Program is not approved yet. Use 'prog approve <name>' or relax the rule in Settings > Security."));
                return;
            }
            program_context_enter(name, manifest.caps, manifest.trust);
            execute_script_file(manifest.entry[0] != '\0' ? manifest.entry : value, rest);
            program_context_leave();
            return;
        }
        if (g_setting_allow_legacy_commands == 0) {
            console_writeln(tr("Legacy-Befehle ohne Manifest sind gesperrt. Nutze Settings > Sicherheit.", "Legacy commands without a manifest are blocked. Use Settings > Security."));
            return;
        }
        program_context_enter(name, EXTPROG_CAP_FS_READ, "legacy");
        execute_script_file(value, rest);
        program_context_leave();
        return;
    }
    if (kstrcmp(action, "add") == 0) {
        if (split_two_args(args + 4, name, sizeof(name), value, sizeof(value)) != 0 || command_name_valid(name) == 0) {
            console_writeln(tr("Nutze: cmd add <name> <skriptpfad>", "Use: cmd add <name> <script-path>"));
            return;
        }
        if (afs_exists(value) == 0) {
            console_writeln(tr("Skript-Datei nicht gefunden.", "Script file not found."));
            return;
        }
        (void)afs_mkdir("/apps/commands");
        (void)afs_mkdir("/apps/programs");
        build_command_wrapper_path(name, wrapper_path, sizeof(wrapper_path));
        if (afs_write_file(wrapper_path, value) != AFS_OK) {
            console_writeln(tr("Befehl konnte nicht registriert werden.", "Could not register command."));
            return;
        }
        if (extprog_register(name, value, EXTPROG_CAP_FS_READ | EXTPROG_CAP_FS_WRITE, "local", g_setting_default_autoapprove) != AFS_OK) {
            console_writeln(tr("Manifest konnte nicht erstellt werden.", "Could not create manifest."));
            return;
        }
        console_writeln(tr("Eigener Befehl registriert. Standard-Rechte: fs-read, fs-write (nur Home und /temp).", "Custom command registered. Default rights: fs-read, fs-write (home and /temp only)."));
        return;
    }
    if (kstrcmp(action, "new") == 0) {
        if (split_first_arg(args + 4, name, sizeof(name)) != 0 || command_name_valid(name) == 0) {
            console_writeln(tr("Nutze: cmd new <name>", "Use: cmd new <name>"));
            return;
        }
        script_path[0] = '\0';
        append_limited_local(script_path, sizeof(script_path), "~/");
        append_limited_local(script_path, sizeof(script_path), name);
        append_limited_local(script_path, sizeof(script_path), ".aos");
        if (afs_write_file(script_path, "# AuroraOS command script\necho Hello from AuroraOS\n") != AFS_OK) {
            console_writeln(tr("Skript konnte nicht erstellt werden.", "Could not create script."));
            return;
        }
        (void)afs_mkdir("/apps/commands");
        build_command_wrapper_path(name, wrapper_path, sizeof(wrapper_path));
        if (afs_write_file(wrapper_path, script_path) != AFS_OK) {
            console_writeln(tr("Wrapper konnte nicht erstellt werden.", "Could not create wrapper."));
            return;
        }
        if (extprog_register(name, script_path, EXTPROG_CAP_FS_READ | EXTPROG_CAP_FS_WRITE, "local", g_setting_default_autoapprove) != AFS_OK) {
            console_writeln(tr("Manifest konnte nicht erstellt werden.", "Could not create manifest."));
            return;
        }
        console_write(tr("Neuer Befehl erstellt: ", "New command created: "));
        console_write(name);
        console_write(" -> ");
        console_writeln(script_path);
        console_writeln(tr("Sicherheit: Das Skript darf standardmaessig lesen und im Home oder in /temp schreiben.", "Security: By default the script may read and write inside home or /temp."));
        return;
    }
    if (kstrcmp(action, "remove") == 0) {
        if (split_first_arg(args + 7, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: cmd remove <name>", "Use: cmd remove <name>"));
            return;
        }
        build_command_wrapper_path(name, wrapper_path, sizeof(wrapper_path));
        (void)extprog_remove(name);
        if (afs_rm(wrapper_path) == AFS_OK) {
            console_writeln(tr("Eigener Befehl entfernt.", "Custom command removed."));
        } else {
            console_writeln(tr("Befehl konnte nicht entfernt werden.", "Could not remove command."));
        }
        return;
    }
    console_writeln(tr("Nutze: cmd <list|add|new|show|remove|run>", "Use: cmd <list|add|new|show|remove|run>"));
}

static void command_prog(const char* args) {
    char action[16];
    char name[32];
    char value[96];
    extprog_manifest_t manifest;
    if (split_first_arg(args, action, sizeof(action)) != 0) {
        console_writeln(tr("Nutze: prog <list|info|caps|trust|approve|run|remove> ...", "Use: prog <list|info|caps|trust|approve|run|remove> ..."));
        return;
    }
    if (kstrcmp(action, "list") == 0) {
        afs_ls("/apps/programs");
        return;
    }
    if (kstrcmp(action, "info") == 0) {
        if (split_first_arg(args + 4, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: prog info <name>", "Use: prog info <name>"));
            return;
        }
        print_program_info(name);
        return;
    }
    if (kstrcmp(action, "run") == 0) {
        const char* rest = args + 3;
        while (*rest == ' ') { rest++; }
        while (*rest != '\0' && *rest != ' ') { rest++; }
        while (*rest == ' ') { rest++; }
        if (split_first_arg(args + 3, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: prog run <name> [args]", "Use: prog run <name> [args]"));
            return;
        }
        if (extprog_load(name, &manifest) != 0) {
            console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
            return;
        }
        if (g_setting_require_program_approval != 0 && manifest.approved == 0U) {
            console_writeln(tr("Programm ist noch nicht freigegeben. Nutze 'prog approve <name>' oder lockere die Regel in Settings > Sicherheit.", "Program is not approved yet. Use 'prog approve <name>' or relax the rule in Settings > Security."));
            return;
        }
        program_context_enter(name, manifest.caps, manifest.trust);
        execute_script_file(manifest.entry, rest);
        program_context_leave();
        return;
    }
    if (kstrcmp(action, "caps") == 0) {
        unsigned int caps;
        if (split_two_args(args + 5, name, sizeof(name), value, sizeof(value)) != 0) {
            console_writeln(tr("Nutze: prog caps <name> <rechte>", "Use: prog caps <name> <permissions>"));
            return;
        }
        if (extprog_load(name, &manifest) != 0) {
            console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
            return;
        }
        if (can_manage_program(&manifest) == 0) {
            console_writeln(tr("Nur Besitzer oder System-Modus duerfen Programme aendern.", "Only the owner or system mode may modify programs."));
            return;
        }
        caps = extprog_caps_from_text(value);
        if ((caps & EXTPROG_CAP_SYSTEM) != 0U && user_is_master() == 0) {
            console_writeln(tr("System-Rechte fuer Programme darf nur der System-Modus vergeben.", "Only system mode may grant system permissions to programs."));
            return;
        }
        if ((caps & EXTPROG_CAP_NETWORK) != 0U && user_is_master() == 0) {
            console_writeln(tr("Netzwerk-Rechte fuer Programme vergibt AuroraOS nur im System-Modus.", "AuroraOS only grants network permissions to programs in system mode."));
            return;
        }
        if (extprog_set_caps(name, caps) == 0) {
            console_writeln(tr("Programm-Rechte aktualisiert.", "Program permissions updated."));
        } else {
            console_writeln(tr("Programm-Rechte konnten nicht aktualisiert werden.", "Could not update program permissions."));
        }
        return;
    }
    if (kstrcmp(action, "trust") == 0) {
        if (split_two_args(args + 6, name, sizeof(name), value, sizeof(value)) != 0) {
            console_writeln(tr("Nutze: prog trust <name> <local|trusted>", "Use: prog trust <name> <local|trusted>"));
            return;
        }
        if (extprog_load(name, &manifest) != 0) {
            console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
            return;
        }
        if (can_manage_program(&manifest) == 0) {
            console_writeln(tr("Nur Besitzer oder System-Modus duerfen Programme aendern.", "Only the owner or system mode may modify programs."));
            return;
        }
        if (kstrcmp(value, "trusted") == 0 && user_is_master() == 0) {
            console_writeln(tr("'trusted' darf nur der System-Modus setzen.", "Only system mode may set 'trusted'."));
            return;
        }
        if (extprog_set_trust(name, value) == 0) {
            console_writeln(tr("Vertrauensstufe aktualisiert.", "Trust level updated."));
        } else {
            console_writeln(tr("Vertrauensstufe konnte nicht aktualisiert werden.", "Could not update trust level."));
        }
        return;
    }
    if (kstrcmp(action, "approve") == 0) {
        if (split_first_arg(args + 8, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: prog approve <name>", "Use: prog approve <name>"));
            return;
        }
        if (extprog_load(name, &manifest) != 0) {
            console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
            return;
        }
        if (can_manage_program(&manifest) == 0) {
            console_writeln(tr("Nur Besitzer oder System-Modus duerfen Programme freigeben.", "Only the owner or system mode may approve programs."));
            return;
        }
        if (extprog_set_approved(name, 1) == 0) {
            console_writeln(tr("Programm freigegeben.", "Program approved."));
        } else {
            console_writeln(tr("Programm konnte nicht freigegeben werden.", "Could not approve program."));
        }
        return;
    }
    if (kstrcmp(action, "remove") == 0) {
        if (split_first_arg(args + 7, name, sizeof(name)) != 0) {
            console_writeln(tr("Nutze: prog remove <name>", "Use: prog remove <name>"));
            return;
        }
        if (extprog_load(name, &manifest) != 0) {
            console_writeln(tr("Dieses Programm kenne ich nicht.", "I do not know that program."));
            return;
        }
        if (can_manage_program(&manifest) == 0) {
            console_writeln(tr("Nur Besitzer oder System-Modus duerfen Programme entfernen.", "Only the owner or system mode may remove programs."));
            return;
        }
        if (extprog_remove(name) == AFS_OK) {
            console_writeln(tr("Programm-Manifest entfernt.", "Program manifest removed."));
        } else {
            console_writeln(tr("Programm-Manifest konnte nicht entfernt werden.", "Could not remove program manifest."));
        }
        return;
    }
    console_writeln(tr("Nutze: prog <list|info|caps|trust|approve|run|remove>", "Use: prog <list|info|caps|trust|approve|run|remove>"));
}

static void command_which(const char* args) {
    char name[32];
    char path[96];
    if (split_first_arg(args, name, sizeof(name)) != 0) {
        console_writeln(tr("Nutze: which <name>", "Use: which <name>"));
        return;
    }
    if (app_find(name) != (const app_t*)0) {
        console_write(tr("App: ", "App: "));
        console_writeln(name);
        return;
    }
    if (extprog_manifest_path(name, path, sizeof(path)) == 0 && afs_exists(path) != 0) {
        console_write(tr("Externes Programm: ", "External program: "));
        console_writeln(path);
        return;
    }
    build_command_wrapper_path(name, path, sizeof(path));
    if (afs_exists(path) != 0) {
        console_write(tr("Eigener Befehl: ", "Custom command: "));
        console_writeln(path);
        return;
    }
    console_writeln(tr("Nicht gefunden oder interner Shell-Befehl.", "Not found or internal shell command."));
}

static void command_cp(const char* args) {
    char left[64];
    char right[64];
    int recursive = 0;
    while (*args == ' ') { args++; }
    if (kstarts_with(args, "-r ") != 0) {
        recursive = 1;
        args += 3;
    }
    if (split_two_args(args, left, sizeof(left), right, sizeof(right)) != 0) {
        console_writeln(tr("Nutze: cp <quelle> <ziel> oder cp -r <quelle> <ziel>", "Use: cp <source> <target> or cp -r <source> <target>"));
        return;
    }
    print_afs_result(afs_copy(left, right, recursive), "Kopie erstellt.", "Copy created.", "Kopieren fehlgeschlagen.", "Copy failed.");
}

static void command_mv(const char* args) {
    char left[64];
    char right[64];
    if (split_two_args(args, left, sizeof(left), right, sizeof(right)) != 0) {
        console_writeln(tr("Nutze: mv <quelle> <ziel>", "Use: mv <source> <target>"));
        return;
    }
    print_afs_result(afs_move(left, right), "Eintrag verschoben.", "Entry moved.", "Verschieben fehlgeschlagen.", "Move failed.");
}

static void command_find(const char* args) {
    char first[64];
    char second[64];
    if (split_two_args(args, first, sizeof(first), second, sizeof(second)) == 0) {
        afs_find(first, second);
        return;
    }
    if (split_first_arg(args, first, sizeof(first)) == 0) {
        afs_find("", first);
        return;
    }
    console_writeln(tr("Nutze: find <wort> oder find <startpfad> <wort>", "Use: find <term> or find <start-path> <term>"));
}

static void print_afs_result(int rc, const char* ok_de, const char* ok_en, const char* fail_de, const char* fail_en) {
    if (rc == AFS_OK || rc >= 0) {
        console_writeln(tr(ok_de, ok_en));
        return;
    }
    if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Zugriff verweigert.", "Access denied."));
        return;
    }
    console_writeln(tr(fail_de, fail_en));
}

static void print_remove_result(int rc) {
    if (rc == AFS_OK) {
        console_writeln(tr("Eintrag geloescht.", "Entry deleted."));
        return;
    }
    if (rc == AFS_ERR_NOT_EMPTY) {
        console_writeln(tr("Ordner ist nicht leer. Nutze 'rm -r <pfad>'.", "Folder is not empty. Use 'rm -r <path>'."));
        return;
    }
    if (rc == AFS_ERR_BUSY) {
        console_writeln(tr("Du befindest dich gerade in diesem Ordner.", "You are currently inside that folder."));
        return;
    }
    if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Zugriff verweigert.", "Access denied."));
        return;
    }
    console_writeln(tr("Loeschen fehlgeschlagen.", "Delete failed."));
}

static void command_login(const char* args) {
    char username[24];
    char password[32];
    int rc;
    password[0] = '\0';

    if (split_two_args(args, username, sizeof(username), password, sizeof(password)) != 0) {
        if (split_first_arg(args, username, sizeof(username)) != 0) {
            console_writeln(tr("Nutze: login <name> [passwort]", "Use: login <name> [password]"));
            return;
        }
    }

    if (kstrcmp(username, "master") == 0 || kstrcmp(username, "system") == 0 || kstrcmp(username, "root") == 0) {
        console_writeln(tr("Der System-Modus ist kein normaler Benutzer. Nutze 'elevate <passwort>'.", "System mode is not a regular user. Use 'elevate <password>'."));
        return;
    }

    rc = user_login(username, password[0] != '\0' ? password : (const char*)0);
    if (rc == 0 || rc == 1) {
        afs_set_home(user_current()->username);
        (void)user_save_persistent();
        console_write(tr("Benutzer gewechselt zu ", "Switched user to "));
        console_writeln(user_current()->username);
        return;
    }
    if (rc == -2) {
        console_writeln(tr("Falsches Passwort.", "Wrong password."));
        return;
    }
    console_writeln(tr("Diesen Benutzer kenne ich nicht.", "I do not know that user."));
}

static void command_passwd(const char* args) {
    char first[32];
    char second[32];
    int rc;
    if (split_two_args(args, first, sizeof(first), second, sizeof(second)) == 0) {
        if (needs_master("passwd <user> <neu>") != 0) {
            return;
        }
        rc = user_set_password(first, second);
        if (rc == 0) {
            console_writeln(tr("Passwort aktualisiert.", "Password updated."));
        } else {
            console_writeln(tr("Passwort konnte nicht geaendert werden.", "Could not update password."));
        }
        return;
    }

    if (split_two_args(args, first, sizeof(first), second, sizeof(second)) != 0) {
        if (split_first_arg(args, first, sizeof(first)) != 0) {
            console_writeln(tr("Nutze: passwd <neu> oder passwd <user> <neu>", "Use: passwd <new> or passwd <user> <new>"));
            return;
        }
        rc = user_set_password(user_current()->username, first);
        if (rc == 0) {
            console_writeln(tr("Dein Passwort wurde geaendert.", "Your password was changed."));
        } else {
            console_writeln(tr("Passwort konnte nicht geaendert werden.", "Could not update password."));
        }
    }
}

static void command_protect(const char* args) {
    char mode[24];
    char path[64];
    int rc;
    if (split_two_args(args, mode, sizeof(mode), path, sizeof(path)) != 0) {
        console_writeln(tr("Nutze: protect <private|team|public|shared> <pfad>", "Use: protect <private|team|public|shared> <path>"));
        return;
    }
    rc = afs_protect(path, mode);
    if (rc == AFS_OK) {
        console_writeln(tr("Rechte aktualisiert.", "Rights updated."));
    } else if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Das darf nur der Besitzer oder der System-Modus.", "Only the owner or system mode can do that."));
    } else {
        console_writeln(tr("Rechte konnten nicht gesetzt werden.", "Could not update rights."));
    }
}

static void command_owner(const char* args) {
    char owner[24];
    char path[64];
    int rc;
    if (split_two_args(args, owner, sizeof(owner), path, sizeof(path)) != 0) {
        console_writeln(tr("Nutze: owner <user> <pfad>", "Use: owner <user> <path>"));
        return;
    }
    rc = afs_chown(path, owner, (const char*)0);
    if (rc == AFS_OK) {
        console_writeln(tr("Besitzer aktualisiert.", "Owner updated."));
    } else if (rc == AFS_ERR_DENIED) {
        console_writeln(tr("Dafuer brauchst du System-Rechte.", "You need system rights for that."));
    } else {
        console_writeln(tr("Besitzer konnte nicht gesetzt werden.", "Could not update owner."));
    }
}

static void print_diag(void) {
    console_writeln(tr("Diagnose:", "Diagnostics:"));
    if (g_setting_verbose_diag != 0) {
        console_writeln(tr("  Modus: ausfuehrlich", "  Mode: verbose"));
    }
    console_write(tr("  Speicher: ", "  Storage: "));
    console_writeln(afs_persistence_name());
    console_write(tr("  Benutzer-Modus: ", "  User mode: "));
    console_writeln(user_is_master() != 0 ? tr("system", "system") : tr("normal", "normal"));
    console_write(tr("  Netzwerk-Backend: ", "  Network backend: "));
    console_writeln(network_backend_name());
    console_write(tr("  NICs: ", "  NICs: "));
    console_write_dec((uint32_t)network_nic_count());
    console_putc('\n');
    console_write(tr("  Treiber: ", "  Driver: "));
    console_writeln(network_driver_active() != 0 ? network_driver_name() : tr("nicht aktiv", "not active"));
    console_write(tr("  MAC: ", "  MAC: "));
    console_writeln(network_mac_address());
    console_write(tr("  Apps installiert: ", "  Installed apps: "));
    { uint32_t installed = 0U; size_t i; for (i = 0U; i < app_count(); ++i) { const app_t* app = app_get(i); if (app != (const app_t*)0 && app->installed != 0U) { installed++; } } console_write_dec(installed); console_putc('\n'); }
}


static int save_system_state(void) {
    int afs_ok = afs_save_persistent();
    int user_ok = user_save_persistent();
    int net_ok = network_save_persistent();
    int app_ok = app_save_persistent();
    return (afs_ok == AFS_OK && user_ok == 0 && net_ok == 0 && app_ok == 0) ? 0 : -1;
}

static int load_system_state(void) {
    int user_ok = user_load_persistent();
    int net_ok = network_load_persistent();
    int afs_ok = afs_load_persistent();
    int app_ok = app_load_persistent();
    return (afs_ok == AFS_OK && user_ok == 0 && net_ok == 0 && app_ok == 0) ? 0 : -1;
}

static void __attribute__((unused)) command_hostname(const char* args) {
    char value[32];
    if (split_first_arg(args, value, sizeof(value)) != 0) {
        console_write(tr("Hostname: ", "Hostname: "));
        console_writeln(network_hostname());
        return;
    }
    if (needs_master("hostname <name>") != 0) {
        return;
    }
    if (network_set_hostname(value) == 0) {
        console_writeln(tr("Hostname aktualisiert.", "Hostname updated."));
    } else {
        console_writeln(tr("Hostname konnte nicht gesetzt werden.", "Could not update hostname."));
    }
}

static void __attribute__((unused)) command_dhcp(const char* args) {
    char value[16];
    if (split_first_arg(args, value, sizeof(value)) != 0) {
        console_write(tr("DHCP ist ", "DHCP is "));
        console_writeln(network_dhcp_enabled() != 0 ? tr("aktiv", "enabled") : tr("aus", "off"));
        return;
    }
    if (needs_master("dhcp <on|off>") != 0) {
        return;
    }
    if (kstrcmp(value, "on") == 0) {
        (void)network_set_dhcp(1);
        console_writeln(tr("DHCP aktiviert.", "DHCP enabled."));
        return;
    }
    if (kstrcmp(value, "off") == 0) {
        (void)network_set_dhcp(0);
        console_writeln(tr("DHCP deaktiviert.", "DHCP disabled."));
        return;
    }
    console_writeln(tr("Nutze: dhcp <on|off>", "Use: dhcp <on|off>"));
}

static void __attribute__((unused)) command_ip(const char* args) {
    char action[16];
    char value[24];
    if (split_two_args(args, action, sizeof(action), value, sizeof(value)) == 0 && kstrcmp(action, "set") == 0) {
        if (needs_master("ip set <adresse>") != 0) {
            return;
        }
        if (network_set_ip(value) == 0) {
            console_writeln(tr("IP-Adresse aktualisiert.", "IP address updated."));
        } else {
            console_writeln(tr("IP-Adresse konnte nicht gesetzt werden.", "Could not update IP address."));
        }
        return;
    }
    console_write(tr("IP-Adresse: ", "IP address: "));
    console_writeln(network_ip());
}

static void __attribute__((unused)) command_gateway(const char* args) {
    char action[16];
    char value[24];
    if (split_two_args(args, action, sizeof(action), value, sizeof(value)) == 0 && kstrcmp(action, "set") == 0) {
        if (needs_master("gateway set <adresse>") != 0) {
            return;
        }
        if (network_set_gateway(value) == 0) {
            console_writeln(tr("Gateway aktualisiert.", "Gateway updated."));
        } else {
            console_writeln(tr("Gateway konnte nicht gesetzt werden.", "Could not update gateway."));
        }
        return;
    }
    console_write(tr("Gateway: ", "Gateway: "));
    console_writeln(network_gateway());
}

static void command_ping(const char* args) {
    char target[64];
    char out[160];
    int rc;
    if (split_first_arg(args, target, sizeof(target)) != 0) {
        console_writeln(tr("Nutze: ping <ziel>", "Use: ping <target>"));
        return;
    }
    rc = network_ping(target, out, sizeof(out));
    console_writeln(out);
    if (rc > 0 && network_driver_active() == 0) {
        console_writeln(tr("Hinweis: Starte 'netup', wenn du eine Intel PRO/1000 in der VM nutzt.", "Hint: start 'netup' if you use an Intel PRO/1000 in the VM."));
    }
}


static void command_netup(void) {
    int rc = network_bring_up();
    if (rc == 0) {
        console_writeln(tr("e1000-Treiber gestartet.", "e1000 driver started."));
        if (g_setting_driver_debug != 0) {
            console_writeln(tr("Treiber-Diagnose: MMIO initialisiert, MAC gelesen, Link-Status wird beobachtet.", "Driver diagnostics: MMIO initialized, MAC read, link status will be monitored."));
        }
        print_network_status();
        return;
    }
    if (rc == 1) {
        console_writeln(tr("Der Netzwerktreiber ist bereits aktiv.", "The network driver is already active."));
        return;
    }
    if (rc == -1) {
        console_writeln(tr("Keine passende e1000-Karte gefunden. In VirtualBox hilft Intel PRO/1000 MT Desktop.", "No fitting e1000 card found. In VirtualBox, Intel PRO/1000 MT Desktop helps."));
        return;
    }
    if (rc == -2) {
        console_writeln(tr("Die Karte wurde gefunden, aber der MMIO-Zugriff war ungueltig.", "The card was found, but MMIO access was invalid."));
        return;
    }
    console_writeln(tr("Der e1000-Treiber konnte nicht sauber starten.", "The e1000 driver could not start cleanly."));
}

static void command_netprobe(void) {
    int rc = network_send_probe();
    if (rc == 0) {
        console_writeln(tr("Rohdaten-Testframe gesendet.", "Raw probe frame sent."));
        return;
    }
    if (rc == -1) {
        console_writeln(tr("Noch kein aktiver Netzwerktreiber. Nutze zuerst 'netup'.", "No active network driver yet. Use 'netup' first."));
        return;
    }
    console_writeln(tr("Senden fehlgeschlagen oder Warteschlange blockiert.", "Send failed or the queue is blocked."));
}

static void command_mac(void) {
    console_write(tr("Aktive MAC-Adresse: ", "Active MAC address: "));
    console_writeln(network_mac_address());
}

static void app_run_target(const char* name) {
    if (app_is_installed(name) == 0) {
        console_writeln(tr("Diese App ist gerade nicht installiert.", "That app is not installed right now."));
        return;
    }
    if (kstrcmp(name, "lumen") == 0) {
        editor_open("notes.txt");
        return;
    }
    if (kstrcmp(name, "files") == 0) {
        afs_ls("");
        return;
    }
    if (kstrcmp(name, "settings") == 0) {
        open_target("settings");
        return;
    }
    if (kstrcmp(name, "network") == 0) {
        print_network_status();
        print_nic_status();
        return;
    }
    if (kstrcmp(name, "monitor") == 0) {
        print_status();
        return;
    }
    console_writeln(tr("App-Platzhalter geoeffnet.", "App placeholder opened."));
}

static void command_app(const char* args) {
    char action[16];
    char name[24];
    size_t i;
    if (split_first_arg(args, action, sizeof(action)) != 0) {
        console_writeln(tr("Nutze: app <list|install|remove|info|run> [name]", "Use: app <list|install|remove|info|run> [name]"));
        return;
    }
    if (kstrcmp(action, "list") == 0) {
        console_writeln(tr("App-Liste:", "App list:"));
        for (i = 0U; i < app_count(); ++i) {
            const app_t* app = app_get(i);
            if (app == (const app_t*)0) {
                continue;
            }
            console_write("  - ");
            console_write(app->name);
            console_write(" [");
            console_write(app->installed != 0U ? tr("installiert", "installed") : tr("nicht installiert", "not installed"));
            console_write(app->builtin != 0U ? tr(", intern] ", ", built-in] ") : "] ");
            console_writeln(app->description);
        }
        return;
    }
    if (split_two_args(args, action, sizeof(action), name, sizeof(name)) != 0) {
        console_writeln(tr("Nutze: app <install|remove|run> <name>", "Use: app <install|remove|run> <name>"));
        return;
    }
    if (kstrcmp(action, "install") == 0) {
        int rc;
        if (needs_master("app install <name>") != 0) { return; }
        rc = app_install(name);
        if (rc == 0 || rc == 1) { console_writeln(tr("App ist jetzt installiert.", "App is now installed.")); }
        else { console_writeln(tr("Diese App kenne ich nicht.", "I do not know that app.")); }
        return;
    }
    if (kstrcmp(action, "remove") == 0) {
        int rc;
        if (needs_master("app remove <name>") != 0) { return; }
        rc = app_remove(name);
        if (rc == 0 || rc == 1) { console_writeln(tr("App wurde entfernt.", "App was removed.")); }
        else if (rc == -2) { console_writeln(tr("Interne Apps koennen nicht entfernt werden.", "Built-in apps cannot be removed.")); }
        else { console_writeln(tr("Diese App kenne ich nicht.", "I do not know that app.")); }
        return;
    }
    if (kstrcmp(action, "info") == 0) {
        const app_t* app = app_find(name);
        if (app == (const app_t*)0) {
            console_writeln(tr("Diese App kenne ich nicht.", "I do not know that app."));
            return;
        }
        console_write(tr("Titel: ", "Title: "));
        console_writeln(app->title);
        console_write(tr("Status: ", "State: "));
        console_writeln(app->installed != 0U ? tr("installiert", "installed") : tr("optional", "optional"));
        console_write(tr("Beschreibung: ", "Description: "));
        console_writeln(app->description);
        return;
    }
    if (kstrcmp(action, "run") == 0 || kstrcmp(action, "open") == 0) {
        app_run_target(name);
        return;
    }
    console_writeln(tr("Nutze: app <list|install|remove|info|run> [name]", "Use: app <list|install|remove|info|run> [name]"));
}

static void run_command(const char* cmd) {
    if (program_guard_command(cmd) == 0) { return; }
    if (kstrcmp(cmd, "help") == 0) { show_help(); return; }
    if (kstrcmp(cmd, "quickstart") == 0) { print_quickstart(); return; }
    if (kstrcmp(cmd, "clear") == 0) { console_clear(); console_writeln(tr("Bildschirm geleert.", "Screen cleared.")); return; }
    if (kstrcmp(cmd, "apps") == 0 || kstrcmp(cmd, "modules") == 0) { print_apps(); return; }
    if (kstrcmp(cmd, "tasks") == 0 || kstrcmp(cmd, "services") == 0) { print_tasks(); return; }
    if (kstrcmp(cmd, "memory") == 0 || kstrcmp(cmd, "mem") == 0) { print_memory_status(); return; }
    if (kstrcmp(cmd, "history") == 0) { print_history(); return; }
    if (kstrcmp(cmd, "status") == 0) { print_status(); return; }
    if (kstrcmp(cmd, "about") == 0) { show_about(); return; }
    if (kstrcmp(cmd, "version") == 0) { print_version(); return; }
    if (kstrcmp(cmd, "welcome") == 0 || kstrcmp(cmd, "banner") == 0) { shell_show_welcome(); return; }
    if (kstrcmp(cmd, "users") == 0) { print_users(); return; }
    if (kstrcmp(cmd, "whoami") == 0) { print_whoami(); return; }
    if (kstrcmp(cmd, "notes") == 0) { editor_list_documents(); return; }
    if (kstrcmp(cmd, "fs") == 0 || kstrcmp(cmd, "filesystem") == 0) { print_fs(); return; }
    if (kstrcmp(cmd, "settings") == 0 || kstrcmp(cmd, "preferences") == 0) { settings_open(); return; }
    if (kstrcmp(cmd, "settings general") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_GENERAL; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings network") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_NETWORK; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings security") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_SECURITY; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings expert") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_EXPERT; settings_draw_list(); return; }
    if (kstrcmp(cmd, "network") == 0 || kstrcmp(cmd, "net") == 0) { print_network_status(); return; }
    if (kstrcmp(cmd, "nic") == 0 || kstrcmp(cmd, "adapters") == 0) { print_nic_status(); return; }
    if (kstrcmp(cmd, "netup") == 0) { command_netup(); return; }
    if (kstrcmp(cmd, "netprobe") == 0) { command_netprobe(); return; }
    if (kstrcmp(cmd, "mac") == 0) { command_mac(); return; }
    if (kstrcmp(cmd, "diag") == 0) { print_diag(); return; }
    if (kstarts_with(cmd, "which ")) { command_which(cmd + 6); return; }
    if (kstrcmp(cmd, "disk") == 0 || kstrcmp(cmd, "storage") == 0) { print_disk_status(); return; }
    if (kstrcmp(cmd, "savefs") == 0 || kstrcmp(cmd, "sync") == 0) {
        int rc = save_system_state();
        if (rc == 0) { console_writeln(tr("AuroraOS-Zustand wurde auf die virtuelle Platte gespeichert (Dateien, Nutzer, Netzwerk und Apps).", "AuroraOS state was saved to the virtual disk (files, users, network and apps).")); }
        else { console_writeln(tr("Speichern fehlgeschlagen. Wahrscheinlich ist keine virtuelle Platte angeschlossen.", "Save failed. There is probably no virtual disk attached.")); }
        return;
    }
    if (kstrcmp(cmd, "loadfs") == 0) {
        int rc = load_system_state();
        if (rc == 0) { console_writeln(tr("AuroraOS-Zustand wurde von der virtuellen Platte geladen.", "AuroraOS state was loaded from the virtual disk.")); network_rescan(); }
        else { console_writeln(tr("Laden fehlgeschlagen. Vielleicht gibt es noch keinen gespeicherten Stand.", "Load failed. There may be no saved state yet.")); }
        return;
    }
    if (kstrcmp(cmd, "pwd") == 0) { print_working_path(); return; }
    if (kstrcmp(cmd, "cmd") == 0) { command_cmd("list"); return; }
    if (kstarts_with(cmd, "stat ")) { print_entry_status(cmd + 5); return; }
    if (kstrcmp(cmd, "cd") == 0) {
        if (afs_set_home(user_current()->username) == AFS_OK) {
            print_working_path();
        }
        return;
    }
    if (kstrcmp(cmd, "ls") == 0) { afs_ls(""); return; }
    if (kstrcmp(cmd, "reboot") == 0 || kstrcmp(cmd, "restart") == 0) { shell_reboot(); return; }
    if (kstarts_with(cmd, "elevate")) {
        const char* pass = cmd + 7;
        while (*pass == ' ') { pass++; }
        if (*pass == '\0') {
            console_writeln(tr("Nutze: elevate <passwort>", "Use: elevate <password>"));
            return;
        }
        if (user_elevate(pass) >= 0) {
            afs_set_home(user_current()->username);
            console_writeln(tr("System-Rechte aktiv.", "System rights active."));
        } else {
            console_writeln(tr("System-Modus konnte nicht aktiviert werden.", "Could not activate system mode."));
        }
        return;
    }
    if (kstrcmp(cmd, "drop") == 0) {
        user_drop();
        afs_set_home(user_current()->username);
        console_writeln(tr("Normale Rechte wieder aktiv.", "Normal rights are active again."));
        return;
    }
    if (kstarts_with(cmd, "echo ")) { console_writeln(cmd + 5); return; }
    if (kstarts_with(cmd, "alloc ")) {
        int bytes = katoi(cmd + 6);
        void* ptr;
        if (bytes <= 0) { console_writeln(tr("Bitte gib eine positive Zahl an.", "Please enter a positive number.")); return; }
        ptr = kmalloc((size_t)bytes);
        if (ptr == (void*)0) { console_writeln(tr("Dafuer ist nicht genug freier Speicher da.", "There is not enough free memory for that.")); return; }
        console_write(tr("Speicher reserviert bei ", "Memory reserved at "));
        console_write_hex((uint32_t)(uintptr_t)ptr);
        console_write(tr(" mit ", " with "));
        console_write_dec((uint32_t)bytes);
        console_writeln(" bytes");
        return;
    }
    if (kstarts_with(cmd, "start ")) {
        const char* name = cmd + 6;
        if (needs_master("start <name>") != 0) return;
        if (task_start(name) == 0) {
            console_write(tr("Bereich gestartet: ", "Area started: "));
            console_writeln(name);
            return;
        }
        console_writeln(tr("Diesen Bereich kenne ich noch nicht.", "I do not know that area yet."));
        return;
    }
    if (kstarts_with(cmd, "stop ")) {
        const char* name = cmd + 5;
        if (needs_master("stop <name>") != 0) return;
        if (task_stop(name) == 0) {
            console_write(tr("Bereich gestoppt: ", "Area stopped: "));
            console_writeln(name);
            return;
        }
        console_writeln(tr("Diesen Bereich kenne ich noch nicht.", "I do not know that area yet."));
        return;
    }
    if (kstarts_with(cmd, "open ")) { open_target(cmd + 5); return; }
    if (kstarts_with(cmd, "ping ")) { command_ping(cmd + 5); return; }
    if (kstrcmp(cmd, "netscan") == 0) { network_rescan(); print_nic_status(); return; }
    if (kstrcmp(cmd, "app") == 0) { command_app("list"); return; }
    if (kstrcmp(cmd, "prog") == 0) { command_prog("list"); return; }
    if (kstarts_with(cmd, "app ")) { command_app(cmd + 4); return; }
    if (kstarts_with(cmd, "prog ")) { command_prog(cmd + 5); return; }
    if (kstarts_with(cmd, "cmd ")) { command_cmd(cmd + 4); return; }
    if (kstarts_with(cmd, "login ")) { command_login(cmd + 6); return; }
    if (kstarts_with(cmd, "passwd ")) { command_passwd(cmd + 7); return; }
    if (kstarts_with(cmd, "protect ")) { command_protect(cmd + 8); return; }
    if (kstarts_with(cmd, "chmod ")) { command_protect(cmd + 6); return; }
    if (kstarts_with(cmd, "owner ")) { command_owner(cmd + 6); return; }
    if (kstarts_with(cmd, "chown ")) { command_owner(cmd + 6); return; }
    if (kstarts_with(cmd, "cd ")) {
        int rc = afs_cd(cmd + 3);
        if (rc == AFS_OK) {
            print_working_path();
            return;
        }
        print_afs_result(rc, "", "", "Ordner nicht gefunden.", "Folder not found.");
        return;
    }
    if (kstarts_with(cmd, "ls ")) { afs_ls(cmd + 3); return; }
    if (kstarts_with(cmd, "mkdir ")) {
        print_afs_result(afs_mkdir(cmd + 6), "Ordner erstellt.", "Folder created.", "Ordner konnte nicht erstellt werden.", "Could not create folder.");
        return;
    }
    if (kstarts_with(cmd, "touch ")) {
        print_afs_result(afs_touch(cmd + 6), "Datei erstellt.", "File created.", "Datei konnte nicht erstellt werden.", "Could not create file.");
        return;
    }
    if (kstarts_with(cmd, "rmdir ")) {
        print_remove_result(afs_rmdir(cmd + 6, 0));
        return;
    }
    if (kstarts_with(cmd, "rm ") || kstarts_with(cmd, "delete ")) {
        const char* arg = kstarts_with(cmd, "rm ") ? cmd + 3 : cmd + 7;
        int recursive = 0;
        while (*arg == ' ') { arg++; }
        if (kstarts_with(arg, "-r ")) {
            recursive = 1;
            arg += 3;
        } else if (kstarts_with(arg, "-rf ")) {
            recursive = 1;
            arg += 4;
        }
        while (*arg == ' ') { arg++; }
        if (*arg == '\0') {
            console_writeln(tr("Nutze: rm <pfad> oder rm -r <pfad>", "Use: rm <path> or rm -r <path>"));
            return;
        }
        if (afs_is_dir(arg) != 0) {
            print_remove_result(afs_rmdir(arg, recursive));
        } else {
            print_remove_result(afs_rm(arg));
        }
        return;
    }
    if (kstarts_with(cmd, "cp ")) { command_cp(cmd + 3); return; }
    if (kstarts_with(cmd, "copy ")) { command_cp(cmd + 5); return; }
    if (kstarts_with(cmd, "mv ")) { command_mv(cmd + 3); return; }
    if (kstarts_with(cmd, "move ")) { command_mv(cmd + 5); return; }
    if (kstrcmp(cmd, "find") == 0) { command_find(""); return; }
    if (kstarts_with(cmd, "find ")) { command_find(cmd + 5); return; }
    if (kstarts_with(cmd, "cat ")) { print_file_contents(cmd + 4); return; }
    if (kstarts_with(cmd, "write ")) {
        char name[48];
        const char* text;
        if (split_name_and_text(cmd + 6, name, sizeof(name), &text) != 0) {
            console_writeln(tr("Nutze: write <datei> <text>", "Use: write <file> <text>"));
            return;
        }
        print_afs_result(afs_write_file(name, text), "Datei geschrieben.", "File written.", "Schreiben fehlgeschlagen.", "Write failed.");
        return;
    }
    if (kstarts_with(cmd, "append ")) {
        char name[48];
        const char* text;
        if (split_name_and_text(cmd + 7, name, sizeof(name), &text) != 0) {
            console_writeln(tr("Nutze: append <datei> <text>", "Use: append <file> <text>"));
            return;
        }
        print_afs_result(afs_append_file(name, text), "Text angehaengt.", "Text appended.", "Anhaengen fehlgeschlagen.", "Append failed.");
        return;
    }
    if (kstarts_with(cmd, "edit ")) {
        editor_open(cmd + 5);
        return;
    }
    if (kstarts_with(cmd, "ai ")) { ai_route(cmd + 3); return; }
    if (try_custom_command(cmd) != 0) { return; }
    if (kstrlen(cmd) == 0U) { return; }

    if (g_setting_ai_smart != 0) {
        console_writeln(tr("Nicht direkt erkannt. Ich versuche die AI-Hilfe ...", "Not recognized directly. I will try AI help ..."));
        ai_route(cmd);
    } else {
        console_writeln(tr("Nicht direkt erkannt. Tipp: Nutze 'help' oder 'settings'.", "Not recognized directly. Tip: use 'help' or 'settings'."));
    }
}

void shell_init(void) {
    input_len = 0;
    history_count = 0U;
    history_view = -1;
    input_buffer[0] = '\0';
    history_draft[0] = '\0';
    print_quickstart();
    console_writeln("");
    prompt();
}

void shell_handle_key(int key) {
    if (key == KEY_NONE) {
        return;
    }

    if (editor_is_active() != 0) {
        editor_handle_key(key);
        if (editor_is_active() == 0) {
            prompt();
        }
        return;
    }

    if (g_settings_active != 0) {
        settings_handle_key(key);
        if (g_settings_active == 0) {
            prompt();
        }
        return;
    }

    if (key == KEY_PAGEUP) {
        console_scroll_page_up();
        return;
    }
    if (key == KEY_PAGEDOWN) {
        console_scroll_page_down();
        return;
    }
    if (console_is_scrollback_active() != 0) {
        console_scroll_to_bottom();
        prompt();
        replace_input_line(input_buffer);
    }
    if (key == KEY_CTRL_C) {
        shell_cancel_input();
        return;
    }
    if (key == KEY_UP) { history_up(); return; }
    if (key == KEY_DOWN) { history_down(); return; }
    if (key == KEY_BACKSPACE) {
        if (input_len > 0U) {
            input_len--;
            input_buffer[input_len] = '\0';
            console_backspace();
        }
        return;
    }
    if (key == KEY_ENTER) {
        input_buffer[input_len] = '\0';
        console_putc('\n');
        history_store(input_buffer);
        run_command(input_buffer);
        input_len = 0;
        input_buffer[0] = '\0';
        history_draft[0] = '\0';
        history_view = -1;
        if (editor_is_active() == 0) {
            prompt();
        }
        return;
    }
    if (key > 0 && key < 256 && input_len < INPUT_MAX - 1U) {
        input_buffer[input_len++] = (char)key;
        input_buffer[input_len] = '\0';
        console_putc((char)key);
    }
}
