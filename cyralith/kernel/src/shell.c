#include "shell.h"
#include "cyralithfs.h"
#include "console.h"
#include "string.h"
#include "ai_core.h"
#include "memory.h"
#include "task.h"
#include "timer.h"
#include "keyboard.h"
#include "editor.h"
#include "snake.h"
#include "arcade.h"
#include "user.h"
#include "network.h"
#include "app.h"
#include "extprog.h"
#include "actionlog.h"
#include "automation.h"
#include "recovery.h"
#include "paging.h"
#include "process.h"
#include "interrupts.h"
#include "syslog.h"
#include "panic.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

#define INPUT_MAX 256
#define HISTORY_MAX 16
#define VERSION_TEXT "Cyralith Stable Build 2.7.0"

typedef enum {
    LANG_DE = 0,
    LANG_EN = 1,
    LANG_ID = 2
} language_t;

static char input_buffer[INPUT_MAX];
static size_t input_len = 0;
static language_t g_language = LANG_DE;
static char history[HISTORY_MAX][INPUT_MAX];
static size_t history_count = 0;
static int history_view = -1;
static char history_draft[INPUT_MAX];
static int g_keytest_active = 0;
static uint32_t g_keytest_count = 0U;

static void prompt(void);
static void print_file_contents(const char* path);

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
    SETTINGS_NETWORK_DRIVER = 4,
    SETTINGS_NETWORK_BACK = 5,
    SETTINGS_NETWORK_COUNT = 6
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
static unsigned int g_settings_anim_frame = 0U;
static unsigned int g_settings_anim_idle = 0U;

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
static void print_jobs(void);
static void print_action_log(void);
static void print_paging_status(void);
static void print_processes(void);
static void command_job(const char* args);
static void command_launch(const char* args);
static void command_pkg(const char* args);
static void command_doctor(void);
static void command_recover(void);
static void command_health(void);
static void command_bootinfo(void);
static void command_logview(const char* args);
static void command_safemode(const char* args);
static void command_panic(const char* args);
static void command_proc(const char* args);
static void command_netdriver(const char* args);
static void command_netdrivers(void);
static void command_games(void);
static void command_game(const char* args);
static void show_help_all(void);
static void show_help_overview(void);
static void command_help(const char* args);
static int split_first_arg(const char* input, char* out, size_t max);
static void log_action_simple(const char* action, const char* detail, actionlog_result_t result);
static const char* language_code(void) {
    switch (g_language) {
        case LANG_EN: return "en";
        case LANG_ID: return "id";
        case LANG_DE:
        default: return "de";
    }
}

static const char* language_label_full(void) {
    switch (g_language) {
        case LANG_EN: return "English / EN";
        case LANG_ID: return "Bahasa Indonesia / ID";
        case LANG_DE:
        default: return "Deutsch / DE";
    }
}

static const char* language_label_short(void) {
    switch (g_language) {
        case LANG_EN: return "English";
        case LANG_ID: return "Bahasa Indonesia";
        case LANG_DE:
        default: return "Deutsch";
    }
}

static const char* translate_id(const char* de, const char* en) {
    if (kstrcmp(de, "Apps und Bereiche:") == 0 && kstrcmp(en, "Apps and areas:") == 0) return "Aplikasi dan area:";
    if (kstrcmp(de, "  system    - Shell, Benutzer, CyralithFS und Grunddienste.") == 0 && kstrcmp(en, "  system    - Shell, users, CyralithFS and core services.") == 0) return "  system    - Shell, pengguna, CyralithFS, dan layanan inti.";
    if (kstrcmp(de, "installiert") == 0 && kstrcmp(en, "installed") == 0) return "terpasang";
    if (kstrcmp(de, "optional") == 0 && kstrcmp(en, "optional") == 0) return "opsional";
    if (kstrcmp(de, ", intern] ") == 0 && kstrcmp(en, ", built-in] ") == 0) return ", bawaan] ";
    if (kstrcmp(de, "  Hinweis: Nutze 'app list', 'app install <name>', 'app remove <name>' und 'app run <name>'.") == 0 && kstrcmp(en, "  Hint: use 'app list', 'app install <name>', 'app remove <name>' and 'app run <name>'.") == 0) return "  Tips: gunakan 'app list', 'app install <name>', 'app remove <name>', dan 'app run <name>'.";
    if (kstrcmp(de, "Speicher:") == 0 && kstrcmp(en, "Memory:") == 0) return "Memori:";
    if (kstrcmp(de, "  Insgesamt: ") == 0 && kstrcmp(en, "  Total:      ") == 0) return "  Total:      ";
    if (kstrcmp(de, "  Belegt:    ") == 0 && kstrcmp(en, "  Used:       ") == 0) return "  Terpakai:   ";
    if (kstrcmp(de, "  Frei:      ") == 0 && kstrcmp(en, "  Free:       ") == 0) return "  Kosong:     ";
    if (kstrcmp(de, "Paging / virtueller Speicher:") == 0 && kstrcmp(en, "Paging / virtual memory:") == 0) return "Paging / memori virtual:";
    if (kstrcmp(de, "  Aktiv: ") == 0 && kstrcmp(en, "  Enabled: ") == 0) return "  Aktif: ";
    if (kstrcmp(de, "ja") == 0 && kstrcmp(en, "yes") == 0) return "ya";
    if (kstrcmp(de, "nein") == 0 && kstrcmp(en, "no") == 0) return "tidak";
    if (kstrcmp(de, "  Seitengroesse: ") == 0 && kstrcmp(en, "  Page size: ") == 0) return "  Ukuran halaman: ";
    if (kstrcmp(de, "  Frames gesamt: ") == 0 && kstrcmp(en, "  Total frames: ") == 0) return "  Total frame: ";
    if (kstrcmp(de, "  Reserviert: ") == 0 && kstrcmp(en, "  Reserved: ") == 0) return "  Dicadangkan: ";
    if (kstrcmp(de, " | Benutzt: ") == 0 && kstrcmp(en, " | Used: ") == 0) return " | Terpakai: ";
    if (kstrcmp(de, " | Frei: ") == 0 && kstrcmp(en, " | Free: ") == 0) return " | Kosong: ";
    if (kstrcmp(de, "  Page Directory: ") == 0 && kstrcmp(en, "  Page directory: ") == 0) return "  Direktori halaman: ";
    if (kstrcmp(de, "  Page Faults: ") == 0 && kstrcmp(en, "  Page faults: ") == 0) return "  Page fault: ";
    if (kstrcmp(de, " | Letzte Adresse: ") == 0 && kstrcmp(en, " | Last address: ") == 0) return " | Alamat terakhir: ";
    if (kstrcmp(de, " | Fehlercode: ") == 0 && kstrcmp(en, " | Error code: ") == 0) return " | Kode galat: ";
    if (kstrcmp(de, "Aktive Systembereiche:") == 0 && kstrcmp(en, "Active system areas:") == 0) return "Area sistem aktif:";
    if (kstrcmp(de, "  Aktuell: ") == 0 && kstrcmp(en, "  Current: ") == 0) return "  Saat ini: ";
    if (kstrcmp(de, "Prozesse:") == 0 && kstrcmp(en, "Processes:") == 0) return "Proses:";
    if (kstrcmp(de, "  Keine Prozesse registriert.") == 0 && kstrcmp(en, "  No processes registered.") == 0) return "  Tidak ada proses terdaftar.";
    if (kstrcmp(de, "Geplante Aufgaben:") == 0 && kstrcmp(en, "Scheduled jobs:") == 0) return "Pekerjaan terjadwal:";
    if (kstrcmp(de, "  Keine aktiven Jobs.") == 0 && kstrcmp(en, "  No active jobs.") == 0) return "  Tidak ada job aktif.";
    if (kstrcmp(de, "Letzte erklaerbare Aktionen:") == 0 && kstrcmp(en, "Recent explainable actions:") == 0) return "Aksi yang dapat dijelaskan terakhir:";
    if (kstrcmp(de, "  Noch keine Aktionen protokolliert.") == 0 && kstrcmp(en, "  No logged actions yet.") == 0) return "  Belum ada aksi yang dicatat.";
    if (kstrcmp(de, "Letzte Befehle:") == 0 && kstrcmp(en, "Recent commands:") == 0) return "Perintah terakhir:";
    if (kstrcmp(de, "  Noch nichts gespeichert.") == 0 && kstrcmp(en, "  Nothing saved yet.") == 0) return "  Belum ada yang disimpan.";
    if (kstrcmp(de, "Bekannte Benutzer und Rollen:") == 0 && kstrcmp(en, "Known users and roles:") == 0) return "Pengguna dan peran yang dikenal:";
    if (kstrcmp(de, "  - system-mode  [special] Root-aehnlicher Sondermodus per 'elevate <passwort>'.") == 0 && kstrcmp(en, "  - system-mode  [special] Root-like special mode via 'elevate <password>'.") == 0) return "  - system-mode  [khusus] mode khusus mirip root lewat 'elevate <password>'.";
    if (kstrcmp(de, "Aktiver Benutzer: ") == 0 && kstrcmp(en, "Current user: ") == 0) return "Pengguna aktif: ";
    if (kstrcmp(de, "Gruppe: ") == 0 && kstrcmp(en, "Group: ") == 0) return "Grup: ";
    if (kstrcmp(de, "Sitzung: ") == 0 && kstrcmp(en, "Session: ") == 0) return "Sesi: ";
    if (kstrcmp(de, "System-Modus aktiv (#)") == 0 && kstrcmp(en, "System mode active (#)") == 0) return "Mode sistem aktif (#)";
    if (kstrcmp(de, "Normale Rechte (>)") == 0 && kstrcmp(en, "Normal rights (>)") == 0) return "Hak normal (>)";
    if (kstrcmp(de, "Tastatur-Layout: ") == 0 && kstrcmp(en, "Keyboard layout: ") == 0) return "Layout keyboard: ";
    if (kstrcmp(de, "Aktueller Ort: ") == 0 && kstrcmp(en, "Current location: ") == 0) return "Lokasi saat ini: ";
    if (kstrcmp(de, "CyralithFS Speicher:") == 0 && kstrcmp(en, "CyralithFS storage:") == 0) return "Penyimpanan CyralithFS:";
    if (kstrcmp(de, "  Status: virtuelle ATA-Platte erkannt. savefs/loadfs sind aktiv.") == 0 && kstrcmp(en, "  Status: virtual ATA disk detected. savefs/loadfs are active.") == 0) return "  Status: disk ATA virtual terdeteksi. savefs/loadfs aktif.";
    if (kstrcmp(de, "  Status: keine virtuelle Platte erkannt. CyralithFS bleibt nur im RAM.") == 0 && kstrcmp(en, "  Status: no virtual disk detected. CyralithFS stays in RAM only.") == 0) return "  Status: tidak ada disk virtual terdeteksi. CyralithFS tetap hanya di RAM.";
    if (kstrcmp(de, "  Tipp: Fuege in VirtualBox oder QEMU eine kleine virtuelle Festplatte hinzu.") == 0 && kstrcmp(en, "  Tip: add a small virtual hard disk in VirtualBox or QEMU.") == 0) return "  Tips: tambahkan hard disk virtual kecil di VirtualBox atau QEMU.";
    if (kstrcmp(de, "CyralithNet im Ueberblick:") == 0 && kstrcmp(en, "CyralithNet overview:") == 0) return "Ringkasan CyralithNet:";
    if (kstrcmp(de, "  Adresse: ") == 0 && kstrcmp(en, "  Address: ") == 0) return "  Alamat: ";
    if (kstrcmp(de, "  DHCP: ") == 0 && kstrcmp(en, "  DHCP: ") == 0) return "  DHCP: ";
    if (kstrcmp(de, "aktiv") == 0 && kstrcmp(en, "enabled") == 0) return "aktif";
    if (kstrcmp(de, "aus") == 0 && kstrcmp(en, "off") == 0) return "mati";
    if (kstrcmp(de, "  Erkannte NICs: ") == 0 && kstrcmp(en, "  Detected NICs: ") == 0) return "  NIC terdeteksi: ";
    if (kstrcmp(de, "  Treiber: ") == 0 && kstrcmp(en, "  Driver: ") == 0) return "  Driver: ";
    if (kstrcmp(de, "noch aus") == 0 && kstrcmp(en, "off") == 0) return "mati";
    if (kstrcmp(de, "  Verbindung: ") == 0 && kstrcmp(en, "  Link: ") == 0) return "  Koneksi: ";
    if (kstrcmp(de, "aktiv") == 0 && kstrcmp(en, "up") == 0) return "aktif";
    if (kstrcmp(de, "Treiber aktiv, Link unklar") == 0 && kstrcmp(en, "driver active, link not confirmed") == 0) return "Driver aktif, link belum pasti";
    if (kstrcmp(de, "noch nicht gestartet") == 0 && kstrcmp(en, "not started yet") == 0) return "belum dimulai";
    if (kstrcmp(de, "  Expertenmodus: ") == 0 && kstrcmp(en, "  Expert mode: ") == 0) return "  Mode ahli: ";
    if (kstrcmp(de, "Treiberdiagnose aktiv") == 0 && kstrcmp(en, "driver diagnostics active") == 0) return "diagnostik driver aktif";
    if (kstrcmp(de, "kompakt") == 0 && kstrcmp(en, "compact") == 0) return "ringkas";
    if (kstrcmp(de, "  Tipp: 'netup' startet den e1000-Treiber. 'netprobe' sendet einen kleinen Rohdaten-Test.") == 0 && kstrcmp(en, "  Tip: 'netup' starts the e1000 driver. 'netprobe' sends a small raw packet test.") == 0) return "  Tips: 'netup' memulai driver e1000. 'netprobe' mengirim uji paket mentah kecil.";
    if (kstrcmp(de, "Erkannte Netzwerkadapter:") == 0 && kstrcmp(en, "Detected network adapters:") == 0) return "Adaptor jaringan terdeteksi:";
    if (kstrcmp(de, "  Keine PCI-Netzwerkkarte erkannt. In VirtualBox hilft oft Intel PRO/1000 oder PCnet.") == 0 && kstrcmp(en, "  No PCI network card detected. In VirtualBox, Intel PRO/1000 or PCnet often helps.") == 0) return "  Tidak ada kartu jaringan PCI terdeteksi. Di VirtualBox, Intel PRO/1000 atau PCnet sering membantu.";
    if (kstrcmp(de, "  Noch kein Treiber aktiv. Nutze 'netup'.") == 0 && kstrcmp(en, "  No driver active yet. Use 'netup'.") == 0) return "  Belum ada driver aktif. Gunakan 'netup'.";
    if (kstrcmp(de, "CyralithFS im Ueberblick:") == 0 && kstrcmp(en, "CyralithFS overview:") == 0) return "Ringkasan CyralithFS:";
    if (kstrcmp(de, "  Hauptordner: /system, /home, /apps, /temp") == 0 && kstrcmp(en, "  Main folders: /system, /home, /apps, /temp") == 0) return "  Folder utama: /system, /home, /apps, /temp";
    if (kstrcmp(de, "  /system und /apps sind system-eigene Bereiche.") == 0 && kstrcmp(en, "  /system and /apps are system-owned areas.") == 0) return "  /system dan /apps adalah area milik sistem.";
    if (kstrcmp(de, "  /home/<user> gehoert dem jeweiligen Benutzer und ist privat.") == 0 && kstrcmp(en, "  /home/<user> belongs to that user and is private.") == 0) return "  /home/<user> milik pengguna tersebut dan bersifat privat.";
    if (kstrcmp(de, "  /temp ist ein gemeinsamer Bereich fuer schnelle Tests.") == 0 && kstrcmp(en, "  /temp is a shared area for quick tests.") == 0) return "  /temp adalah area bersama untuk tes cepat.";
    if (kstrcmp(de, "  Nutze stat/protect/owner fuer Rechte und Besitz.") == 0 && kstrcmp(en, "  Use stat/protect/owner for rights and ownership.") == 0) return "  Gunakan stat/protect/owner untuk hak akses dan kepemilikan.";
    if (kstrcmp(de, "  Speicher-Art: ") == 0 && kstrcmp(en, "  Storage mode: ") == 0) return "  Mode penyimpanan: ";
    if (kstrcmp(de, "persistenter ATA-Datentraeger aktiv") == 0 && kstrcmp(en, "persistent ATA disk active") == 0) return "disk ATA persisten aktif";
    if (kstrcmp(de, "nur RAM (keine Platte gefunden)") == 0 && kstrcmp(en, "RAM only (no disk found)") == 0) return "hanya RAM (tidak ada disk ditemukan)";
    if (kstrcmp(de, "  Eintraege: ") == 0 && kstrcmp(en, "  Entries: ") == 0) return "  Entri: ";
    if (kstrcmp(de, "Kurzer Systemueberblick:") == 0 && kstrcmp(en, "Quick system overview:") == 0) return "Ringkasan sistem singkat:";
    if (kstrcmp(de, "  Cyralith laeuft im stabilen Startmodus.") == 0 && kstrcmp(en, "  Cyralith is running in stable mode.") == 0) return "  Cyralith berjalan dalam mode start stabil.";
    if (kstrcmp(de, "  Freier Speicher: ") == 0 && kstrcmp(en, "  Free memory: ") == 0) return "  Memori kosong: ";
    if (kstrcmp(de, " / freie Frames=") == 0 && kstrcmp(en, " / free frames=") == 0) return " / frame kosong=";
    if (kstrcmp(de, "  CyralithFS Ort: ") == 0 && kstrcmp(en, "  CyralithFS path: ") == 0) return "  Lokasi CyralithFS: ";
    if (kstrcmp(de, "  CyralithFS Speicher: ") == 0 && kstrcmp(en, "  CyralithFS storage: ") == 0) return "  Penyimpanan CyralithFS: ";
    if (kstrcmp(de, "  Netzwerk: ") == 0 && kstrcmp(en, "  Network: ") == 0) return "  Jaringan: ";
    if (kstrcmp(de, " / Treiber=") == 0 && kstrcmp(en, " / Driver=") == 0) return " / Driver=";
    if (kstrcmp(de, "  Installierte Apps: ") == 0 && kstrcmp(en, "  Installed apps: ") == 0) return "  Aplikasi terpasang: ";
    if (kstrcmp(de, "  Offene Lumen-Dateien: ") == 0 && kstrcmp(en, "  Open Lumen files: ") == 0) return "  File Lumen terbuka: ";
    if (kstrcmp(de, "  Ticks seit Start: ") == 0 && kstrcmp(en, "  Ticks since boot: ") == 0) return "  Tick sejak boot: ";
    if (kstrcmp(de, "  Geplante Jobs: ") == 0 && kstrcmp(en, "  Scheduled jobs: ") == 0) return "  Job terjadwal: ";
    if (kstrcmp(de, "  Scheduler aktiv: ") == 0 && kstrcmp(en, "  Scheduler active: ") == 0) return "  Scheduler aktif: ";
    if (kstrcmp(de, "Schnellstart:") == 0 && kstrcmp(en, "Quick start:") == 0) return "Mulai cepat:";
    if (kstrcmp(de, "  1. help              - Zeigt alle wichtigen Befehle.") == 0 && kstrcmp(en, "  1. help              - Shows the main commands.") == 0) return "  1. help              - Menampilkan perintah utama.";
    if (kstrcmp(de, "  2. settings          - Oeffnet die uebersichtlichen Einstellungen.") == 0 && kstrcmp(en, "  2. settings          - Opens the clear settings screen.") == 0) return "  2. settings          - Membuka pengaturan yang rapi.";
    if (kstrcmp(de, "  3. whoami            - Zeigt, wer gerade angemeldet ist.") == 0 && kstrcmp(en, "  3. whoami            - Shows who is currently signed in.") == 0) return "  3. whoami            - Menampilkan siapa yang sedang masuk.";
    if (kstrcmp(de, "  4. cd ~              - Springt in deinen Home-Ordner.") == 0 && kstrcmp(en, "  4. cd ~              - Jumps to your home folder.") == 0) return "  4. cd ~              - Pindah ke folder home kamu.";
    if (kstrcmp(de, "  5. app list          - Zeigt installierte und optionale Apps.") == 0 && kstrcmp(en, "  5. app list          - Shows installed and optional apps.") == 0) return "  5. app list          - Menampilkan aplikasi terpasang dan opsional.";
    if (kstrcmp(de, "  6. edit notes.txt    - Oeffnet Lumen.") == 0 && kstrcmp(en, "  6. edit notes.txt    - Opens Lumen.") == 0) return "  6. edit notes.txt    - Membuka Lumen.";
    if (kstrcmp(de, "  7. nic               - Zeigt erkannte Netzwerkadapter.") == 0 && kstrcmp(en, "  7. nic               - Shows detected network adapters.") == 0) return "  7. nic               - Menampilkan adaptor jaringan yang terdeteksi.";
    if (kstrcmp(de, "  8. netup             - Startet den ersten e1000-Treiber.") == 0 && kstrcmp(en, "  8. netup             - Starts the first e1000 driver.") == 0) return "  8. netup             - Memulai driver e1000 pertama.";
    if (kstrcmp(de, "  9. elevate cyralith    - Aktiviert den root-aehnlichen System-Modus.") == 0 && kstrcmp(en, "  9. elevate cyralith    - Activates the root-like system mode.") == 0) return "  9. elevate cyralith    - Mengaktifkan mode sistem mirip root.";
    if (kstrcmp(de, " 10. savefs            - Speichert Dateien, Nutzer, Netzwerk und Apps auf die virtuelle Platte.") == 0 && kstrcmp(en, " 10. savefs            - Saves files, users, network and apps to the virtual disk.") == 0) return " 10. savefs            - Menyimpan file, pengguna, jaringan, dan aplikasi ke disk virtual.";
    if (kstrcmp(de, " 11. cmd new hallo     - Erstellt einen eigenen Befehl.") == 0 && kstrcmp(en, " 11. cmd new hallo     - Creates your own command.") == 0) return " 11. cmd new hallo     - Membuat perintah sendiri.";
    if (kstrcmp(de, " 12. prog info hallo   - Zeigt Rechte, Vertrauen und Freigabe.") == 0 && kstrcmp(en, " 12. prog info hallo   - Shows permissions, trust and approval.") == 0) return " 12. prog info hallo   - Menampilkan izin, kepercayaan, dan persetujuan.";
    if (kstrcmp(de, " 13. which hallo       - Zeigt, woher ein Befehl kommt.") == 0 && kstrcmp(en, " 13. which hallo       - Shows where a command comes from.") == 0) return " 13. which hallo       - Menampilkan asal sebuah perintah.";
    if (kstrcmp(de, " 14. job add 10 status - Plant einen Befehl in 10 Sekunden.") == 0 && kstrcmp(en, " 14. job add 10 status - Schedules a command in 10 seconds.") == 0) return " 14. job add 10 status - Menjadwalkan perintah dalam 10 detik.";
    if (kstrcmp(de, " 15. doctor            - Startet den Diagnose-Assistenten.") == 0 && kstrcmp(en, " 15. doctor            - Starts the diagnosis assistant.") == 0) return " 15. doctor            - Memulai asisten diagnosis.";
    if (kstrcmp(de, "Layout gesetzt: Deutsch (QWERTZ)") == 0 && kstrcmp(en, "Layout set: German (QWERTZ)") == 0) return "Layout diatur: Jerman (QWERTZ)";
    if (kstrcmp(de, "Layout gesetzt: US (QWERTY)") == 0 && kstrcmp(en, "Layout set: US (QWERTY)") == 0) return "Layout diatur: US (QWERTY)";
    if (kstrcmp(de, "Nutze: layout <de|us>") == 0 && kstrcmp(en, "Use: layout <de|us>") == 0) return "Gunakan: layout <de|us>";
    if (kstrcmp(de, "Einstellungen") == 0 && kstrcmp(en, "Settings") == 0) return "Pengaturan";
    if (kstrcmp(de, "Allgemein") == 0 && kstrcmp(en, "General") == 0) return "Umum";
    if (kstrcmp(de, "Netzwerk") == 0 && kstrcmp(en, "Network") == 0) return "Jaringan";
    if (kstrcmp(de, "Sicherheit") == 0 && kstrcmp(en, "Security") == 0) return "Keamanan";
    if (kstrcmp(de, "Expertenmodus") == 0 && kstrcmp(en, "Expert mode") == 0) return "Mode ahli";
    if (kstrcmp(de, "Treiber & System tief") == 0 && kstrcmp(en, "Drivers & low-level") == 0) return "Driver & tingkat rendah";
    if (kstrcmp(de, "Jetzt speichern") == 0 && kstrcmp(en, "Save now") == 0) return "Simpan sekarang";
    if (kstrcmp(de, "Einstellungen schliessen") == 0 && kstrcmp(en, "Close settings") == 0) return "Tutup pengaturan";
    if (kstrcmp(de, "Sprache") == 0 && kstrcmp(en, "Language") == 0) return "Bahasa";
    if (kstrcmp(de, "Tastatur-Layout") == 0 && kstrcmp(en, "Keyboard layout") == 0) return "Layout keyboard";
    if (kstrcmp(de, "KI-Hilfe") == 0 && kstrcmp(en, "AI help") == 0) return "Bantuan AI";
    if (kstrcmp(de, "Start-Hinweise") == 0 && kstrcmp(en, "Startup hints") == 0) return "Petunjuk awal";
    if (kstrcmp(de, "Zurueck") == 0 && kstrcmp(en, "Back") == 0) return "Kembali";
    if (kstrcmp(de, "Computername") == 0 && kstrcmp(en, "Computer name") == 0) return "Nama komputer";
    if (kstrcmp(de, "Programm-Freigaben") == 0 && kstrcmp(en, "Program approvals") == 0) return "Persetujuan program";
    if (kstrcmp(de, "Alte Wrapper ohne Manifest") == 0 && kstrcmp(en, "Legacy wrappers without manifest") == 0) return "Wrapper lama tanpa manifest";
    if (kstrcmp(de, "Neue lokale Befehle auto-freigeben") == 0 && kstrcmp(en, "Auto-approve new local commands") == 0) return "Setujui otomatis perintah lokal baru";
    if (kstrcmp(de, "Netzwerktreiber beim Start versuchen") == 0 && kstrcmp(en, "Try network driver at startup") == 0) return "Coba driver jaringan saat startup";
    if (kstrcmp(de, "Treiber-Diagnose erweitern") == 0 && kstrcmp(en, "Driver diagnostics") == 0) return "Diagnostik driver";
    if (kstrcmp(de, "Aenderungen automatisch speichern") == 0 && kstrcmp(en, "Auto-save changes") == 0) return "Simpan perubahan otomatis";
    if (kstrcmp(de, "Ausfuehrlichere Diagnose") == 0 && kstrcmp(en, "Verbose diagnostics") == 0) return "Diagnostik lebih rinci";
    if (kstrcmp(de, "streng") == 0 && kstrcmp(en, "strict") == 0) return "ketat";
    if (kstrcmp(de, "locker") == 0 && kstrcmp(en, "relaxed") == 0) return "longgar";
    if (kstrcmp(de, "verfuegbar") == 0 && kstrcmp(en, "available") == 0) return "tersedia";
    if (kstrcmp(de, "erst aktivieren") == 0 && kstrcmp(en, "enable first") == 0) return "aktifkan dulu";
    if (kstrcmp(de, "virtuelle Platte") == 0 && kstrcmp(en, "virtual disk") == 0) return "disk virtual";
    if (kstrcmp(de, "nur RAM") == 0 && kstrcmp(en, "RAM only") == 0) return "hanya RAM";
    if (kstrcmp(de, "zur Shell") == 0 && kstrcmp(en, "to shell") == 0) return "ke shell";
    if (kstrcmp(de, "smarter Helfer") == 0 && kstrcmp(en, "smarter helper") == 0) return "asisten lebih pintar";
    if (kstrcmp(de, "einfacher Helfer") == 0 && kstrcmp(en, "basic helper") == 0) return "asisten dasar";
    if (kstrcmp(de, "anzeigen") == 0 && kstrcmp(en, "show") == 0) return "tampilkan";
    if (kstrcmp(de, "ausblenden") == 0 && kstrcmp(en, "hide") == 0) return "sembunyikan";
    if (kstrcmp(de, "zur Uebersicht") == 0 && kstrcmp(en, "to overview") == 0) return "ke ringkasan";
    if (kstrcmp(de, "Pflicht") == 0 && kstrcmp(en, "required") == 0) return "wajib";
    if (kstrcmp(de, "optional lokal") == 0 && kstrcmp(en, "optional local") == 0) return "opsional lokal";
    if (kstrcmp(de, "erlaubt") == 0 && kstrcmp(en, "allowed") == 0) return "diizinkan";
    if (kstrcmp(de, "gesperrt") == 0 && kstrcmp(en, "blocked") == 0) return "diblokir";
    if (kstrcmp(de, "an") == 0 && kstrcmp(en, "on") == 0) return "nyala";
    if (kstrcmp(de, "mehr Details") == 0 && kstrcmp(en, "more details") == 0) return "lebih detail";
    if (kstrcmp(de, "ausfuehrlich") == 0 && kstrcmp(en, "verbose") == 0) return "rinci";
    if (kstrcmp(de, "normal") == 0 && kstrcmp(en, "normal") == 0) return "normal";
    if (kstrcmp(de, " - Uebersicht wie menuconfig, nur leichter.") == 0 && kstrcmp(en, " - overview like menuconfig, only easier.") == 0) return " - ringkasan seperti menuconfig, tapi lebih mudah.";
    if (kstrcmp(de, " - Pfeile waehlen, Enter aendert.") == 0 && kstrcmp(en, " - arrows select, Enter changes.") == 0) return " - panah memilih, Enter mengubah.";
    if (kstrcmp(de, " Sprache=") == 0 && kstrcmp(en, " Language=") == 0) return " Bahasa=";
    if (kstrcmp(de, "Deutsch") == 0 && kstrcmp(en, "German") == 0) return "Jerman";
    if (kstrcmp(de, "  Layout=") == 0 && kstrcmp(en, "  Layout=") == 0) return "  Layout=";
    if (kstrcmp(de, "  KI=") == 0 && kstrcmp(en, "  AI=") == 0) return "  AI=";
    if (kstrcmp(de, "einfach") == 0 && kstrcmp(en, "basic") == 0) return "dasar";
    if (kstrcmp(de, "  Expert=") == 0 && kstrcmp(en, "  Expert=") == 0) return "  Ahli=";
    if (kstrcmp(de, " Steuerung: Pfeil hoch/runter = waehlen, Enter = aendern, q oder Ctrl+C = zurueck.") == 0 && kstrcmp(en, " Controls: Up/Down = select, Enter = change, q or Ctrl+C = back.") == 0) return " Kontrol: panah atas/bawah = pilih, Enter = ubah, q atau Ctrl+C = kembali.";
    if (kstrcmp(de, " Meldung: ") == 0 && kstrcmp(en, " Message: ") == 0) return " Pesan: ";
    if (kstrcmp(de, " Meldung: bereit.") == 0 && kstrcmp(en, " Message: ready.") == 0) return " Pesan: siap.";
    if (kstrcmp(de, " Feld: ") == 0 && kstrcmp(en, " Field: ") == 0) return " Kolom: ";
    if (kstrcmp(de, " Gib den neuen Wert ein und druecke Enter. Ctrl+C verwirft die Aenderung.") == 0 && kstrcmp(en, " Enter the new value and press Enter. Ctrl+C discards the change.") == 0) return " Masukkan nilai baru lalu tekan Enter. Ctrl+C membatalkan perubahan.";
    if (kstrcmp(de, " Neuer Wert: ") == 0 && kstrcmp(en, " New value: ") == 0) return " Nilai baru: ";
    if (kstrcmp(de, "Dafuer brauchst du System-Rechte. Nutze elevate cyralith.") == 0 && kstrcmp(en, "You need system rights for this. Use elevate cyralith.") == 0) return "Kamu butuh hak sistem untuk ini. Gunakan elevate cyralith.";
    if (kstrcmp(de, "Leerer Wert wurde verworfen.") == 0 && kstrcmp(en, "Empty value was ignored.") == 0) return "Nilai kosong diabaikan.";
    if (kstrcmp(de, "Computername aktualisiert.") == 0 && kstrcmp(en, "Computer name updated.") == 0) return "Nama komputer diperbarui.";
    if (kstrcmp(de, "Computername konnte nicht gesetzt werden.") == 0 && kstrcmp(en, "Could not update computer name.") == 0) return "Nama komputer tidak bisa diperbarui.";
    if (kstrcmp(de, "IP-Adresse aktualisiert.") == 0 && kstrcmp(en, "IP address updated.") == 0) return "Alamat IP diperbarui.";
    if (kstrcmp(de, "IP-Adresse konnte nicht gesetzt werden.") == 0 && kstrcmp(en, "Could not update IP address.") == 0) return "Alamat IP tidak bisa diperbarui.";
    if (kstrcmp(de, "Gateway aktualisiert.") == 0 && kstrcmp(en, "Gateway updated.") == 0) return "Gateway diperbarui.";
    if (kstrcmp(de, "Gateway konnte nicht gesetzt werden.") == 0 && kstrcmp(en, "Could not update gateway.") == 0) return "Gateway tidak bisa diperbarui.";
    if (kstrcmp(de, "Expertenmodus aktiviert.") == 0 && kstrcmp(en, "Expert mode enabled.") == 0) return "Mode ahli diaktifkan.";
    if (kstrcmp(de, "Expertenmodus deaktiviert.") == 0 && kstrcmp(en, "Expert mode disabled.") == 0) return "Mode ahli dinonaktifkan.";
    if (kstrcmp(de, "Schalte zuerst den Expertenmodus ein.") == 0 && kstrcmp(en, "Enable expert mode first.") == 0) return "Aktifkan mode ahli terlebih dahulu.";
    if (kstrcmp(de, "Einstellungen gespeichert.") == 0 && kstrcmp(en, "Settings saved.") == 0) return "Pengaturan disimpan.";
    if (kstrcmp(de, "Speichern fehlgeschlagen. Wahrscheinlich keine virtuelle Platte.") == 0 && kstrcmp(en, "Save failed. Probably no virtual disk attached.") == 0) return "Gagal menyimpan. Mungkin tidak ada disk virtual terpasang.";
    if (kstrcmp(de, "Sprache umgeschaltet.") == 0 && kstrcmp(en, "Language switched.") == 0) return "Bahasa diganti.";
    if (kstrcmp(de, "Tastatur-Layout umgeschaltet.") == 0 && kstrcmp(en, "Keyboard layout switched.") == 0) return "Layout keyboard diganti.";
    if (kstrcmp(de, "KI-Hilfe umgeschaltet.") == 0 && kstrcmp(en, "AI help switched.") == 0) return "Bantuan AI diganti.";
    if (kstrcmp(de, "Start-Hinweise umgeschaltet.") == 0 && kstrcmp(en, "Startup hints switched.") == 0) return "Petunjuk awal diganti.";
    if (kstrcmp(de, "DHCP umgeschaltet.") == 0 && kstrcmp(en, "DHCP switched.") == 0) return "DHCP diganti.";
    if (kstrcmp(de, "DHCP konnte nicht geaendert werden.") == 0 && kstrcmp(en, "Could not change DHCP.") == 0) return "DHCP tidak bisa diubah.";
    if (kstrcmp(de, "Programm-Freigaben umgeschaltet.") == 0 && kstrcmp(en, "Program approvals switched.") == 0) return "Persetujuan program diganti.";
    if (kstrcmp(de, "Legacy-Befehle umgeschaltet.") == 0 && kstrcmp(en, "Legacy command policy switched.") == 0) return "Kebijakan perintah lama diganti.";
    if (kstrcmp(de, "Auto-Freigabe fuer neue lokale Befehle umgeschaltet.") == 0 && kstrcmp(en, "Auto-approval for new local commands switched.") == 0) return "Persetujuan otomatis untuk perintah lokal baru diganti.";
    if (kstrcmp(de, "Netzwerk-Autostart umgeschaltet.") == 0 && kstrcmp(en, "Network auto-start switched.") == 0) return "Autostart jaringan diganti.";
    if (kstrcmp(de, "Treiber-Diagnose umgeschaltet.") == 0 && kstrcmp(en, "Driver diagnostics switched.") == 0) return "Diagnostik driver diganti.";
    if (kstrcmp(de, "Auto-Speichern umgeschaltet.") == 0 && kstrcmp(en, "Auto-save switched.") == 0) return "Simpan otomatis diganti.";
    if (kstrcmp(de, "Ausfuehrliche Diagnose umgeschaltet.") == 0 && kstrcmp(en, "Verbose diagnostics switched.") == 0) return "Diagnostik rinci diganti.";
    if (kstrcmp(de, "Bereit. Waehle einen Bereich aus.") == 0 && kstrcmp(en, "Ready. Pick a category.") == 0) return "Siap. Pilih kategori.";
    if (kstrcmp(de, "Aenderung verworfen.") == 0 && kstrcmp(en, "Change discarded.") == 0) return "Perubahan dibatalkan.";
    if (kstrcmp(de, "Cyralith Hilfe") == 0 && kstrcmp(en, "Cyralith help") == 0) return "Bantuan Cyralith";
    if (kstrcmp(de, "Die wichtigsten Befehle:") == 0 && kstrcmp(en, "The most useful commands:") == 0) return "Perintah paling penting:";
    if (kstrcmp(de, "  help                 - Zeigt diese Hilfe.") == 0 && kstrcmp(en, "  help                 - Shows this help.") == 0) return "  help                 - Menampilkan bantuan ini.";
    if (kstrcmp(de, "  quickstart           - Einfache erste Schritte.") == 0 && kstrcmp(en, "  quickstart           - Simple first steps.") == 0) return "  quickstart           - Langkah awal sederhana.";
    if (kstrcmp(de, "  status               - Zeigt den aktuellen Zustand.") == 0 && kstrcmp(en, "  status               - Shows the current status.") == 0) return "  status               - Menampilkan status saat ini.";
    if (kstrcmp(de, "  apps                 - Zeigt die wichtigsten Teile des Systems.") == 0 && kstrcmp(en, "  apps                 - Shows the main parts of the system.") == 0) return "  apps                 - Menampilkan bagian utama sistem.";
    if (kstrcmp(de, "  tasks                - Zeigt laufende und gestoppte Bereiche.") == 0 && kstrcmp(en, "  tasks                - Shows running and stopped areas.") == 0) return "  tasks                - Menampilkan area yang berjalan dan berhenti.";
    if (kstrcmp(de, "  jobs                 - Zeigt geplante Automations-Jobs.") == 0 && kstrcmp(en, "  jobs                 - Shows scheduled automation jobs.") == 0) return "  jobs                 - Menampilkan job otomasi terjadwal.";
    if (kstrcmp(de, "  ps                   - Zeigt das Prozessmodell mit PID und Status.") == 0 && kstrcmp(en, "  ps                   - Shows the process model with PID and state.") == 0) return "  ps                   - Menampilkan model proses dengan PID dan status.";
    if (kstrcmp(de, "  proc ...             - Startet, stoppt oder beendet Prozesse.") == 0 && kstrcmp(en, "  proc ...             - Starts, stops or ends processes.") == 0) return "  proc ...             - Memulai, menghentikan, atau mengakhiri proses.";
    if (kstrcmp(de, "  actionlog            - Zeigt erklaerbare Systemaktionen.") == 0 && kstrcmp(en, "  actionlog            - Shows explainable system actions.") == 0) return "  actionlog            - Menampilkan aksi sistem yang dapat dijelaskan.";
    if (kstrcmp(de, "  memory               - Zeigt freien und belegten Speicher.") == 0 && kstrcmp(en, "  memory               - Shows free and used memory.") == 0) return "  memory               - Menampilkan memori kosong dan terpakai.";
    if (kstrcmp(de, "  paging               - Zeigt Paging, Frames und Page-Faults.") == 0 && kstrcmp(en, "  paging               - Shows paging, frames and page faults.") == 0) return "  paging               - Menampilkan paging, frame, dan page fault.";
    if (kstrcmp(de, "  history              - Zeigt deine letzten Befehle.") == 0 && kstrcmp(en, "  history              - Shows your recent commands.") == 0) return "  history              - Menampilkan perintah terakhirmu.";
    if (kstrcmp(de, "  settings             - Oeffnet die neue Einstellungszentrale mit Kategorien und Expertenmodus.") == 0 && kstrcmp(en, "  settings             - Opens the new settings center with categories and expert mode.") == 0) return "  settings             - Membuka pusat pengaturan baru dengan kategori dan mode ahli.";
    if (kstrcmp(de, "  whoami               - Zeigt Benutzername, Rolle und Home-Ordner.") == 0 && kstrcmp(en, "  whoami               - Shows user, role and home folder.") == 0) return "  whoami               - Menampilkan pengguna, peran, dan folder home.";
    if (kstrcmp(de, "  users                - Zeigt bekannte Benutzer, Gruppen und den System-Modus.") == 0 && kstrcmp(en, "  users                - Shows known users, groups and system mode.") == 0) return "  users                - Menampilkan pengguna, grup, dan mode sistem yang dikenal.";
    if (kstrcmp(de, "  login <n> [pw]       - Meldet einen normalen Benutzer an.") == 0 && kstrcmp(en, "  login <n> [pw]       - Signs in a regular user.") == 0) return "  login <n> [pw]       - Masuk sebagai pengguna biasa.";
    if (kstrcmp(de, "  elevate <pw>         - Aktiviert den root-aehnlichen System-Modus.") == 0 && kstrcmp(en, "  elevate <pw>         - Activates the root-like system mode.") == 0) return "  elevate <pw>         - Mengaktifkan mode sistem mirip root.";
    if (kstrcmp(de, "  drop                 - Schaltet den System-Modus wieder aus.") == 0 && kstrcmp(en, "  drop                 - Turns system mode off again.") == 0) return "  drop                 - Mematikan mode sistem lagi.";
    if (kstrcmp(de, "  passwd <neu>         - Aendert dein Passwort.") == 0 && kstrcmp(en, "  passwd <new>         - Changes your password.") == 0) return "  passwd <baru>        - Mengubah kata sandimu.";
    if (kstrcmp(de, "  pwd                  - Zeigt deinen aktuellen Ort.") == 0 && kstrcmp(en, "  pwd                  - Shows your current location.") == 0) return "  pwd                  - Menampilkan lokasi saat ini.";
    if (kstrcmp(de, "Mehr Uebersicht:") == 0 && kstrcmp(en, "More overview:") == 0) return "Ringkasan tambahan:";
    if (kstrcmp(de, "  Pfeil hoch / runter  - Springt durch alte Befehle.") == 0 && kstrcmp(en, "  Up / Down arrows     - Browse older commands.") == 0) return "  Panah atas / bawah   - Menelusuri perintah lama.";
    if (kstrcmp(de, "  Bild auf / ab        - Scrollt durch den Bildschirm-Verlauf.") == 0 && kstrcmp(en, "  Page Up / Down       - Scrolls through screen history.") == 0) return "  Page Up / Down       - Menggulir riwayat layar.";
    if (kstrcmp(de, "  Ctrl+C               - Bricht die aktuelle Eingabe ab.") == 0 && kstrcmp(en, "  Ctrl+C               - Cancels the current input.") == 0) return "  Ctrl+C               - Membatalkan input saat ini.";
    if (kstrcmp(de, "  In Lumen             - Ctrl+S speichert, Ctrl+Q beendet, .help zeigt Hilfe.") == 0 && kstrcmp(en, "  In Lumen             - Ctrl+S saves, Ctrl+Q quits, .help shows help.") == 0) return "  Di Lumen             - Ctrl+S menyimpan, Ctrl+Q keluar, .help menampilkan bantuan.";
    if (kstrcmp(de, "  Externe Programme    - Bekommen nur die Rechte aus ihrem Manifest.") == 0 && kstrcmp(en, "  External programs    - Only get the permissions from their manifest.") == 0) return "  Program eksternal    - Hanya mendapat izin dari manifestnya.";
    if (kstrcmp(de, "Cyralith ist ein eigenes, noch junges Betriebssystem.") == 0 && kstrcmp(en, "Cyralith is its own young operating system.") == 0) return "Cyralith adalah sistem operasi miliknya sendiri yang masih muda.";
    if (kstrcmp(de, "Die Idee: leicht zu bedienen wie Windows und anpassbar wie Linux.") == 0 && kstrcmp(en, "The idea: easy to use like Windows and customizable like Linux.") == 0) return "Idenya: mudah dipakai seperti Windows dan fleksibel seperti Linux.";
    if (kstrcmp(de, "Der System-Modus ist root-aehnlich: kein normaler Nutzer, sondern eine Rechte-Erweiterung.") == 0 && kstrcmp(en, "System mode is root-like: not a normal user, but a privilege elevation.") == 0) return "Mode sistem mirip root: bukan pengguna biasa, melainkan peningkatan hak akses.";
    if (kstrcmp(de, "Programmiert von Obsidian.") == 0 && kstrcmp(en, "Programmiert von Obsidian.") == 0) return "Diprogram oleh Obsidian.";
    if (kstrcmp(de, "Desktop-Platzhalter geoeffnet.") == 0 && kstrcmp(en, "Desktop placeholder opened.") == 0) return "Placeholder desktop dibuka.";
    if (kstrcmp(de, "Nutze: open <settings|desktop|network|files|monitor>") == 0 && kstrcmp(en, "Use: open <settings|desktop|network|files|monitor>") == 0) return "Gunakan: open <settings|desktop|network|files|monitor>";
    if (kstrcmp(de, "Bildschirm geleert.") == 0 && kstrcmp(en, "Screen cleared.") == 0) return "Layar dibersihkan.";
    return en;
}

static const char* tr(const char* de, const char* en) {
    if (g_language == LANG_DE) return de;
    if (g_language == LANG_ID) return translate_id(de, en);
    return en;
}

static const char* tr3(const char* de, const char* en, const char* id) {
    switch (g_language) {
        case LANG_DE: return de;
        case LANG_ID: return id;
        case LANG_EN:
        default: return en;
    }
}

static void log_action_simple(const char* action, const char* detail, actionlog_result_t result) {
    syslog_level_t level = SYSLOG_INFO;
    actionlog_add(user_current()->username, action, detail, result);
    switch (result) {
        case ACTIONLOG_WARN: level = SYSLOG_WARN; break;
        case ACTIONLOG_DENY: level = SYSLOG_WARN; break;
        case ACTIONLOG_FAIL: level = SYSLOG_ERROR; break;
        case ACTIONLOG_OK:
        default:
            level = SYSLOG_INFO;
            break;
    }
    syslog_write(level, action != (const char*)0 ? action : "action", detail != (const char*)0 ? detail : "");
}

static int can_manage_process_entry(const process_t* proc) {
    if (proc == (const process_t*)0) {
        return 0;
    }
    if (user_is_master() != 0) {
        return 1;
    }
    return kstrcmp(proc->owner, user_current()->username) == 0 ? 1 : 0;
}

static unsigned int process_session_start(process_kind_t kind, const char* name, const char* command) {
    unsigned int pid = 0U;
    if (process_spawn(kind, name, command, user_current()->username, 2U, 2U, &pid) == 0) {
        (void)process_activate(pid);
        return pid;
    }
    return 0U;
}

static void process_session_finish(unsigned int pid, int exit_code) {
    if (pid == 0U) {
        return;
    }
    (void)process_exit(pid, exit_code);
    (void)process_activate(1U);
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
    console_write("cyralith(");
    console_write(language_code());
    console_write(")@");
    console_write(user_current()->username);
    console_write(suffix);
    console_write(user_is_master() != 0 ? "# " : "> ");
}

void shell_show_welcome(void) {
    console_writeln("============================================================");
    console_writeln("Cyralith Stable Build 2.7.0");
    console_writeln(tr3("Danke, dass du Cyralith nutzt!", "Thank you for using Cyralith!", "Terima kasih sudah menggunakan Cyralith!"));
    console_writeln(tr3("Tippe 'help' fuer Hilfe.", "Type 'help' for help.", "Ketik 'help' untuk bantuan."));
    console_writeln(tr3("Programmiert von Obsidian.", "Programmed by Obsidian.", "Diprogram oleh Obsidian."));
    console_writeln("============================================================");
}

static void print_apps(void) {
    size_t i;
    console_writeln(tr("Apps und Bereiche:", "Apps and areas:"));
    console_writeln(tr("  system    - Shell, Benutzer, CyralithFS und Grunddienste.", "  system    - Shell, users, CyralithFS and core services."));
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

static void command_games(void) {
    size_t i;
    console_writeln(tr3("Spiele in Cyralith:", "Games in Cyralith:", "Game di Cyralith:"));
    for (i = 0U; i < arcade_game_count(); ++i) {
        const arcade_game_info_t* game = arcade_game_get(i);
        if (game == (const arcade_game_info_t*)0) {
            continue;
        }
        console_write("  ");
        console_write(game->name);
        if (game->available != 0U) {
            console_write(tr3("  [spielbar]  Best: ", "  [playable]  Best: ", "  [bisa dimainkan]  Best: "));
            console_write_dec(game->best_score);
            console_write(tr3("  Plays: ", "  Plays: ", "  Main: "));
            console_write_dec(game->plays);
            console_putc('\n');
        } else {
            console_write(tr3("  [geplant]   Arcade-Vorbereitung aktiv", "  [planned]   arcade groundwork active", "  [direncanakan] dasar arcade aktif"));
            console_putc('\n');
        }
    }
    console_writeln(tr3("  snake               - Startet das eingebaute Snake-Spiel.", "  snake               - Starts the built-in Snake game.", "  snake               - Menjalankan game Snake bawaan."));
    console_writeln(tr3("  game stats snake    - Zeigt Score-, Highscore- und Session-Daten.", "  game stats snake    - Shows score, high score, and session data.", "  game stats snake    - Menampilkan skor, high score, dan data sesi."));
    console_writeln(tr3("  app run snake       - Startet Snake ueber die App-Liste.", "  app run snake       - Starts Snake through the app list.", "  app run snake       - Menjalankan Snake lewat daftar aplikasi."));
    console_writeln(tr3("  Steuerung: Pfeile oder WASD, q zum Beenden.", "  Controls: arrows or WASD, q to quit.", "  Kontrol: panah atau WASD, q untuk keluar."));
}

static void command_game(const char* args) {
    char action[16];
    char name[24];
    const arcade_game_info_t* info;
    snake_stats_t stats;

    if (split_first_arg(args, action, sizeof(action)) != 0) {
        command_games();
        return;
    }

    if (kstrcmp(action, "stats") == 0) {
        const char* rest = args;
        while (*rest != '\0' && *rest != ' ') {
            rest++;
        }
        while (*rest == ' ') {
            rest++;
        }
        if (split_first_arg(rest, name, sizeof(name)) != 0) {
            console_writeln(tr3("Nutze: game stats <name>", "Use: game stats <name>", "Pakai: game stats <nama>"));
            return;
        }
        info = arcade_game_find(name);
        if (info == (const arcade_game_info_t*)0) {
            console_writeln(tr3("Unbekanntes Spiel.", "Unknown game.", "Game tidak dikenal."));
            return;
        }
        console_write(tr3("Spiel: ", "Game: ", "Game: "));
        console_writeln(info->name);
        console_write(tr3("  Status: ", "  State: ", "  Status: "));
        console_writeln(info->available != 0U ? tr3("spielbar", "playable", "bisa dimainkan") : tr3("geplant", "planned", "direncanakan"));
        console_write(tr3("  Plays: ", "  Plays: ", "  Main: "));
        console_write_dec(info->plays);
        console_putc('\n');
        console_write(tr3("  Highscore: ", "  High score: ", "  High score: "));
        console_write_dec(info->best_score);
        console_write(tr3("  | Letzter Score: ", "  | Last score: ", "  | Skor terakhir: "));
        console_write_dec(info->last_score);
        console_putc('\n');
        console_write(tr3("  Beste Laenge: ", "  Best length: ", "  Panjang terbaik: "));
        console_write_dec(info->best_length);
        console_write(tr3("  | Letzte Laenge: ", "  | Last length: ", "  | Panjang terakhir: "));
        console_write_dec(info->last_length);
        console_putc('\n');
        console_write(tr3("  Beste Aepfel: ", "  Best apples: ", "  Apel terbaik: "));
        console_write_dec(info->best_apples);
        console_write(tr3("  | Letzte Aepfel: ", "  | Last apples: ", "  | Apel terakhir: "));
        console_write_dec(info->last_apples);
        console_putc('\n');
        console_write(tr3("  Bestes Level: ", "  Best level: ", "  Level terbaik: "));
        console_write_dec(info->best_level);
        console_write(tr3("  | Letztes Level: ", "  | Last level: ", "  | Level terakhir: "));
        console_write_dec(info->last_level);
        console_putc('\n');
        if (kstrcmp(name, "snake") == 0) {
            snake_get_stats(&stats);
            console_write(tr3("  Aktuelle Session Score: ", "  Current session score: ", "  Skor sesi saat ini: "));
            console_write_dec(stats.score);
            console_write(tr3("  | Level: ", "  | Level: ", "  | Level: "));
            console_write_dec(stats.level);
            console_putc('\n');
        }
        return;
    }

    if (kstrcmp(action, "snake") == 0) {
        snake_open();
        return;
    }

    console_writeln(tr3("Nutze: game stats <name> oder snake", "Use: game stats <name> or snake", "Pakai: game stats <nama> atau snake"));
}

static void command_netdrivers(void) {
    console_writeln(tr3("Netzwerktreiber in Cyralith:", "Network drivers in Cyralith:", "Driver jaringan di Cyralith:"));
    console_writeln(tr3("  intel    - Intel PRO/1000 compatibility, Status + MAC, vorsichtiger Modus.", "  intel    - Intel PRO/1000 compatibility, status + MAC, careful mode.", "  intel    - Kompatibilitas Intel PRO/1000, status + MAC, mode hati-hati."));
    console_writeln(tr3("  e1000    - Intel e1000 pilot, experimentellerer Pfad mit netprobe.", "  e1000    - Intel e1000 pilot, more experimental path with netprobe.", "  e1000    - Pilot Intel e1000, jalur lebih eksperimental dengan netprobe."));
    console_writeln(tr3("  pcnet    - AMD PCnet safe attach, gut fuer viele VirtualBox-Setups.", "  pcnet    - AMD PCnet safe attach, good for many VirtualBox setups.", "  pcnet    - Safe attach AMD PCnet, cocok untuk banyak setup VirtualBox."));
    console_writeln(tr3("  rtl8139  - Realtek RTL8139 safe attach, leichtgewichtiger Fallback.", "  rtl8139  - Realtek RTL8139 safe attach, lightweight fallback.", "  rtl8139  - Safe attach Realtek RTL8139, fallback ringan."));
    console_writeln(tr3("  virtio   - Virtio preview attach, Erkennung zuerst, Datapfad spaeter.", "  virtio   - Virtio preview attach, detection first, datapath later.", "  virtio   - Preview attach Virtio, deteksi dulu, datapath nanti."));
    console_writeln(tr3("Nutze 'netdriver <name>' und danach 'netup'.", "Use 'netdriver <name>' and then 'netup'.", "Pakai 'netdriver <nama>' lalu 'netup'."));
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

static void print_paging_status(void) {
    paging_status_t status;
    paging_get_status(&status);
    console_writeln(tr("Paging / virtueller Speicher:", "Paging / virtual memory:"));
    console_write(tr("  Aktiv: ", "  Enabled: "));
    console_writeln(status.enabled != 0U ? tr("ja", "yes") : tr("nein", "no"));
    console_write(tr("  Seitengroesse: ", "  Page size: "));
    console_write_dec(status.page_size);
    console_writeln(" bytes");
    console_write(tr("  Frames gesamt: ", "  Total frames: "));
    console_write_dec(status.total_frames);
    console_putc('\n');
    console_write(tr("  Reserviert: ", "  Reserved: "));
    console_write_dec(status.reserved_frames);
    console_write(tr(" | Benutzt: ", " | Used: "));
    console_write_dec(status.used_frames);
    console_write(tr(" | Frei: ", " | Free: "));
    console_write_dec(status.free_frames);
    console_putc('\n');
    console_write(tr("  Page Directory: ", "  Page directory: "));
    console_write_hex(paging_directory_address());
    console_putc('\n');
    console_write(tr("  Page Faults: ", "  Page faults: "));
    console_write_dec(status.fault_count);
    if (status.fault_count != 0U) {
        console_write(tr(" | Letzte Adresse: ", " | Last address: "));
        console_write_hex(status.last_fault_address);
        console_write(tr(" | Fehlercode: ", " | Error code: "));
        console_write_hex(status.last_fault_error);
    }
    console_putc('\n');
}

static void print_tasks(void) {
    console_writeln(tr("Aktive Systembereiche:", "Active system areas:"));
    console_write(tr("  Scheduler: ", "  Scheduler: "));
    console_write(task_scheduler_name());
    console_write(tr(" | ticks=", " | ticks="));
    console_write_dec(task_scheduler_ticks());
    console_putc('\n');
    if (task_current() != (const task_t*)0) {
        console_write(tr("  Aktuell: ", "  Current: "));
        console_writeln(task_current()->name);
    }
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
        console_write(task->description);
        console_write(" | runtime=");
        console_write_dec(task->runtime_ticks);
        console_write(" | switches=");
        console_write_dec(task->switches);
        console_putc('\n');
    }
}

static void print_processes(void) {
    console_writeln(tr("Prozesse:", "Processes:"));
    if (process_count() == 0U) {
        console_writeln(tr("  Keine Prozesse registriert.", "  No processes registered."));
        return;
    }
    for (size_t i = 0U; i < process_count(); ++i) {
        const process_t* proc = process_get(i);
        if (proc == (const process_t*)0) {
            continue;
        }
        console_write("  #");
        console_write_dec(proc->pid);
        console_write(" ");
        console_write(proc->name);
        console_write(" [");
        console_write(process_kind_name(proc->kind));
        console_write("/");
        console_write(process_state_name(proc->state));
        console_write("] owner=");
        console_write(proc->owner);
        console_write(" pages=");
        console_write_dec(proc->mapped_page_count);
        console_write(" runtime=");
        console_write_dec(proc->runtime_ticks);
        if (process_current() == proc) {
            console_write(" *current");
        }
        console_putc('\n');
    }
}

static void print_jobs(void) {
    console_writeln(tr("Geplante Aufgaben:", "Scheduled jobs:"));
    if (automation_count() == 0U) {
        console_writeln(tr("  Keine aktiven Jobs.", "  No active jobs."));
        return;
    }
    for (size_t i = 0U; i < automation_count(); ++i) {
        const automation_job_t* job = automation_get(i);
        if (job == (const automation_job_t*)0) {
            continue;
        }
        console_write("  #");
        console_write_dec(job->id);
        console_write(" due=");
        console_write_dec(job->due_tick);
        console_write(" cmd=");
        console_writeln(job->command);
    }
}

static void print_action_log(void) {
    console_writeln(tr("Letzte erklaerbare Aktionen:", "Recent explainable actions:"));
    if (actionlog_count() == 0U) {
        console_writeln(tr("  Noch keine Aktionen protokolliert.", "  No logged actions yet."));
        return;
    }
    for (size_t i = 0U; i < actionlog_count(); ++i) {
        const actionlog_entry_t* entry = actionlog_get_recent(i);
        if (entry == (const actionlog_entry_t*)0) {
            continue;
        }
        console_write("  [t=");
        console_write_dec(entry->tick);
        console_write("] ");
        console_write(entry->actor);
        console_write(": ");
        console_write(entry->action);
        console_write(" -> ");
        console_write(entry->detail);
        console_write(" [");
        console_write(actionlog_result_name(entry->result));
        console_writeln("]");
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

static void print_signed_dec(int value) {
    if (value < 0) {
        console_putc('-');
        console_write_dec((uint32_t)(-value));
        return;
    }
    console_write_dec((uint32_t)value);
}

static const char* current_language_code(void) {
    if (g_language == LANG_EN) {
        return "en";
    }
    if (g_language == LANG_ID) {
        return "id";
    }
    return "de";
}

static const char* keytest_name(int key) {
    switch (key) {
        case KEY_NONE: return "none";
        case KEY_ENTER: return "enter";
        case KEY_BACKSPACE: return "backspace";
        case KEY_UP: return "up";
        case KEY_DOWN: return "down";
        case KEY_LEFT: return "left";
        case KEY_RIGHT: return "right";
        case KEY_PAGEUP: return "pageup";
        case KEY_PAGEDOWN: return "pagedown";
        case KEY_CTRL_C: return "ctrl+c";
        case KEY_CTRL_S: return "ctrl+s";
        case KEY_CTRL_Q: return "ctrl+q";
        case KEY_CTRL_F: return "ctrl+f";
        case KEY_CTRL_G: return "ctrl+g";
        default: return "char";
    }
}

static void keytest_open(void) {
    g_keytest_active = 1;
    g_keytest_count = 0U;
    console_clear();
    console_writeln(tr3("Cyralith Keytest", "Cyralith keytest", "Keytest Cyralith"));
    console_writeln(tr3("Druecke Tasten. Mit q oder Ctrl+Q beenden.", "Press keys. Leave with q or Ctrl+Q.", "Tekan tombol. Keluar dengan q atau Ctrl+Q."));
    console_writeln(tr3("Diese Ansicht hilft bei Layout- und Editor-Problemen.", "This view helps with layout and editor problems.", "Tampilan ini membantu untuk masalah layout dan editor."));
    console_writeln("----------------------------------------------------------------");
}

static void keytest_close(void) {
    g_keytest_active = 0;
    console_writeln(tr3("Keytest beendet.", "Keytest closed.", "Keytest ditutup."));
}

static void keytest_handle(int key) {
    if (key == KEY_NONE) {
        return;
    }
    if (key == 'q' || key == 'Q' || key == KEY_CTRL_Q) {
        keytest_close();
        prompt();
        return;
    }
    g_keytest_count++;
    console_write("#");
    console_write_dec(g_keytest_count);
    console_write("  code=");
    print_signed_dec(key);
    console_write("  kind=");
    console_write(keytest_name(key));
    if (key > 31 && key < 127) {
        console_write("  text='");
        console_putc((char)key);
        console_write("'");
    }
    console_putc('\n');
}

static void build_home_path(const char* name, char* out, size_t max) {
    size_t pos = 0U;
    const user_t* current = user_current();
    if (max == 0U) {
        return;
    }
    out[0] = '\0';
    if (current == (const user_t*)0) {
        return;
    }
    while (current->home[pos] != '\0' && pos + 1U < max) {
        out[pos] = current->home[pos];
        pos++;
    }
    if (pos > 0U && out[pos - 1U] != '/' && pos + 1U < max) {
        out[pos++] = '/';
    }
    for (size_t i = 0U; name[i] != '\0' && pos + 1U < max; ++i) {
        out[pos++] = name[i];
    }
    out[pos] = '\0';
}

static void command_view(const char* arg) {
    while (*arg == ' ') { arg++; }
    if (*arg == '\0') {
        console_writeln(tr3("Nutze: view <datei>", "Use: view <file>", "Gunakan: view <file>"));
        return;
    }
    print_file_contents(arg);
}

static void command_todo(const char* arg) {
    char path[96];
    build_home_path("todo.txt", path, sizeof(path));
    while (*arg == ' ') { arg++; }
    if (*arg == '\0' || kstrcmp(arg, "edit") == 0 || kstrcmp(arg, "open") == 0) {
        editor_open(path);
        return;
    }
    if (kstrcmp(arg, "show") == 0 || kstrcmp(arg, "list") == 0) {
        print_file_contents(path);
        return;
    }
    if (kstarts_with(arg, "add ")) {
        if (afs_exists(path) == 0) {
            (void)afs_touch(path);
        }
        if (afs_append_file(path, "- ") != AFS_OK || afs_append_file(path, arg + 4) != AFS_OK || afs_append_file(path, "\n") != AFS_OK) {
            console_writeln(tr3("Todo konnte nicht gespeichert werden.", "Could not save todo item.", "Tidak bisa menyimpan todo."));
        } else {
            console_writeln(tr3("Todo gespeichert.", "Todo saved.", "Todo disimpan."));
        }
        return;
    }
    console_writeln(tr3("Nutze: todo [show|add <text>|edit]", "Use: todo [show|add <text>|edit]", "Gunakan: todo [show|add <teks>|edit]"));
}

static const char* calc_skip_spaces(const char* s) {
    while (*s == ' ') {
        s++;
    }
    return s;
}

static int calc_parse_value(const char** sp, int* out) {
    const char* s = calc_skip_spaces(*sp);
    int sign = 1;
    int seen = 0;
    int value = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = (value * 10) + (*s - '0');
        s++;
        seen = 1;
    }
    if (seen == 0) {
        return -1;
    }
    *out = value * sign;
    *sp = s;
    return 0;
}

static void command_calc(const char* expr) {
    const char* pexpr = expr;
    int left;
    int right;
    int result;
    char op;
    pexpr = calc_skip_spaces(pexpr);
    if (*pexpr == '\0') {
        console_writeln(tr3("Nutze: calc <zahl> <op> <zahl>", "Use: calc <number> <op> <number>", "Gunakan: calc <angka> <op> <angka>"));
        console_writeln("  +  -  *  /");
        return;
    }
    if (calc_parse_value(&pexpr, &left) != 0) {
        console_writeln(tr3("Calc: erste Zahl fehlt.", "Calc: missing first number.", "Calc: angka pertama hilang."));
        return;
    }
    pexpr = calc_skip_spaces(pexpr);
    op = *pexpr;
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        console_writeln(tr3("Calc: Operator muss +, -, * oder / sein.", "Calc: operator must be +, -, * or /.", "Calc: operator harus +, -, * atau /."));
        return;
    }
    pexpr++;
    if (calc_parse_value(&pexpr, &right) != 0) {
        console_writeln(tr3("Calc: zweite Zahl fehlt.", "Calc: missing second number.", "Calc: angka kedua hilang."));
        return;
    }
    pexpr = calc_skip_spaces(pexpr);
    if (*pexpr != '\0') {
        console_writeln(tr3("Calc: bitte nur zwei Zahlen und einen Operator eingeben.", "Calc: please enter only two numbers and one operator.", "Calc: masukkan hanya dua angka dan satu operator."));
        return;
    }
    if (op == '+') {
        result = left + right;
    } else if (op == '-') {
        result = left - right;
    } else if (op == '*') {
        result = left * right;
    } else {
        if (right == 0) {
            console_writeln(tr3("Calc: Division durch 0 ist nicht erlaubt.", "Calc: division by 0 is not allowed.", "Calc: pembagian dengan 0 tidak diizinkan."));
            return;
        }
        result = left / right;
    }
    print_signed_dec(left);
    console_write(" ");
    console_putc(op);
    console_write(" ");
    print_signed_dec(right);
    console_write(" = ");
    print_signed_dec(result);
}

static void print_working_path(void) {
    char path[96];
    afs_pwd(path, sizeof(path));
    console_write(tr("Aktueller Ort: ", "Current location: "));
    console_writeln(path);
}

static void print_disk_status(void) {
    console_writeln(tr("CyralithFS Speicher:", "CyralithFS storage:"));
    console_write(tr("  Backend: ", "  Backend: "));
    console_writeln(afs_persistence_name());
    if (afs_persistence_available() != 0) {
        console_writeln(tr("  Status: virtuelle ATA-Platte erkannt. savefs/loadfs sind aktiv.", "  Status: virtual ATA disk detected. savefs/loadfs are active."));
    } else {
        console_writeln(tr("  Status: keine virtuelle Platte erkannt. CyralithFS bleibt nur im RAM.", "  Status: no virtual disk detected. CyralithFS stays in RAM only."));
        console_writeln(tr("  Tipp: Fuege in VirtualBox oder QEMU eine kleine virtuelle Festplatte hinzu.", "  Tip: add a small virtual hard disk in VirtualBox or QEMU."));
    }
}

static void print_network_status(void) {
    console_writeln(tr("CyralithNet im Ueberblick:", "CyralithNet overview:"));
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
    console_write(tr3("  Auswahl: ", "  Selected: ", "  Dipilih: "));
    console_writeln(network_driver_preference_name());
    console_write(tr("  Verbindung: ", "  Link: "));
    console_writeln(network_driver_active() != 0 ? (network_link_up() != 0 ? tr("aktiv", "up") : tr("Treiber aktiv, Link unklar", "driver active, link not confirmed")) : tr("noch nicht gestartet", "not started yet"));
    console_write(tr("  MAC: ", "  MAC: "));
    console_writeln(network_mac_address());
    if (g_setting_expert_mode != 0) {
        console_write(tr("  Expertenmodus: ", "  Expert mode: "));
        console_writeln(g_setting_driver_debug != 0 ? tr("Treiberdiagnose aktiv", "driver diagnostics active") : tr("kompakt", "compact"));
    }
    console_writeln(tr3("  Tipp: In Settings > Netzwerk > Treiber oder per 'netdriver <name>' wechselst du den Treiber.", "  Tip: In Settings > Network > Driver or with 'netdriver <name>' you can switch drivers.", "  Tips: Di Settings > Jaringan > Driver atau dengan 'netdriver <nama>' kamu bisa mengganti driver."));
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
        console_writeln(tr3("  Noch kein Treiber aktiv. Nutze 'netdrivers', waehle mit 'netdriver <name>' und starte dann 'netup'.", "  No driver active yet. Use 'netdrivers', pick one with 'netdriver <name>' and then run 'netup'.", "  Belum ada driver aktif. Pakai 'netdrivers', pilih dengan 'netdriver <nama>' lalu jalankan 'netup'."));
    }
}


static void print_fs(void) {
    char path[96];
    afs_pwd(path, sizeof(path));
    console_writeln(tr("CyralithFS im Ueberblick:", "CyralithFS overview:"));
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
    console_writeln(tr("  Cyralith laeuft im stabilen Startmodus.", "  Cyralith is running in stable mode."));
    print_whoami();
    print_layout();
    console_write(tr("  Freier Speicher: ", "  Free memory: "));
    console_write_dec((uint32_t)kmem_free_bytes());
    console_writeln(" bytes");
    {
        paging_status_t paging;
        paging_get_status(&paging);
        console_write(tr("  Paging: ", "  Paging: "));
        console_write(paging.enabled != 0U ? tr("aktiv", "enabled") : tr("aus", "off"));
        console_write(tr(" / freie Frames=", " / free frames="));
        console_write_dec(paging.free_frames);
        console_putc('\n');
    }
    console_write(tr("  CyralithFS Ort: ", "  CyralithFS path: "));
    console_writeln(path);
    console_write(tr("  CyralithFS Speicher: ", "  CyralithFS storage: "));
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
    console_write(tr("  Geplante Jobs: ", "  Scheduled jobs: "));
    console_write_dec((uint32_t)automation_count());
    console_putc('\n');
    if (task_current() != (const task_t*)0) {
        console_write(tr("  Scheduler aktiv: ", "  Scheduler active: "));
        console_writeln(task_current()->name);
    }
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
    console_writeln(tr3("  8. netdriver e1000  - Waehlt den e1000-Treiber, danach 'netup'.", "  8. netdriver e1000  - Selects the e1000 driver, then run 'netup'.", "  8. netdriver e1000  - Pilih driver e1000, lalu jalankan 'netup'."));
    console_writeln(tr("  9. elevate cyralith    - Aktiviert den root-aehnlichen System-Modus.", "  9. elevate cyralith    - Activates the root-like system mode."));
    console_writeln(tr(" 10. savefs            - Speichert Dateien, Nutzer, Netzwerk und Apps auf die virtuelle Platte.", " 10. savefs            - Saves files, users, network and apps to the virtual disk."));
    console_writeln(tr(" 11. cmd new hallo     - Erstellt einen eigenen Befehl.", " 11. cmd new hallo     - Creates your own command."));
    console_writeln(tr(" 12. prog info hallo   - Zeigt Rechte, Vertrauen und Freigabe.", " 12. prog info hallo   - Shows permissions, trust and approval."));
    console_writeln(tr(" 13. which hallo       - Zeigt, woher ein Befehl kommt.", " 13. which hallo       - Shows where a command comes from."));
    console_writeln(tr(" 14. job add 10 status - Plant einen Befehl in 10 Sekunden.", " 14. job add 10 status - Schedules a command in 10 seconds."));
    console_writeln(tr(" 15. doctor            - Startet den Diagnose-Assistenten.", " 15. doctor            - Starts the diagnosis assistant."));
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

    if (kstrcmp(code, "id") == 0) {
        g_language = LANG_ID;
        console_writeln("Bahasa diatur: Indonesia");
        return;
    }

    console_writeln(tr3("Nutze: lang <de|en|id>", "Use: lang <de|en|id>", "Pakai: lang <de|en|id>"));
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
                case SETTINGS_NETWORK_DRIVER: return tr3("Treiber", "Driver", "Driver");
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
                    kstrcpy(out, language_label_full());
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
                    kstrcpy(out, language_label_short());
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
                case SETTINGS_NETWORK_DRIVER:
                    kstrcpy(out, network_driver_preference_name());
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
        return index == SETTINGS_NETWORK_HOSTNAME || index == SETTINGS_NETWORK_DHCP || index == SETTINGS_NETWORK_IP || index == SETTINGS_NETWORK_GATEWAY || index == SETTINGS_NETWORK_DRIVER;
    }
    if (view == SETTINGS_VIEW_SECURITY || view == SETTINGS_VIEW_EXPERT) {
        return 1;
    }
    return 0;
}

static void settings_set_notice(const char* de, const char* en) {
    kstrcpy(g_settings_notice, tr(de, en));
}

static void settings_box_line(const char* text) {
    size_t i;
    size_t len = kstrlen(text);
    console_write("| ");
    console_write(text);
    for (i = len; i < 76U; ++i) {
        console_putc(' ');
    }
    console_writeln(" |");
}

static void settings_draw_easter_egg(void) {
    settings_box_line("");
    settings_box_line(tr3("UI-Sprachen aktiv: Deutsch, English, Bahasa Indonesia.",
                          "UI languages active: German, English, Bahasa Indonesia.",
                          "Bahasa antarmuka aktif: Jerman, Inggris, Bahasa Indonesia."));
    settings_box_line(tr3("Tipp: Mit 'lang id' oder im Menue wechselst du die Shell-Sprache.",
                          "Tip: Use 'lang id' or the menu to switch the shell language.",
                          "Tips: Gunakan 'lang id' atau menu untuk mengganti bahasa shell."));
}


static void settings_draw_list(void) {
    int i;
    int count = settings_view_count(g_settings_view);
    console_clear();
    console_writeln("+==============================================================================+");
    console_writeln("|                              Cyralith Settings                               |");
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
    settings_draw_easter_egg();
    console_writeln("+------------------------------------------------------------------------------+");
    console_write(tr(" Sprache=", " Language="));
    console_write(language_label_short());
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
    console_writeln(tr3("|                        Cyralith Settings / Bearbeiten                         |", "|                         Cyralith Settings / Editing                          |", "|                       Pengaturan Cyralith / Edit Nilai                       |"));
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
        settings_set_notice("Dafuer brauchst du System-Rechte. Nutze elevate cyralith.", "You need system rights for this. Use elevate cyralith.");
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
                if (g_language == LANG_DE) {
                    set_language("en");
                } else if (g_language == LANG_EN) {
                    set_language("id");
                } else {
                    set_language("de");
                }
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
            case SETTINGS_NETWORK_DRIVER:
                rc = network_cycle_driver_preference();
                settings_set_notice(rc == 0 ? "Netzwerktreiber umgeschaltet." : "Netzwerktreiber konnte nicht geaendert werden.", rc == 0 ? "Network driver switched." : "Could not change network driver.");
                settings_draw_list();
                return;
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
    g_settings_anim_frame = 0U;
    g_settings_anim_idle = 0U;
    settings_set_notice("Bereit. Waehle einen Bereich aus.", "Ready. Pick a category.");
    settings_draw_list();
}

static void settings_handle_key(int key) {
    int count;
    if (g_settings_active == 0) {
        return;
    }
    g_settings_anim_idle = 0U;
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

static void settings_poll(void) {
    if (g_settings_active == 0 || g_settings_edit_field != SETTINGS_EDIT_NONE) {
        return;
    }
    g_settings_anim_idle++;
    if (g_settings_anim_idle < 120000U) {
        return;
    }
    g_settings_anim_idle = 0U;
    g_settings_anim_frame = (g_settings_anim_frame + 1U) % 6U;
    settings_draw_list();
}

static int needs_master(const char* action) {
    if (user_is_master() != 0) {
        return 0;
    }
    console_write(tr("Dafuer brauchst du System-Rechte. Nutze 'elevate <passwort>': ", "You need system rights for that. Use 'elevate <password>': "));
    console_writeln(action);
    return 1;
}


static void show_help_topic_unknown(const char* topic) {
    console_writeln(tr3("Unbekanntes Hilfe-Thema.", "Unknown help topic.", "Topik bantuan tidak dikenal."));
    console_write(tr3("Nutze: ", "Use: ", "Gunakan: "));
    console_writeln("help network | files | users | apps | games | processes | system | settings | all");
    if (topic != (const char*)0 && topic[0] != '\0') {
        console_write(tr3("Eingegeben: ", "Entered: ", "Yang dimasukkan: "));
        console_writeln(topic);
    }
}

static void show_help_network(void) {
    console_writeln(tr3("Hilfe: Netzwerk", "Help: network", "Bantuan: jaringan"));
    console_writeln(tr3("  network              - Zeigt Netzwerk-Status und aktive Werte.", "  network              - Shows network status and active values.", "  network              - Menampilkan status jaringan dan nilai aktif."));
    console_writeln(tr3("  ip                   - Zeigt die aktuelle IPv4-Adresse.", "  ip                   - Shows the current IPv4 address.", "  ip                   - Menampilkan alamat IPv4 saat ini."));
    console_writeln(tr3("  ip set <addr>        - Setzt die IPv4-Adresse im System-Modus.", "  ip set <addr>        - Sets the IPv4 address in system mode.", "  ip set <addr>        - Mengatur alamat IPv4 dalam mode sistem."));
    console_writeln(tr3("  gateway              - Zeigt das aktuelle Gateway.", "  gateway              - Shows the current gateway.", "  gateway              - Menampilkan gateway saat ini."));
    console_writeln(tr3("  gateway set <addr>   - Setzt das Gateway im System-Modus.", "  gateway set <addr>   - Sets the gateway in system mode.", "  gateway set <addr>   - Mengatur gateway dalam mode sistem."));
    console_writeln(tr3("  dhcp                 - Zeigt, ob DHCP aktiv ist.", "  dhcp                 - Shows whether DHCP is enabled.", "  dhcp                 - Menampilkan apakah DHCP aktif."));
    console_writeln(tr3("  dhcp on/off          - Schaltet DHCP im System-Modus um.", "  dhcp on/off          - Toggles DHCP in system mode.", "  dhcp on/off          - Mengubah DHCP dalam mode sistem."));
    console_writeln(tr3("  nic                  - Zeigt erkannte PCI-Netzwerkadapter.", "  nic                  - Shows detected PCI network adapters.", "  nic                  - Menampilkan adapter jaringan PCI yang terdeteksi."));
    console_writeln(tr3("  mac                  - Zeigt die MAC-Adresse des aktiven Treibers.", "  mac                  - Shows the MAC address of the active driver.", "  mac                  - Menampilkan alamat MAC driver aktif."));
    console_writeln(tr3("  netdrivers           - Zeigt alle verfuegbaren Treibermodi.", "  netdrivers           - Shows all available driver modes.", "  netdrivers           - Menampilkan semua mode driver yang tersedia."));
    console_writeln(tr3("  netdriver intel      - Waehlt den Intel-Kompatibilitaetsmodus.", "  netdriver intel      - Selects Intel compatibility mode.", "  netdriver intel      - Memilih mode kompatibilitas Intel."));
    console_writeln(tr3("  netdriver e1000      - Waehlt den e1000-Pilot-Treiber.", "  netdriver e1000      - Selects the e1000 pilot driver.", "  netdriver e1000      - Memilih driver pilot e1000."));
    console_writeln(tr3("  netdriver pcnet      - Waehlt den AMD-PCnet-Safe-Attach-Modus.", "  netdriver pcnet      - Selects the AMD PCnet safe-attach mode.", "  netdriver pcnet      - Memilih mode safe-attach AMD PCnet."));
    console_writeln(tr3("  netdriver rtl8139    - Waehlt den Realtek-RTL8139-Safe-Attach-Modus.", "  netdriver rtl8139    - Selects the Realtek RTL8139 safe-attach mode.", "  netdriver rtl8139    - Memilih mode safe-attach Realtek RTL8139."));
    console_writeln(tr3("  netdriver virtio     - Waehlt den Virtio-Vorschau-Modus.", "  netdriver virtio     - Selects the virtio preview mode.", "  netdriver virtio     - Memilih mode pratinjau virtio."));
    console_writeln(tr3("  netup                - Startet den ausgewaehlten Netzwerktreiber.", "  netup                - Starts the selected network driver.", "  netup                - Menyalakan driver jaringan yang dipilih."));
    console_writeln(tr3("  netprobe             - Testet Rohdaten mit e1000.", "  netprobe             - Tests raw packets with e1000.", "  netprobe             - Menguji paket mentah dengan e1000."));
    console_writeln(tr3("  ping <ziel>          - Testet Loopback und zeigt den Treiberstatus.", "  ping <target>        - Tests loopback and shows driver state.", "  ping <target>        - Menguji loopback dan menampilkan status driver."));
}

static void show_help_files(void) {
    console_writeln(tr3("Hilfe: Dateien und CyralithFS", "Help: files and CyralithFS", "Bantuan: file dan CyralithFS"));
    console_writeln(tr3("  pwd                  - Zeigt den aktuellen Ort.", "  pwd                  - Shows the current location.", "  pwd                  - Menampilkan lokasi saat ini."));
    console_writeln(tr3("  cd [pfad]            - Wechselt den Ordner.", "  cd [path]            - Changes the folder.", "  cd [path]            - Ganti folder."));
    console_writeln(tr3("  ls [pfad]            - Zeigt Dateien und Ordner.", "  ls [path]            - Shows files and folders.", "  ls [path]            - Menampilkan file dan folder."));
    console_writeln(tr3("  mkdir <name>         - Erstellt einen Ordner.", "  mkdir <name>         - Creates a folder.", "  mkdir <name>         - Membuat folder."));
    console_writeln(tr3("  touch <name>         - Erstellt eine leere Datei.", "  touch <name>         - Creates an empty file.", "  touch <name>         - Membuat file kosong."));
    console_writeln(tr3("  cat <datei>          - Zeigt den Inhalt einer Datei.", "  cat <file>           - Shows a file's contents.", "  cat <file>           - Menampilkan isi file."));
    console_writeln(tr3("  write <d> <text>     - Schreibt Text in eine Datei.", "  write <f> <text>     - Writes text into a file.", "  write <f> <text>     - Menulis teks ke file."));
    console_writeln(tr3("  append <d> <text>    - Haengt Text an eine Datei an.", "  append <f> <text>    - Appends text to a file.", "  append <f> <text>    - Menambah teks ke file."));
    console_writeln(tr3("  cp <von> <nach>      - Kopiert eine Datei oder einen Ordner.", "  cp <from> <to>       - Copies a file or folder.", "  cp <from> <to>       - Menyalin file atau folder."));
    console_writeln(tr3("  mv <von> <nach>      - Verschiebt oder benennt etwas um.", "  mv <from> <to>       - Moves or renames something.", "  mv <from> <to>       - Memindahkan atau mengganti nama."));
    console_writeln(tr3("  rm <pfad>            - Loescht eine Datei oder einen leeren Ordner.", "  rm <path>            - Deletes a file or empty folder.", "  rm <path>            - Menghapus file atau folder kosong."));
    console_writeln(tr3("  rm -r <pfad>         - Loescht einen Ordner mit Inhalt.", "  rm -r <path>         - Deletes a folder with contents.", "  rm -r <path>         - Menghapus folder beserta isinya."));
    console_writeln(tr3("  find <name>          - Sucht nach Dateien oder Ordnern.", "  find <name>          - Searches for files or folders.", "  find <name>          - Mencari file atau folder."));
    console_writeln(tr3("  stat <pfad>          - Zeigt Besitzer, Gruppe und Rechte.", "  stat <path>          - Shows owner, group and rights.", "  stat <path>          - Menampilkan pemilik, grup, dan hak akses."));
    console_writeln(tr3("  protect/chmod        - Veraendert Rechte einfach oder numerisch.", "  protect/chmod        - Changes rights simply or numerically.", "  protect/chmod        - Mengubah izin secara sederhana atau numerik."));
    console_writeln(tr3("  savefs / loadfs      - Speichert oder laedt den Dateistand.", "  savefs / loadfs      - Saves or loads the file state.", "  savefs / loadfs      - Menyimpan atau memuat keadaan file."));
    console_writeln(tr3("  disk                 - Zeigt Persistenz und virtuelle Platte.", "  disk                 - Shows persistence and virtual disk.", "  disk                 - Menampilkan persistensi dan disk virtual."));
    console_writeln(tr3("  edit <name>          - Oeffnet Lumen.", "  edit <name>          - Opens Lumen.", "  edit <name>          - Membuka Lumen."));
    console_writeln(tr3("  notes                - Zeigt offene Lumen-Dateien.", "  notes                - Shows open Lumen files.", "  notes                - Menampilkan file Lumen yang terbuka."));
    console_writeln(tr3("  view <datei>         - Zeigt eine Datei ohne Editor.", "  view <file>          - Shows a file without opening the editor.", "  view <file>          - Menampilkan file tanpa editor."));
    console_writeln(tr3("  todo                 - Oeffnet ~/todo.txt in Lumen.", "  todo                 - Opens ~/todo.txt in Lumen.", "  todo                 - Membuka ~/todo.txt di Lumen."));
}

static void show_help_users(void) {
    console_writeln(tr3("Hilfe: Benutzer und Rechte", "Help: users and permissions", "Bantuan: pengguna dan izin"));
    console_writeln(tr3("  whoami               - Zeigt Benutzername, Rolle und Home.", "  whoami               - Shows user, role and home.", "  whoami               - Menampilkan pengguna, peran, dan home."));
    console_writeln(tr3("  users                - Zeigt bekannte Benutzer und Gruppen.", "  users                - Shows known users and groups.", "  users                - Menampilkan pengguna dan grup yang dikenal."));
    console_writeln(tr3("  login <n> [pw]       - Meldet einen Benutzer an.", "  login <n> [pw]       - Signs in a user.", "  login <n> [pw]       - Masuk sebagai pengguna."));
    console_writeln(tr3("  elevate <pw>         - Aktiviert den System-Modus.", "  elevate <pw>         - Activates system mode.", "  elevate <pw>         - Mengaktifkan mode sistem."));
    console_writeln(tr3("  drop                 - Verlaesst den System-Modus.", "  drop                 - Leaves system mode.", "  drop                 - Keluar dari mode sistem."));
    console_writeln(tr3("  passwd <neu>         - Aendert dein eigenes Passwort.", "  passwd <new>         - Changes your own password.", "  passwd <new>         - Mengubah kata sandi sendiri."));
    console_writeln(tr3("  passwd <u> <neu>     - Aendert Passwoerter im System-Modus.", "  passwd <u> <new>     - Changes passwords in system mode.", "  passwd <u> <new>     - Mengubah kata sandi dalam mode sistem."));
    console_writeln(tr3("  owner/chown <u> <p>  - Aendert den Besitzer.", "  owner/chown <u> <p>  - Changes the owner.", "  owner/chown <u> <p>  - Mengubah pemilik."));
    console_writeln(tr3("  protect/chmod        - Passt Rechte an.", "  protect/chmod        - Adjusts permissions.", "  protect/chmod        - Menyesuaikan izin."));
}

static void show_help_apps(void) {
    console_writeln(tr3("Hilfe: Apps, Befehle und Programme", "Help: apps, commands and programs", "Bantuan: aplikasi, perintah, dan program"));
    console_writeln(tr3("  apps                 - Zeigt Systembereiche und Hauptteile.", "  apps                 - Shows system areas and main parts.", "  apps                 - Menampilkan area sistem dan bagian utama."));
    console_writeln(tr3("  app list             - Zeigt eingebaute und optionale Apps.", "  app list             - Shows built-in and optional apps.", "  app list             - Menampilkan aplikasi bawaan dan opsional."));
    console_writeln(tr3("  app run <name>       - Startet eine App.", "  app run <name>       - Starts an app.", "  app run <name>       - Menjalankan aplikasi."));
    console_writeln(tr3("  app install/remove   - Installiert oder entfernt optionale Apps.", "  app install/remove   - Installs or removes optional apps.", "  app install/remove   - Memasang atau menghapus aplikasi opsional."));
    console_writeln(tr3("  app info <name>      - Zeigt Infos zu einer App.", "  app info <name>      - Shows info about an app.", "  app info <name>      - Menampilkan info aplikasi."));
    console_writeln(tr3("  cmd list             - Zeigt eigene Befehle.", "  cmd list             - Shows custom commands.", "  cmd list             - Menampilkan perintah kustom."));
    console_writeln(tr3("  cmd add/new/show/remove - Verwalten eigene Befehle.", "  cmd add/new/show/remove - Manage custom commands.", "  cmd add/new/show/remove - Mengelola perintah kustom."));
    console_writeln(tr3("  prog list            - Zeigt externe Programme mit Manifesten.", "  prog list            - Shows external programs with manifests.", "  prog list            - Menampilkan program eksternal dengan manifest."));
    console_writeln(tr3("  prog info/caps/trust/approve/run - Verwaltet externe Programme.", "  prog info/caps/trust/approve/run - Manages external programs.", "  prog info/caps/trust/approve/run - Mengelola program eksternal."));
    console_writeln(tr3("  launch <name>        - Startet App, Befehl oder externes Programm.", "  launch <name>        - Starts an app, command or external program.", "  launch <name>        - Menjalankan aplikasi, perintah, atau program eksternal."));
    console_writeln(tr3("  which <name>         - Zeigt, was hinter einem Namen steckt.", "  which <name>         - Shows what a name refers to.", "  which <name>         - Menampilkan arti sebuah nama."));
    console_writeln(tr3("  pkg list             - Zeigt die Paket-/Moduluebersicht.", "  pkg list             - Shows the package/module overview.", "  pkg list             - Menampilkan ringkasan paket/modul."));
}

static void show_help_games(void) {
    console_writeln(tr3("Hilfe: Spiele", "Help: games", "Bantuan: game"));
    console_writeln(tr3("  games                - Zeigt die eingebauten Spiele.", "  games                - Shows the built-in games.", "  games                - Menampilkan game bawaan."));
    console_writeln(tr3("  snake                - Startet Snake direkt.", "  snake                - Starts Snake directly.", "  snake                - Menjalankan Snake secara langsung."));
    console_writeln(tr3("  game stats snake     - Zeigt Score, Highscore und Session-Daten.", "  game stats snake     - Shows score, high score, and session data.", "  game stats snake     - Menampilkan skor, high score, dan data sesi."));
    console_writeln(tr3("  app run snake        - Startet Snake als App.", "  app run snake        - Starts Snake as an app.", "  app run snake        - Menjalankan Snake sebagai aplikasi."));
}

static void show_help_processes(void) {
    console_writeln(tr3("Hilfe: Prozesse, Tasks und Jobs", "Help: processes, tasks and jobs", "Bantuan: proses, task, dan job"));
    console_writeln(tr3("  tasks                - Zeigt laufende und gestoppte Bereiche.", "  tasks                - Shows running and stopped areas.", "  tasks                - Menampilkan area yang berjalan dan berhenti."));
    console_writeln(tr3("  jobs                 - Zeigt geplante Jobs.", "  jobs                 - Shows scheduled jobs.", "  jobs                 - Menampilkan job terjadwal."));
    console_writeln(tr3("  job add <s> <cmd>    - Fuehrt spaeter einen Befehl aus.", "  job add <s> <cmd>    - Runs a command later.", "  job add <s> <cmd>    - Menjalankan perintah nanti."));
    console_writeln(tr3("  job cancel <id>      - Bricht einen Job ab.", "  job cancel <id>      - Cancels a job.", "  job cancel <id>      - Membatalkan job."));
    console_writeln(tr3("  ps                   - Zeigt das Prozessmodell mit PID und Status.", "  ps                   - Shows the process model with PID and state.", "  ps                   - Menampilkan model proses dengan PID dan status."));
    console_writeln(tr3("  proc spawn <name>    - Startet einen verwalteten Prozess.", "  proc spawn <name>    - Starts a managed process.", "  proc spawn <name>    - Menjalankan proses terkelola."));
    console_writeln(tr3("  proc info <pid>      - Zeigt Details zu einem Prozess.", "  proc info <pid>      - Shows details about a process.", "  proc info <pid>      - Menampilkan detail proses."));
    console_writeln(tr3("  proc stop/resume/kill <pid> - Steuert einen Prozess.", "  proc stop/resume/kill <pid> - Controls a process.", "  proc stop/resume/kill <pid> - Mengendalikan proses."));
    console_writeln(tr3("  start/stop <name>    - Startet oder stoppt einen Bereich.", "  start/stop <name>    - Starts or stops an area.", "  start/stop <name>    - Menjalankan atau menghentikan area."));
}

static void show_help_system(void) {
    console_writeln(tr3("Hilfe: System und Diagnose", "Help: system and diagnostics", "Bantuan: sistem dan diagnosis"));
    console_writeln(tr3("  status               - Zeigt den aktuellen Zustand.", "  status               - Shows the current state.", "  status               - Menampilkan keadaan saat ini."));
    console_writeln(tr3("  health               - Zeigt Systemgesundheit und offene Warnungen.", "  health               - Shows system health and open warnings.", "  health               - Menampilkan kesehatan sistem dan peringatan terbuka."));
    console_writeln(tr3("  bootinfo             - Zeigt Boot-, Recovery- und Safe-Mode-Infos.", "  bootinfo             - Shows boot, recovery and safe-mode info.", "  bootinfo             - Menampilkan info boot, recovery, dan safe mode."));
    console_writeln(tr3("  log [all|boot|warn|errors|clear] - Zeigt das System-Log.", "  log [all|boot|warn|errors|clear] - Shows the system log.", "  log [all|boot|warn|errors|clear] - Menampilkan log sistem."));
    console_writeln(tr3("  safemode [status|on|off] - Steuert den persistenten Safe Mode.", "  safemode [status|on|off] - Controls persistent safe mode.", "  safemode [status|on|off] - Mengatur safe mode persisten."));
    console_writeln(tr3("  actionlog            - Zeigt erklaerbare Aktionen.", "  actionlog            - Shows explainable actions.", "  actionlog            - Menampilkan aksi yang bisa dijelaskan."));
    console_writeln(tr3("  history              - Zeigt letzte Befehle.", "  history              - Shows recent commands.", "  history              - Menampilkan perintah terakhir."));
    console_writeln(tr3("  memory               - Zeigt freien und belegten Speicher.", "  memory               - Shows free and used memory.", "  memory               - Menampilkan memori kosong dan terpakai."));
    console_writeln(tr3("  paging               - Zeigt Paging, Frames und letzte Faults.", "  paging               - Shows paging, frames and recent faults.", "  paging               - Menampilkan paging, frame, dan fault terakhir."));
    console_writeln(tr3("  alloc <zahl>         - Reserviert testweise Speicher.", "  alloc <number>       - Reserves memory for testing.", "  alloc <number>       - Mencadangkan memori untuk pengujian."));
    console_writeln(tr3("  diag                 - Zeigt eine kurze Diagnose.", "  diag                 - Shows a short diagnosis.", "  diag                 - Menampilkan diagnosis singkat."));
    console_writeln(tr3("  doctor               - Gibt konkrete Hilfshinweise.", "  doctor               - Gives concrete help hints.", "  doctor               - Memberi petunjuk bantuan konkret."));
    console_writeln(tr3("  recover              - Setzt sichere Basiswerte.", "  recover              - Restores safe baseline values.", "  recover              - Mengembalikan nilai dasar yang aman."));
    console_writeln(tr3("  version / about      - Zeigt Build und Projektinfo.", "  version / about      - Shows build and project info.", "  version / about      - Menampilkan build dan info proyek."));
    console_writeln(tr3("  reboot               - Startet Cyralith neu.", "  reboot               - Restarts Cyralith.", "  reboot               - Memulai ulang Cyralith."));
    console_writeln(tr3("  shutdown             - Markiert einen sauberen Stopp und haelt an.", "  shutdown             - Marks a clean stop and halts.", "  shutdown             - Menandai stop bersih lalu berhenti."));
    console_writeln(tr3("  panic test           - Zeigt den roten Crashscreen zum Testen.", "  panic test           - Shows the red crash screen for testing.", "  panic test           - Menampilkan layar crash merah untuk pengujian."));
    console_writeln(tr3("  clear                - Leert den Bildschirm.", "  clear                - Clears the screen.", "  clear                - Membersihkan layar."));
}

static void show_help_settings(void) {
    console_writeln(tr3("Hilfe: Einstellungen und Bedienung", "Help: settings and navigation", "Bantuan: pengaturan dan navigasi"));
    console_writeln(tr3("  settings             - Oeffnet die Einstellungszentrale.", "  settings             - Opens the settings center.", "  settings             - Membuka pusat pengaturan."));
    console_writeln(tr3("  open settings        - Zweiter Weg in die Einstellungen.", "  open settings        - Second way into settings.", "  open settings        - Cara kedua masuk ke pengaturan."));
    console_writeln(tr3("  settings general     - Springt direkt in Allgemein.", "  settings general     - Jumps directly to General.", "  settings general     - Langsung ke Umum."));
    console_writeln(tr3("  settings network     - Springt direkt in Netzwerk.", "  settings network     - Jumps directly to Network.", "  settings network     - Langsung ke Jaringan."));
    console_writeln(tr3("  settings security    - Springt direkt in Sicherheit.", "  settings security    - Jumps directly to Security.", "  settings security    - Langsung ke Keamanan."));
    console_writeln(tr3("  settings expert      - Springt direkt in Expertenmodus.", "  settings expert      - Jumps directly to Expert mode.", "  settings expert      - Langsung ke mode pakar."));
    console_writeln(tr3("  Pfeil hoch/runter    - Waehlt einen Eintrag.", "  Up/Down arrows       - Select an entry.", "  Panah atas/bawah     - Memilih satu entri."));
    console_writeln(tr3("  Enter                - Aendert oder oeffnet den Eintrag.", "  Enter                - Changes or opens the entry.", "  Enter                - Mengubah atau membuka entri."));
    console_writeln(tr3("  q oder Ctrl+C        - Geht zurueck.", "  q or Ctrl+C          - Goes back.", "  q atau Ctrl+C        - Kembali."));
}

static void show_help_overview(void) {
    console_writeln(tr3("Cyralith Hilfe", "Cyralith help", "Bantuan Cyralith"));
    console_writeln(tr3("Themen statt Textwand:", "Topics instead of a text wall:", "Topik tanpa dinding teks:"));
    console_writeln(tr3("  help network         - Zeigt Netzwerk-Befehle.", "  help network         - Shows network commands.", "  help network         - Menampilkan perintah jaringan."));
    console_writeln(tr3("  help files           - Zeigt Datei- und CyralithFS-Befehle.", "  help files           - Shows file and CyralithFS commands.", "  help files           - Menampilkan perintah file dan CyralithFS."));
    console_writeln(tr3("  help users           - Zeigt Benutzer- und Rechte-Befehle.", "  help users           - Shows user and permission commands.", "  help users           - Menampilkan perintah pengguna dan izin."));
    console_writeln(tr3("  help apps            - Zeigt Apps, cmd, prog und launch.", "  help apps            - Shows apps, cmd, prog and launch.", "  help apps            - Menampilkan apps, cmd, prog, dan launch."));
    console_writeln(tr3("  help games           - Zeigt Spiele und schnelle Unterhaltung.", "  help games           - Shows games and quick fun.", "  help games           - Menampilkan game dan hiburan singkat."));
    console_writeln(tr3("  help processes       - Zeigt tasks, jobs und proc.", "  help processes       - Shows tasks, jobs and proc.", "  help processes       - Menampilkan tasks, jobs, dan proc."));
    console_writeln(tr3("  help system          - Zeigt Status, Reliability und Diagnose.", "  help system          - Shows status, reliability and diagnostics.", "  help system          - Menampilkan status, reliability, dan diagnosis."));
    console_writeln(tr3("  help settings        - Zeigt Einstellungen und Steuerung.", "  help settings        - Shows settings and navigation.", "  help settings        - Menampilkan pengaturan dan navigasi."));
    console_writeln(tr3("  help all             - Zeigt weiterhin die komplette Liste.", "  help all             - Still shows the complete list.", "  help all             - Tetap menampilkan daftar lengkap."));
    console_writeln("");
    console_writeln(tr3("Tipps:", "Tips:", "Tips:"));
    console_writeln(tr3("  quickstart           - Einfache erste Schritte.", "  quickstart           - Simple first steps.", "  quickstart           - Langkah awal sederhana."));
    console_writeln(tr3("  status               - Zeigt den aktuellen Zustand.", "  status               - Shows the current state.", "  status               - Menampilkan keadaan saat ini."));
    console_writeln(tr3("  ai <text>            - Du kannst auch normal schreiben.", "  ai <text>            - You can also write naturally.", "  ai <text>            - Kamu juga bisa menulis biasa."));
}

static void command_help(const char* args) {
    char topic[32];
    if (args == (const char*)0 || split_first_arg(args, topic, sizeof(topic)) != 0) {
        show_help_overview();
        return;
    }
    if (kstrcmp(topic, "network") == 0 || kstrcmp(topic, "netzwerk") == 0 || kstrcmp(topic, "net") == 0) { show_help_network(); return; }
    if (kstrcmp(topic, "files") == 0 || kstrcmp(topic, "dateien") == 0 || kstrcmp(topic, "fs") == 0 || kstrcmp(topic, "filesystem") == 0) { show_help_files(); return; }
    if (kstrcmp(topic, "users") == 0 || kstrcmp(topic, "user") == 0 || kstrcmp(topic, "benutzer") == 0 || kstrcmp(topic, "rechte") == 0) { show_help_users(); return; }
    if (kstrcmp(topic, "apps") == 0 || kstrcmp(topic, "app") == 0 || kstrcmp(topic, "programs") == 0 || kstrcmp(topic, "programme") == 0 || kstrcmp(topic, "commands") == 0 || kstrcmp(topic, "befehle") == 0) { show_help_apps(); return; }
    if (kstrcmp(topic, "games") == 0 || kstrcmp(topic, "game") == 0 || kstrcmp(topic, "spiele") == 0 || kstrcmp(topic, "snake") == 0) { show_help_games(); return; }
    if (kstrcmp(topic, "processes") == 0 || kstrcmp(topic, "process") == 0 || kstrcmp(topic, "proc") == 0 || kstrcmp(topic, "jobs") == 0 || kstrcmp(topic, "tasks") == 0 || kstrcmp(topic, "prozesse") == 0) { show_help_processes(); return; }
    if (kstrcmp(topic, "system") == 0 || kstrcmp(topic, "diagnose") == 0 || kstrcmp(topic, "status") == 0) { show_help_system(); return; }
    if (kstrcmp(topic, "settings") == 0 || kstrcmp(topic, "einstellungen") == 0 || kstrcmp(topic, "prefs") == 0) { show_help_settings(); return; }
    if (kstrcmp(topic, "all") == 0 || kstrcmp(topic, "alles") == 0 || kstrcmp(topic, "full") == 0) { show_help_all(); return; }
    show_help_topic_unknown(topic);
}

static void show_help_all(void) {
    console_writeln(tr("Cyralith Hilfe", "Cyralith help"));
    console_writeln(tr("Die wichtigsten Befehle:", "The most useful commands:"));
    console_writeln(tr("  help                 - Zeigt diese Hilfe.", "  help                 - Shows this help."));
    console_writeln(tr("  quickstart           - Einfache erste Schritte.", "  quickstart           - Simple first steps."));
    console_writeln(tr("  status               - Zeigt den aktuellen Zustand.", "  status               - Shows the current status."));
    console_writeln(tr("  apps                 - Zeigt die wichtigsten Teile des Systems.", "  apps                 - Shows the main parts of the system."));
    console_writeln(tr("  tasks                - Zeigt laufende und gestoppte Bereiche.", "  tasks                - Shows running and stopped areas."));
    console_writeln(tr("  jobs                 - Zeigt geplante Automations-Jobs.", "  jobs                 - Shows scheduled automation jobs."));
    console_writeln(tr("  ps                   - Zeigt das Prozessmodell mit PID und Status.", "  ps                   - Shows the process model with PID and state."));
    console_writeln(tr("  proc ...             - Startet, stoppt oder beendet Prozesse.", "  proc ...             - Starts, stops or ends processes."));
    console_writeln(tr("  actionlog            - Zeigt erklaerbare Systemaktionen.", "  actionlog            - Shows explainable system actions."));
    console_writeln(tr("  memory               - Zeigt freien und belegten Speicher.", "  memory               - Shows free and used memory."));
    console_writeln(tr("  paging               - Zeigt Paging, Frames und Page-Faults.", "  paging               - Shows paging, frames and page faults."));
    console_writeln(tr("  history              - Zeigt deine letzten Befehle.", "  history              - Shows your recent commands."));
    console_writeln(tr3("  keytest              - Zeigt rohe Tastenwerte fuer Layout-Tests.", "  keytest              - Shows raw key values for layout tests.", "  keytest              - Menampilkan nilai tombol mentah untuk tes layout."));
    console_writeln(tr3("  layout <de|us>       - Stellt das Cyralith-Tastaturlayout um.", "  layout <de|us>       - Changes the Cyralith keyboard layout.", "  layout <de|us>       - Mengubah layout keyboard Cyralith."));
    console_writeln(tr3("  calc <a> <op> <b>    - Rechnet direkt in der Shell.", "  calc <a> <op> <b>    - Calculates directly in the shell.", "  calc <a> <op> <b>    - Menghitung langsung di shell."));
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
    console_writeln(tr("  fs                   - Erklaert CyralithFS kurz.", "  fs                   - Explains CyralithFS briefly."));
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
    console_writeln(tr3("  netdrivers           - Zeigt alle unterstuetzten Netzwerktreiber.", "  netdrivers           - Shows all supported network drivers.", "  netdrivers           - Menampilkan semua driver jaringan yang didukung."));
    console_writeln(tr3("  netdriver <intel|e1000|pcnet|rtl8139|virtio> - Waehlt den bevorzugten Netzwerktreiber.", "  netdriver <intel|e1000|pcnet|rtl8139|virtio> - Selects the preferred network driver.", "  netdriver <intel|e1000|pcnet|rtl8139|virtio> - Memilih driver jaringan utama."));
    console_writeln(tr3("  netup                - Startet den ausgewaehlten Netzwerktreiber.", "  netup                - Starts the selected network driver.", "  netup                - Menyalakan driver jaringan yang dipilih."));
    console_writeln(tr3("  netprobe             - Sendet einen kleinen Rohdaten-Test, aber nur mit e1000.", "  netprobe             - Sends a small raw packet test, but only with e1000.", "  netprobe             - Mengirim tes paket mentah, tapi hanya dengan e1000."));
    console_writeln(tr("  mac                  - Zeigt die erkannte MAC-Adresse des aktiven Treibers.", "  mac                  - Shows the detected MAC address of the active driver."));
    console_writeln(tr("  diag                 - Zeigt eine kurze Diagnose fuer System, Platte und Netzwerk.", "  diag                 - Shows a short diagnosis for system, disk and network."));
    console_writeln(tr("  doctor               - Diagnose-Assistent mit konkreten Hinweisen.", "  doctor               - Diagnosis assistant with concrete hints."));
    console_writeln(tr3("  health               - Zeigt Stabilitaet, Safe Mode und Warnungen.", "  health               - Shows stability, safe mode and warnings.", "  health               - Menampilkan stabilitas, safe mode, dan peringatan."));
    console_writeln(tr3("  bootinfo             - Zeigt Bootzaehler und Recovery-Historie.", "  bootinfo             - Shows boot counters and recovery history.", "  bootinfo             - Menampilkan penghitung boot dan riwayat recovery."));
    console_writeln(tr3("  log boot             - Zeigt die Boot-Historie aus dem System-Log.", "  log boot             - Shows boot history from the system log.", "  log boot             - Menampilkan riwayat boot dari log sistem."));
    console_writeln(tr3("  safemode on/off      - Schaltet persistenten Safe Mode.", "  safemode on/off      - Toggles persistent safe mode.", "  safemode on/off      - Mengubah safe mode persisten."));
    console_writeln(tr("  recover              - Setzt sichere Basiswerte fuer Netzwerk und Rechte.", "  recover              - Restores safe baseline values for network and rights."));
    console_writeln(tr("  app list             - Zeigt eingebaute und optionale Apps.", "  app list             - Shows built-in and optional apps."));
    console_writeln(tr("  app run <name>       - Startet eine App oder ihren Platzhalter.", "  app run <name>       - Starts an app or its placeholder."));
    console_writeln(tr("  launch <name>        - Startet App, Custom-Command oder externes Programm.", "  launch <name>        - Starts an app, custom command or external program."));
    console_writeln(tr("  pkg list             - Einfache Paket-/Moduluebersicht.", "  pkg list             - Simple package/module overview."));
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
    console_writeln(tr("  job add <s> <cmd>    - Fuehrt spaeter einen Shell-Befehl aus.", "  job add <s> <cmd>    - Runs a shell command later."));
    console_writeln(tr("  job cancel <id>      - Bricht einen geplanten Job ab.", "  job cancel <id>      - Cancels a scheduled job."));
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
    console_writeln(tr("  proc spawn <name>    - Startet einen verwalteten Prozess mit eigener Region.", "  proc spawn <name>    - Starts a managed process with its own region."));
    console_writeln(tr("  proc info <pid>      - Zeigt Speicherlayout und Metadaten eines Prozesses.", "  proc info <pid>      - Shows the memory layout and metadata of a process."));
    console_writeln(tr("  proc stop <pid>      - Haelt einen Prozess an.", "  proc stop <pid>      - Stops a process."));
    console_writeln(tr("  proc resume <pid>    - Setzt einen Prozess fort.", "  proc resume <pid>    - Resumes a process."));
    console_writeln(tr("  proc kill <pid>      - Beendet einen Prozess.", "  proc kill <pid>      - Kills a process."));
    console_writeln(tr("  reboot               - Startet Cyralith neu.", "  reboot               - Restarts Cyralith."));
    console_writeln(tr("  open <ziel>          - Oeffnet settings, desktop, network, files oder monitor.", "  open <target>        - Opens settings, desktop, network, files or monitor."));
    console_writeln(tr("  app run settings     - Startet die Settings-App.", "  app run settings     - Starts the settings app."));
    console_writeln(tr3("  snake                - Startet das eingebaute Snake-Spiel.", "  snake                - Starts the built-in Snake game.", "  snake                - Menjalankan game Snake bawaan."));
    console_writeln(tr("  version              - Zeigt die Build-Version.", "  version              - Shows the build version."));
    console_writeln(tr("  about                - Erklaert kurz, was Cyralith werden soll.", "  about                - Briefly explains what Cyralith should become."));
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
    console_writeln(tr("Cyralith ist ein eigenes, noch junges Betriebssystem.", "Cyralith is its own young operating system."));
    console_writeln(tr("Die Idee: leicht zu bedienen wie Windows und anpassbar wie Linux.", "The idea: easy to use like Windows and customizable like Linux."));
    console_writeln(tr("Aktuell gibt es eine stabile Shell, Nutzer- und Rechte-Persistenz, CyralithFS mit cp/mv/find, erste Platten-Speicherung, PCI-Netzwerkerkennung, einen Intel-Kompatibilitaetsmodus und einen e1000-Pilottreiber, ein kleines App-Modell, Paging-Grundlagen, ein kooperatives Prozessmodell, eine groessere Settings-Zentrale mit Kategorien und Expertenmodus, eigene Skript-Befehle, sichere externe Programme mit Manifesten und Lumen.", "Right now it has a stable shell, persistent users and rights, CyralithFS with cp/mv/find, first disk persistence, PCI network detection, an e1000 pilot driver, a small app model, paging foundations, a cooperative process model, a larger categorized settings center with expert mode, custom script commands and Lumen."));
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

static void shell_shutdown(void) {
    (void)save_system_state();
    syslog_write(SYSLOG_INFO, "shell", "shutdown requested");
    (void)syslog_save_persistent();
    (void)recovery_mark_clean_shutdown("shutdown");
    (void)network_driver_shutdown();
    console_writeln(tr3("System wird sauber heruntergefahren ...", "System is powering off cleanly ...", "Sistem sedang dimatikan dengan bersih ..."));
    console_writeln(tr3("Sende Ausschalt-Signal an die VM ...", "Sending power-off signal to the VM ...", "Mengirim sinyal mati ke VM ..."));

    __asm__ volatile ("cli");

    /* QEMU / Bochs ACPI power-off */
    outw(0x604U, 0x2000U);
    outw(0xB004U, 0x2000U);
    /* VirtualBox ACPI power-off */
    outw(0x4004U, 0x3400U);
    /* Legacy/alternate power-off ports used by some emulators */
    outw(0x600U, 0x0034U);

    console_writeln(tr3(
        "Falls die VM offen bleibt, hat der Emulator das Ausschalt-Signal ignoriert.",
        "If the VM stays open, the emulator ignored the power-off request.",
        "Jika VM tetap terbuka, emulator mengabaikan permintaan mati."
    ));

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void shell_reboot(void) {
    (void)save_system_state();
    syslog_write(SYSLOG_INFO, "shell", "reboot requested");
    (void)syslog_save_persistent();
    (void)recovery_mark_clean_shutdown("reboot");
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
    {
        unsigned int pid = process_session_start(PROCESS_KIND_COMMAND, name, cmd);
        program_context_enter(name, EXTPROG_CAP_FS_READ, "legacy");
        execute_script_file(script_path, args);
        program_context_leave();
        process_session_finish(pid, 0);
    }
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
        {
            unsigned int pid = process_session_start(PROCESS_KIND_COMMAND, name, args);
            program_context_enter(name, EXTPROG_CAP_FS_READ, "legacy");
            execute_script_file(value, rest);
            program_context_leave();
            process_session_finish(pid, 0);
        }
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
        if (afs_write_file(script_path, "# Cyralith command script\necho Hello from Cyralith\n") != AFS_OK) {
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
        {
            unsigned int pid = process_session_start(PROCESS_KIND_PROGRAM, name, args);
            program_context_enter(name, manifest.caps, manifest.trust);
            execute_script_file(manifest.entry, rest);
            program_context_leave();
            process_session_finish(pid, 0);
        }
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
            console_writeln(tr("Netzwerk-Rechte fuer Programme vergibt Cyralith nur im System-Modus.", "Cyralith only grants network permissions to programs in system mode."));
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
    int arcade_ok = arcade_save_persistent();
    int recovery_ok = recovery_save_persistent();
    int log_ok = syslog_save_persistent();
    return (afs_ok == AFS_OK && user_ok == 0 && net_ok == 0 && app_ok == 0 && arcade_ok == 0 && recovery_ok == 0 && log_ok == 0) ? 0 : -1;
}

static int load_system_state(void) {
    int user_ok = user_load_persistent();
    int net_ok = network_load_persistent();
    int afs_ok = afs_load_persistent();
    int app_ok = app_load_persistent();
    int arcade_ok = arcade_load_persistent();
    int recovery_ok = recovery_load_persistent();
    int log_ok = syslog_load_persistent();
    return (afs_ok == AFS_OK && user_ok == 0 && net_ok == 0 && app_ok == 0 && arcade_ok == 0 && recovery_ok == 0 && log_ok == 0) ? 0 : -1;
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
        console_write(tr3("Treiber gestartet: ", "Driver started: ", "Driver dimulai: "));
        console_writeln(network_driver_name());
        log_action_simple("netup", network_driver_name(), ACTIONLOG_OK);
        if (g_setting_driver_debug != 0) {
            if (network_driver_kind() == NETWORK_DRIVER_E1000) {
                console_writeln(tr3("Treiber-Diagnose: MMIO initialisiert, MAC gelesen, Link-Status wird beobachtet.", "Driver diagnostics: MMIO initialized, MAC read, link status will be monitored.", "Diagnostik driver: MMIO diinisialisasi, MAC dibaca, status link dipantau."));
            } else {
                console_writeln(tr3("Treiber-Diagnose: sicherer Attach-Modus aktiv, kaum invasive Hardware-Zugriffe.", "Driver diagnostics: safe attach mode active, only minimally invasive hardware access.", "Diagnostik driver: mode safe attach aktif, akses hardware sangat minimal."));
            }
        }
        print_network_status();
        return;
    }
    if (rc == 1) {
        console_writeln(tr3("Der ausgewaehlte Netzwerktreiber ist bereits aktiv.", "The selected network driver is already active.", "Driver jaringan yang dipilih sudah aktif."));
        return;
    }
    if (rc == 2) {
        console_writeln(tr3("Es laeuft schon ein anderer Netzwerktreiber. Wechsle ihn in Settings > Netzwerk oder mit 'netdriver'.", "Another network driver is already running. Switch it in Settings > Network or with 'netdriver'.", "Driver jaringan lain sudah aktif. Ganti di Settings > Network atau dengan 'netdriver'."));
        return;
    }
    if (rc == -1) {
        console_writeln(tr3("Keine passende Netzwerkkarte fuer den ausgewaehlten Treiber gefunden. Nutze 'nic' und 'netdrivers'.", "No matching network card found for the selected driver. Use 'nic' and 'netdrivers'.", "Tidak ada kartu jaringan yang cocok untuk driver yang dipilih. Pakai 'nic' dan 'netdrivers'."));
        recovery_note_issue("netup: no matching NIC");
        log_action_simple("netup", "no supported nic", ACTIONLOG_FAIL);
        return;
    }
    if (rc == -2) {
        console_writeln(tr3("Die Karte wurde gefunden, aber BAR/MMIO/IO war fuer diesen Modus ungueltig.", "The card was found, but BAR/MMIO/IO was invalid for this mode.", "Kartu ditemukan, tetapi BAR/MMIO/IO tidak valid untuk mode ini."));
        return;
    }
    console_writeln(tr3("Der ausgewaehlte Netzwerktreiber konnte nicht sauber starten.", "The selected network driver could not start cleanly.", "Driver jaringan yang dipilih tidak bisa dimulai dengan bersih."));
    recovery_note_issue("netup failed");
    log_action_simple("netup", "driver start failed", ACTIONLOG_FAIL);
}

static void command_netdriver(const char* args) {
    char choice[16];
    int rc;
    if (split_first_arg(args, choice, sizeof(choice)) != 0) {
        console_write(tr3("Aktuelle Treiberwahl: ", "Current driver choice: ", "Pilihan driver saat ini: "));
        console_writeln(network_driver_preference_name());
        console_writeln(tr3("Nutze: netdriver <intel|e1000|pcnet|rtl8139|virtio>", "Use: netdriver <intel|e1000|pcnet|rtl8139|virtio>", "Pakai: netdriver <intel|e1000|pcnet|rtl8139|virtio>"));
        return;
    }
    if (needs_master("netdriver <name>") != 0) {
        return;
    }
    if (kstrcmp(choice, "intel") == 0 || kstrcmp(choice, "pro1000") == 0) {
        rc = network_set_driver_preference(NETWORK_DRIVER_INTEL);
    } else if (kstrcmp(choice, "e1000") == 0) {
        rc = network_set_driver_preference(NETWORK_DRIVER_E1000);
    } else if (kstrcmp(choice, "pcnet") == 0 || kstrcmp(choice, "amd") == 0) {
        rc = network_set_driver_preference(NETWORK_DRIVER_PCNET);
    } else if (kstrcmp(choice, "rtl8139") == 0 || kstrcmp(choice, "realtek") == 0) {
        rc = network_set_driver_preference(NETWORK_DRIVER_RTL8139);
    } else if (kstrcmp(choice, "virtio") == 0) {
        rc = network_set_driver_preference(NETWORK_DRIVER_VIRTIO);
    } else {
        console_writeln(tr3("Nutze: netdriver <intel|e1000|pcnet|rtl8139|virtio>", "Use: netdriver <intel|e1000|pcnet|rtl8139|virtio>", "Pakai: netdriver <intel|e1000|pcnet|rtl8139|virtio>"));
        return;
    }
    if (rc == 0) {
        console_write(tr3("Treiberwahl gesetzt: ", "Driver selection set: ", "Pilihan driver disetel: "));
        console_writeln(network_driver_preference_name());
        if (network_driver_active() != 0) {
            console_writeln(tr3("Ein aktiver anderer Treiber wurde dabei gestoppt. Starte jetzt 'netup'.", "An active different driver was stopped. Now run 'netup'.", "Driver aktif lain dihentikan. Sekarang jalankan 'netup'."));
        }
        log_action_simple("netdriver", network_driver_preference_name(), ACTIONLOG_OK);
        return;
    }
    console_writeln(tr3("Treiberwahl konnte nicht gesetzt werden.", "Could not set driver choice.", "Pilihan driver tidak bisa disetel."));
    log_action_simple("netdriver", choice, ACTIONLOG_FAIL);
}

static void command_netprobe(void) {
    int rc = network_send_probe();
    if (rc == 0) {
        console_writeln(tr("Rohdaten-Testframe gesendet.", "Raw probe frame sent."));
        return;
    }
    if (rc == -1) {
        console_writeln(tr3("Noch kein aktiver Netzwerktreiber. Nutze zuerst 'netup'.", "No active network driver yet. Use 'netup' first.", "Belum ada driver jaringan aktif. Jalankan 'netup' dulu."));
        return;
    }
    if (rc == -4) {
        console_writeln(tr3("Dieser Treibermodus kann keinen Rohdaten-Test senden. Wechsle auf e1000.", "This driver mode cannot send a raw probe. Switch to e1000.", "Mode driver ini tidak bisa mengirim probe mentah. Ganti ke e1000."));
        return;
    }
    console_writeln(tr3("Senden fehlgeschlagen oder Warteschlange blockiert.", "Send failed or the queue is blocked.", "Pengiriman gagal atau antrean macet."));
}

static void command_mac(void) {
    console_write(tr("Aktive MAC-Adresse: ", "Active MAC address: "));
    console_writeln(network_mac_address());
}

static void app_run_target(const char* name) {
    unsigned int pid;
    if (app_is_installed(name) == 0) {
        console_writeln(tr("Diese App ist gerade nicht installiert.", "That app is not installed right now."));
        return;
    }
    pid = process_session_start(PROCESS_KIND_APP, name, name);
    if (kstrcmp(name, "lumen") == 0) {
        editor_open("notes.txt");
        process_session_finish(pid, 0);
        return;
    }
    if (kstrcmp(name, "files") == 0) {
        afs_ls("");
        process_session_finish(pid, 0);
        return;
    }
    if (kstrcmp(name, "settings") == 0) {
        open_target("settings");
        process_session_finish(pid, 0);
        return;
    }
    if (kstrcmp(name, "network") == 0) {
        print_network_status();
        print_nic_status();
        process_session_finish(pid, 0);
        return;
    }
    if (kstrcmp(name, "monitor") == 0) {
        print_status();
        process_session_finish(pid, 0);
        return;
    }
    if (kstrcmp(name, "snake") == 0) {
        snake_open();
        process_session_finish(pid, 0);
        return;
    }
    console_writeln(tr("App-Platzhalter geoeffnet.", "App placeholder opened."));
    process_session_finish(pid, 0);
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
        if (rc == 0 || rc == 1) { console_writeln(tr("App ist jetzt installiert.", "App is now installed.")); log_action_simple("app.install", name, ACTIONLOG_OK); }
        else { console_writeln(tr("Diese App kenne ich nicht.", "I do not know that app.")); log_action_simple("app.install", name, ACTIONLOG_FAIL); }
        return;
    }
    if (kstrcmp(action, "remove") == 0) {
        int rc;
        if (needs_master("app remove <name>") != 0) { return; }
        rc = app_remove(name);
        if (rc == 0 || rc == 1) { console_writeln(tr("App wurde entfernt.", "App was removed.")); log_action_simple("app.remove", name, ACTIONLOG_OK); }
        else if (rc == -2) { console_writeln(tr("Interne Apps koennen nicht entfernt werden.", "Built-in apps cannot be removed.")); log_action_simple("app.remove", name, ACTIONLOG_DENY); }
        else { console_writeln(tr("Diese App kenne ich nicht.", "I do not know that app.")); log_action_simple("app.remove", name, ACTIONLOG_FAIL); }
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
        log_action_simple("app.run", name, ACTIONLOG_OK);
        return;
    }
    console_writeln(tr("Nutze: app <list|install|remove|info|run> [name]", "Use: app <list|install|remove|info|run> [name]"));
}

static void command_job(const char* args) {
    char action[16];
    char left[16];
    char right[128];
    unsigned int id;
    if (split_first_arg(args, action, sizeof(action)) != 0) {
        console_writeln(tr("Nutze: job <list|add|cancel> ...", "Use: job <list|add|cancel> ..."));
        return;
    }
    if (kstrcmp(action, "list") == 0) {
        print_jobs();
        return;
    }
    if (kstrcmp(action, "add") == 0) {
        const char* rest = args + 3;
        while (*rest == ' ') { rest++; }
        if (split_two_args(rest, left, sizeof(left), right, sizeof(right)) != 0) {
            console_writeln(tr("Nutze: job add <sekunden> <befehl>", "Use: job add <seconds> <command>"));
            return;
        }
        if (automation_schedule_in_seconds((unsigned int)katoi(left), right, &id) == 0) {
            console_write(tr("Job geplant mit ID #", "Job scheduled with ID #"));
            console_write_dec(id);
            console_putc('\n');
            log_action_simple("job.add", right, ACTIONLOG_OK);
        } else {
            console_writeln(tr("Job konnte nicht geplant werden.", "Could not schedule job."));
            log_action_simple("job.add", right, ACTIONLOG_FAIL);
        }
        return;
    }
    if (kstrcmp(action, "cancel") == 0) {
        if (split_first_arg(args + 6, left, sizeof(left)) != 0) {
            console_writeln(tr("Nutze: job cancel <id>", "Use: job cancel <id>"));
            return;
        }
        id = (unsigned int)katoi(left);
        if (automation_cancel(id) == 0) {
            console_writeln(tr("Job abgebrochen.", "Job canceled."));
            log_action_simple("job.cancel", left, ACTIONLOG_OK);
        } else {
            console_writeln(tr("Diesen Job kenne ich nicht.", "I do not know that job."));
            log_action_simple("job.cancel", left, ACTIONLOG_FAIL);
        }
        return;
    }
    console_writeln(tr("Nutze: job <list|add|cancel>", "Use: job <list|add|cancel>"));
}

static void command_launch(const char* args) {
    char name[32];
    if (split_first_arg(args, name, sizeof(name)) != 0) {
        console_writeln(tr("Nutze: launch <name>", "Use: launch <name>"));
        return;
    }
    if (app_find(name) != (const app_t*)0) {
        app_run_target(name);
        log_action_simple("launch.app", name, ACTIONLOG_OK);
        return;
    }
    if (read_custom_command_script(name, (char[96]){0}, 96) != 0) {
        char line[64];
        line[0] = '\0';
        append_limited_local(line, sizeof(line), "cmd run ");
        append_limited_local(line, sizeof(line), name);
        run_command(line);
        return;
    }
    if (extprog_load(name, &(extprog_manifest_t){0}) == 0) {
        char line[64];
        line[0] = '\0';
        append_limited_local(line, sizeof(line), "prog run ");
        append_limited_local(line, sizeof(line), name);
        run_command(line);
        return;
    }
    console_writeln(tr("Weder App noch Programm gefunden.", "Found neither app nor program."));
    log_action_simple("launch", name, ACTIONLOG_FAIL);
}

static void command_pkg(const char* args) {
    if (kstrcmp(args, "") == 0 || kstrcmp(args, "list") == 0) {
        command_app("list");
        console_writeln(tr("Module/Programme:", "Modules/programs:"));
        command_prog("list");
        return;
    }
    if (kstarts_with(args, "install ")) {
        char line[64];
        line[0] = '\0';
        append_limited_local(line, sizeof(line), "app install ");
        append_limited_local(line, sizeof(line), args + 8);
        run_command(line);
        return;
    }
    if (kstarts_with(args, "remove ")) {
        char line[64];
        line[0] = '\0';
        append_limited_local(line, sizeof(line), "app remove ");
        append_limited_local(line, sizeof(line), args + 7);
        run_command(line);
        return;
    }
    console_writeln(tr("Nutze: pkg [list|install|remove] <name>", "Use: pkg [list|install|remove] <name>"));
}

static void print_bootinfo_summary(void) {
    console_writeln(tr3("Boot- und Recovery-Status:", "Boot and recovery status:", "Status boot dan recovery:"));
    console_write(tr3("  Starts gesamt: ", "  Total boots: ", "  Total boot: "));
    console_write_dec(recovery_boot_count());
    console_putc('\n');
    console_write(tr3("  Letzte Boot-Phase: ", "  Last boot stage: ", "  Tahap boot terakhir: "));
    console_writeln(recovery_last_boot_stage());
    console_write(tr3("  Startup-Fehler: ", "  Startup failures: ", "  Gagal startup: "));
    console_write_dec(recovery_startup_failure_count());
    console_putc('\n');
    console_write(tr3("  Unerwartete Shutdowns: ", "  Unexpected shutdowns: ", "  Shutdown tak terduga: "));
    console_write_dec(recovery_unexpected_shutdown_count());
    console_putc('\n');
    console_write(tr3("  Auto-Recovery-Ereignisse: ", "  Auto recovery events: ", "  Peristiwa auto-recovery: "));
    console_write_dec(recovery_auto_recovery_count());
    console_putc('\n');
    console_write(tr3("  Safe Mode: ", "  Safe mode: ", "  Safe mode: "));
    console_writeln(recovery_safe_mode_enabled() != 0 ? tr3("aktiv", "enabled", "aktif") : tr3("aus", "off", "mati"));
    if (recovery_last_issue()[0] != '\0') {
        console_write(tr3("  Letztes Problem: ", "  Last issue: ", "  Masalah terakhir: "));
        console_writeln(recovery_last_issue());
    }
}

static void print_syslog_filtered(int minimum_level, int boot_only) {
    size_t shown = 0U;
    console_writeln(tr3("System-Log:", "System log:", "Log sistem:"));
    if (syslog_count() == 0U) {
        console_writeln(tr3("  Noch keine Log-Eintraege vorhanden.", "  No log entries yet.", "  Belum ada entri log."));
        return;
    }
    for (size_t i = 0U; i < syslog_count(); ++i) {
        const syslog_entry_t* entry = syslog_get_recent(i);
        if (entry == (const syslog_entry_t*)0) {
            continue;
        }
        if ((int)entry->level < minimum_level) {
            continue;
        }
        if (boot_only != 0 && kcontains_ci(entry->source, "boot") == 0 && kcontains_ci(entry->message, "boot") == 0) {
            continue;
        }
        console_write("  #");
        console_write_dec(entry->sequence);
        console_write(" [");
        console_write(syslog_level_name((syslog_level_t)entry->level));
        console_write("] ");
        console_write(entry->source);
        console_write(": ");
        console_writeln(entry->message);
        shown++;
    }
    if (shown == 0U) {
        console_writeln(tr3("  Keine passenden Eintraege gefunden.", "  No matching entries found.", "  Tidak ada entri yang cocok."));
    }
}

static void command_bootinfo(void) {
    print_bootinfo_summary();
    log_action_simple("bootinfo", "reliability summary", ACTIONLOG_OK);
}

static void command_health(void) {
    console_writeln(tr3("System Health:", "System health:", "Kesehatan sistem:"));
    console_write(tr3("  Speicher-Backend: ", "  Storage backend: ", "  Backend storage: "));
    console_writeln(afs_persistence_name());
    console_write(tr3("  Netzwerk-Treiber: ", "  Network driver: ", "  Driver jaringan: "));
    console_writeln(network_driver_active() != 0 ? network_driver_name() : network_driver_preference_name());
    console_write(tr3("  Prozesse: ", "  Processes: ", "  Proses: "));
    console_write_dec((uint32_t)process_count());
    console_putc('\n');
    console_write(tr3("  Letzte Boot-Phase: ", "  Last boot stage: ", "  Tahap boot terakhir: "));
    console_writeln(recovery_last_boot_stage());
    console_write(tr3("  Safe Mode: ", "  Safe mode: ", "  Safe mode: "));
    console_writeln(recovery_safe_mode_enabled() != 0 ? tr3("aktiv", "enabled", "aktif") : tr3("aus", "off", "mati"));
    console_write(tr3("  Letzte Log-Eintraege: ", "  Recent log entries: ", "  Entri log terbaru: "));
    console_write_dec((uint32_t)syslog_count());
    console_putc('\n');
    if (recovery_last_issue()[0] != '\0') {
        console_write(tr3("  Offenes Problem: ", "  Open issue: ", "  Masalah terbuka: "));
        console_writeln(recovery_last_issue());
    } else {
        console_writeln(tr3("  Keine offenen Recovery-Warnungen.", "  No open recovery warnings.", "  Tidak ada peringatan recovery terbuka."));
    }
    log_action_simple("health", "system health", ACTIONLOG_OK);
}

static void command_logview(const char* args) {
    if (args == (const char*)0 || args[0] == '\0' || kstrcmp(args, "all") == 0) {
        print_syslog_filtered(SYSLOG_DEBUG, 0);
        log_action_simple("log", "all", ACTIONLOG_OK);
        return;
    }
    if (kstrcmp(args, "errors") == 0) {
        print_syslog_filtered(SYSLOG_ERROR, 0);
        log_action_simple("log", "errors", ACTIONLOG_OK);
        return;
    }
    if (kstrcmp(args, "warn") == 0 || kstrcmp(args, "warnings") == 0) {
        print_syslog_filtered(SYSLOG_WARN, 0);
        log_action_simple("log", "warnings", ACTIONLOG_OK);
        return;
    }
    if (kstrcmp(args, "boot") == 0) {
        print_syslog_filtered(SYSLOG_DEBUG, 1);
        log_action_simple("log", "boot", ACTIONLOG_OK);
        return;
    }
    if (kstrcmp(args, "clear") == 0) {
        if (needs_master("log clear") != 0) {
            return;
        }
        syslog_clear();
        (void)syslog_save_persistent();
        console_writeln(tr3("System-Log wurde geleert.", "System log was cleared.", "Log sistem dibersihkan."));
        log_action_simple("log.clear", "system log", ACTIONLOG_WARN);
        return;
    }
    console_writeln(tr3("Nutze: log [all|boot|warn|errors|clear]", "Use: log [all|boot|warn|errors|clear]", "Gunakan: log [all|boot|warn|errors|clear]"));
}

static void command_doctor(void) {
    console_writeln(tr3("Diagnose-Assistent:", "Doctor:", "Asisten diagnosis:"));
    print_diag();
    print_bootinfo_summary();
    if (network_nic_count() == 0U) {
        console_writeln(tr3("  Hinweis: Keine NIC erkannt. Pruefe die VM-Netzwerkkarte.", "  Hint: No NIC detected. Check the VM network adapter.", "  Petunjuk: NIC tidak ditemukan. Periksa adapter jaringan VM."));
    } else if (network_driver_active() == 0U) {
        console_writeln(tr3("  Hinweis: NIC erkannt, aber kein aktiver Treiber. Nutze 'netdriver <typ>' und danach 'netup'.", "  Hint: NIC detected but no active driver. Use 'netdriver <type>' and then 'netup'.", "  Petunjuk: NIC terdeteksi, tetapi belum ada driver aktif. Gunakan 'netdriver <type>' lalu 'netup'."));
    }
    if (app_persistence_available() == 0) {
        console_writeln(tr3("  Hinweis: Keine persistente Platte gefunden. savefs/loadfs, Log- und Recovery-Speicherung bleiben dann nur im RAM.", "  Hint: No persistent disk found. savefs/loadfs, log and recovery state stay in RAM only.", "  Petunjuk: Tidak ada disk persisten. savefs/loadfs, log, dan recovery hanya tinggal di RAM."));
    }
    if (recovery_safe_mode_enabled() != 0) {
        console_writeln(tr3("  Safe Mode ist aktiv. Das ist sinnvoll nach Boot-Problemen oder Treiberstress.", "  Safe mode is enabled. That is useful after boot problems or driver stress.", "  Safe mode aktif. Ini berguna setelah masalah boot atau driver."));
    }
    syslog_write(SYSLOG_INFO, "doctor", "system diagnostics viewed");
    log_action_simple("doctor", "system check", ACTIONLOG_OK);
}

static void command_recover(void) {
    recovery_clear_issue();
    (void)network_driver_shutdown();
    (void)network_set_dhcp(1);
    (void)network_set_ip("10.0.2.15");
    (void)network_set_gateway("10.0.2.2");
    user_drop();
    afs_set_home(user_current()->username);
    syslog_write(SYSLOG_WARN, "recovery", "safe baseline applied");
    console_writeln(tr3("Recovery-Basis angewendet: normale Rechte, DHCP an, aktiver Netzwerktreiber gestoppt.", "Recovery baseline applied: normal rights, DHCP on, active network driver stopped.", "Basis recovery diterapkan: hak normal, DHCP aktif, driver jaringan aktif dihentikan."));
    console_writeln(tr3("Nutze jetzt 'doctor', 'health', 'log boot' und danach bei Bedarf 'safemode on'.", "Now use 'doctor', 'health', 'log boot' and then 'safemode on' if needed.", "Sekarang gunakan 'doctor', 'health', 'log boot', lalu 'safemode on' bila perlu."));
    log_action_simple("recover", "baseline reset", ACTIONLOG_WARN);
}

static void command_safemode(const char* args) {
    if (args == (const char*)0 || args[0] == '\0' || kstrcmp(args, "status") == 0) {
        console_write(tr3("Safe Mode: ", "Safe mode: ", "Safe mode: "));
        console_writeln(recovery_safe_mode_enabled() != 0 ? tr3("aktiv", "enabled", "aktif") : tr3("aus", "off", "mati"));
        return;
    }
    if (kstrcmp(args, "on") == 0 || kstrcmp(args, "enable") == 0) {
        if (needs_master("safemode on") != 0) {
            return;
        }
        recovery_set_safe_mode(1);
        syslog_write(SYSLOG_WARN, "recovery", "safe mode enabled manually");
        console_writeln(tr3("Safe Mode wurde aktiviert und gilt auch fuer den naechsten Start.", "Safe mode was enabled and will also apply to the next boot.", "Safe mode diaktifkan dan juga berlaku untuk boot berikutnya."));
        log_action_simple("safemode.on", "persistent safe mode", ACTIONLOG_WARN);
        return;
    }
    if (kstrcmp(args, "off") == 0 || kstrcmp(args, "disable") == 0) {
        if (needs_master("safemode off") != 0) {
            return;
        }
        recovery_set_safe_mode(0);
        syslog_write(SYSLOG_INFO, "recovery", "safe mode disabled manually");
        console_writeln(tr3("Safe Mode wurde deaktiviert.", "Safe mode was disabled.", "Safe mode dinonaktifkan."));
        log_action_simple("safemode.off", "persistent safe mode", ACTIONLOG_OK);
        return;
    }
    console_writeln(tr3("Nutze: safemode [status|on|off]", "Use: safemode [status|on|off]", "Gunakan: safemode [status|on|off]"));
}

static void command_panic(const char* args) {
    if (args == (const char*)0 || *args == '\0' || kstrcmp(args, "test") == 0) {
        panic_show_message("Manual panic test", "This red screen was triggered by the shell command 'panic test'.");
    }

    console_writeln(tr3("Nutze: panic test", "Use: panic test", "Gunakan: panic test"));
}

static void command_proc(const char* args) {
    char action[16];
    char left[32];
    const process_t* proc;
    unsigned int pid;
    if (split_first_arg(args, action, sizeof(action)) != 0 || kstrcmp(action, "list") == 0) {
        print_processes();
        return;
    }
    if (kstrcmp(action, "spawn") == 0) {
        if (split_first_arg(args + 5, left, sizeof(left)) != 0) {
            console_writeln(tr("Nutze: proc spawn <name>", "Use: proc spawn <name>"));
            return;
        }
        if (process_spawn(PROCESS_KIND_PROGRAM, left, left, user_current()->username, 2U, 2U, &pid) == 0) {
            console_write(tr("Prozess gestartet mit PID #", "Process started with PID #"));
            console_write_dec(pid);
            console_putc('\n');
            log_action_simple("proc.spawn", left, ACTIONLOG_OK);
        } else {
            console_writeln(tr("Prozess konnte nicht gestartet werden.", "Could not start process."));
            log_action_simple("proc.spawn", left, ACTIONLOG_FAIL);
        }
        return;
    }
    if (kstrcmp(action, "info") == 0) {
        if (split_first_arg(args + 4, left, sizeof(left)) != 0) {
            console_writeln(tr("Nutze: proc info <pid>", "Use: proc info <pid>"));
            return;
        }
        proc = process_find((unsigned int)katoi(left));
        if (proc == (const process_t*)0) {
            console_writeln(tr("Diesen Prozess kenne ich nicht.", "I do not know that process."));
            return;
        }
        console_write(tr("Prozess #", "Process #"));
        console_write_dec(proc->pid);
        console_write(": ");
        console_writeln(proc->name);
        console_write(tr("  Zustand: ", "  State: "));
        console_writeln(process_state_name(proc->state));
        console_write(tr("  Typ: ", "  Kind: "));
        console_writeln(process_kind_name(proc->kind));
        console_write(tr("  Besitzer: ", "  Owner: "));
        console_writeln(proc->owner);
        console_write(tr("  Parent: ", "  Parent: "));
        console_write_dec(proc->ppid);
        console_putc('\n');
        console_write(tr("  Command: ", "  Command: "));
        console_writeln(proc->command);
        console_write(tr("  Region: ", "  Region: "));
        console_write_hex(proc->region_base);
        console_write(" - ");
        console_write_hex(proc->region_base + proc->region_size);
        console_putc('\n');
        console_write(tr("  Image: ", "  Image: "));
        console_write_hex(proc->image_base);
        console_write(tr(" | Heap: ", " | Heap: "));
        console_write_hex(proc->heap_base);
        console_write(tr(" | Stack top: ", " | Stack top: "));
        console_write_hex(proc->stack_top);
        console_putc('\n');
        console_write(tr("  Pages: ", "  Pages: "));
        console_write_dec(proc->mapped_page_count);
        console_write(tr(" | Runtime: ", " | Runtime: "));
        console_write_dec(proc->runtime_ticks);
        console_write(tr(" | Exit: ", " | Exit: "));
        console_write_dec((uint32_t)proc->exit_code);
        console_putc('\n');
        return;
    }
    if (split_first_arg(args + kstrlen(action), left, sizeof(left)) != 0) {
        console_writeln(tr("Nutze: proc <stop|resume|kill> <pid>", "Use: proc <stop|resume|kill> <pid>"));
        return;
    }
    pid = (unsigned int)katoi(left);
    proc = process_find(pid);
    if (proc == (const process_t*)0) {
        console_writeln(tr("Diesen Prozess kenne ich nicht.", "I do not know that process."));
        return;
    }
    if (can_manage_process_entry(proc) == 0) {
        console_writeln(tr("Nur Besitzer oder System-Modus duerfen diesen Prozess aendern.", "Only the owner or system mode may modify this process."));
        return;
    }
    if (kstrcmp(action, "stop") == 0) {
        if (process_stop(pid) == 0) {
            console_writeln(tr("Prozess angehalten.", "Process stopped."));
            log_action_simple("proc.stop", left, ACTIONLOG_OK);
        } else {
            console_writeln(tr("Prozess konnte nicht angehalten werden.", "Could not stop process."));
            log_action_simple("proc.stop", left, ACTIONLOG_FAIL);
        }
        return;
    }
    if (kstrcmp(action, "resume") == 0) {
        if (process_resume(pid) == 0) {
            console_writeln(tr("Prozess fortgesetzt.", "Process resumed."));
            log_action_simple("proc.resume", left, ACTIONLOG_OK);
        } else {
            console_writeln(tr("Prozess konnte nicht fortgesetzt werden.", "Could not resume process."));
            log_action_simple("proc.resume", left, ACTIONLOG_FAIL);
        }
        return;
    }
    if (kstrcmp(action, "kill") == 0) {
        if (process_kill(pid, 0) == 0) {
            console_writeln(tr("Prozess beendet.", "Process killed."));
            log_action_simple("proc.kill", left, ACTIONLOG_WARN);
        } else {
            console_writeln(tr("Prozess konnte nicht beendet werden.", "Could not kill process."));
            log_action_simple("proc.kill", left, ACTIONLOG_FAIL);
        }
        return;
    }
    console_writeln(tr("Nutze: proc <list|spawn|info|stop|resume|kill>", "Use: proc <list|spawn|info|stop|resume|kill>"));
}

static void run_command(const char* cmd) {
    if (program_guard_command(cmd) == 0) { return; }
    if (kstrcmp(cmd, "help") == 0) { show_help_overview(); return; }
    if (kstarts_with(cmd, "help ")) { command_help(cmd + 5); return; }
    if (kstrcmp(cmd, "quickstart") == 0) { print_quickstart(); return; }
    if (kstrcmp(cmd, "clear") == 0) { console_clear(); console_writeln(tr("Bildschirm geleert.", "Screen cleared.")); return; }
    if (kstrcmp(cmd, "apps") == 0 || kstrcmp(cmd, "modules") == 0) { print_apps(); return; }
    if (kstrcmp(cmd, "tasks") == 0 || kstrcmp(cmd, "services") == 0) { print_tasks(); return; }
    if (kstrcmp(cmd, "jobs") == 0) { print_jobs(); return; }
    if (kstrcmp(cmd, "ps") == 0 || kstrcmp(cmd, "processes") == 0) { print_processes(); return; }
    if (kstrcmp(cmd, "actionlog") == 0 || kstrcmp(cmd, "actions") == 0) { print_action_log(); return; }
    if (kstrcmp(cmd, "memory") == 0 || kstrcmp(cmd, "mem") == 0) { print_memory_status(); return; }
    if (kstrcmp(cmd, "paging") == 0 || kstrcmp(cmd, "vmem") == 0) { print_paging_status(); return; }
    if (kstrcmp(cmd, "history") == 0) { print_history(); return; }
    if (kstrcmp(cmd, "keytest") == 0 || kstrcmp(cmd, "keys") == 0) { keytest_open(); return; }
    if (kstrcmp(cmd, "status") == 0) { print_status(); return; }
    if (kstrcmp(cmd, "health") == 0) { command_health(); return; }
    if (kstrcmp(cmd, "bootinfo") == 0) { command_bootinfo(); return; }
    if (kstrcmp(cmd, "about") == 0) { show_about(); return; }
    if (kstrcmp(cmd, "version") == 0) { print_version(); return; }
    if (kstrcmp(cmd, "welcome") == 0 || kstrcmp(cmd, "banner") == 0) { shell_show_welcome(); return; }
    if (kstrcmp(cmd, "users") == 0) { print_users(); return; }
    if (kstrcmp(cmd, "whoami") == 0) { print_whoami(); return; }
    if (kstrcmp(cmd, "notes") == 0) { editor_list_documents(); return; }
    if (kstrcmp(cmd, "layout") == 0) { print_layout(); return; }
    if (kstarts_with(cmd, "layout ")) { set_layout(cmd + 7); return; }
    if (kstrcmp(cmd, "lang") == 0) { console_write("Language: "); console_writeln(current_language_code()); return; }
    if (kstarts_with(cmd, "lang ")) { set_language(cmd + 5); return; }
    if (kstarts_with(cmd, "view ")) { command_view(cmd + 5); return; }
    if (kstrcmp(cmd, "todo") == 0) { command_todo(""); return; }
    if (kstarts_with(cmd, "todo ")) { command_todo(cmd + 5); return; }
    if (kstarts_with(cmd, "calc ")) { command_calc(cmd + 5); return; }
    if (kstrcmp(cmd, "fs") == 0 || kstrcmp(cmd, "filesystem") == 0) { print_fs(); return; }
    if (kstrcmp(cmd, "settings") == 0 || kstrcmp(cmd, "preferences") == 0) { settings_open(); return; }
    if (kstrcmp(cmd, "settings general") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_GENERAL; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings network") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_NETWORK; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings security") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_SECURITY; settings_draw_list(); return; }
    if (kstrcmp(cmd, "settings expert") == 0) { settings_open(); g_settings_view = SETTINGS_VIEW_EXPERT; settings_draw_list(); return; }
    if (kstrcmp(cmd, "network") == 0 || kstrcmp(cmd, "net") == 0) { print_network_status(); return; }
    if (kstrcmp(cmd, "netdrivers") == 0) { command_netdrivers(); return; }
    if (kstrcmp(cmd, "games") == 0 || kstrcmp(cmd, "arcade") == 0) { command_games(); return; }
    if (kstarts_with(cmd, "game ")) { command_game(cmd + 5); return; }
    if (kstrcmp(cmd, "snake") == 0) { snake_open(); return; }
    if (kstrcmp(cmd, "game") == 0) { command_games(); return; }
    if (kstrcmp(cmd, "nic") == 0 || kstrcmp(cmd, "adapters") == 0) { print_nic_status(); return; }
    if (kstrcmp(cmd, "netup") == 0) { command_netup(); return; }
    if (kstarts_with(cmd, "netdriver ")) { command_netdriver(cmd + 10); return; }
    if (kstrcmp(cmd, "netdriver") == 0) { command_netdriver(""); return; }
    if (kstrcmp(cmd, "netprobe") == 0) { command_netprobe(); return; }
    if (kstrcmp(cmd, "mac") == 0) { command_mac(); return; }
    if (kstrcmp(cmd, "diag") == 0) { print_diag(); return; }
    if (kstrcmp(cmd, "doctor") == 0) { command_doctor(); return; }
    if (kstrcmp(cmd, "recover") == 0) { command_recover(); return; }
    if (kstrcmp(cmd, "panic") == 0) { command_panic(""); return; }
    if (kstarts_with(cmd, "panic ")) { command_panic(cmd + 6); return; }
    if (kstrcmp(cmd, "log") == 0 || kstrcmp(cmd, "logs") == 0) { command_logview(""); return; }
    if (kstarts_with(cmd, "log ")) { command_logview(cmd + 4); return; }
    if (kstrcmp(cmd, "safemode") == 0) { command_safemode(""); return; }
    if (kstarts_with(cmd, "safemode ")) { command_safemode(cmd + 9); return; }
    if (kstarts_with(cmd, "which ")) { command_which(cmd + 6); return; }
    if (kstrcmp(cmd, "disk") == 0 || kstrcmp(cmd, "storage") == 0) { print_disk_status(); return; }
    if (kstrcmp(cmd, "savefs") == 0 || kstrcmp(cmd, "sync") == 0) {
        int rc = save_system_state();
        if (rc == 0) { console_writeln(tr("Cyralith-Zustand wurde auf die virtuelle Platte gespeichert (Dateien, Nutzer, Netzwerk und Apps).", "Cyralith state was saved to the virtual disk (files, users, network and apps).")); log_action_simple("savefs", "system state", ACTIONLOG_OK); }
        else { console_writeln(tr("Speichern fehlgeschlagen. Wahrscheinlich ist keine virtuelle Platte angeschlossen.", "Save failed. There is probably no virtual disk attached.")); recovery_note_issue("savefs failed"); log_action_simple("savefs", "system state", ACTIONLOG_FAIL); }
        return;
    }
    if (kstrcmp(cmd, "loadfs") == 0) {
        int rc = load_system_state();
        if (rc == 0) { console_writeln(tr("Cyralith-Zustand wurde von der virtuellen Platte geladen.", "Cyralith state was loaded from the virtual disk.")); network_rescan(); log_action_simple("loadfs", "system state", ACTIONLOG_OK); }
        else { console_writeln(tr("Laden fehlgeschlagen. Vielleicht gibt es noch keinen gespeicherten Stand.", "Load failed. There may be no saved state yet.")); recovery_note_issue("loadfs failed"); log_action_simple("loadfs", "system state", ACTIONLOG_FAIL); }
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
    if (kstrcmp(cmd, "shutdown") == 0 || kstrcmp(cmd, "poweroff") == 0 || kstrcmp(cmd, "halt") == 0) { shell_shutdown(); return; }
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
            log_action_simple("elevate", "system mode on", ACTIONLOG_OK);
        } else {
            console_writeln(tr("System-Modus konnte nicht aktiviert werden.", "Could not activate system mode."));
            recovery_note_issue("elevate failed");
            log_action_simple("elevate", "system mode on", ACTIONLOG_FAIL);
        }
        return;
    }
    if (kstrcmp(cmd, "drop") == 0) {
        user_drop();
        afs_set_home(user_current()->username);
        console_writeln(tr("Normale Rechte wieder aktiv.", "Normal rights are active again."));
        log_action_simple("drop", "system mode off", ACTIONLOG_OK);
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
    if (kstrcmp(cmd, "pkg") == 0) { command_pkg("list"); return; }
    if (kstarts_with(cmd, "app ")) { command_app(cmd + 4); return; }
    if (kstarts_with(cmd, "prog ")) { command_prog(cmd + 5); return; }
    if (kstarts_with(cmd, "pkg ")) { command_pkg(cmd + 4); return; }
    if (kstarts_with(cmd, "cmd ")) { command_cmd(cmd + 4); return; }
    if (kstarts_with(cmd, "job ")) { command_job(cmd + 4); return; }
    if (kstarts_with(cmd, "proc ")) { command_proc(cmd + 5); return; }
    if (kstarts_with(cmd, "launch ")) { command_launch(cmd + 7); return; }
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
    ai_bind_runner(run_command);
    automation_bind_runner(run_command);
    log_action_simple("shell.init", "ready", ACTIONLOG_OK);
    print_quickstart();
    console_writeln("");
    prompt();
}

void shell_poll(void) {
    if (snake_is_active() != 0) {
        snake_poll();
        return;
    }
    if (g_settings_active != 0) {
        settings_poll();
    }
}

void shell_handle_key(int key) {
    if (key == KEY_NONE) {
        return;
    }

    if (g_keytest_active != 0) {
        keytest_handle(key);
        return;
    }

    if (editor_is_active() != 0) {
        editor_handle_key(key);
        if (editor_is_active() == 0) {
            prompt();
        }
        return;
    }

    if (snake_is_active() != 0) {
        snake_handle_key(key);
        if (snake_is_active() == 0) {
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
