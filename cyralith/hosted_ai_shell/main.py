#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List

VERSION = "Cyralith Stable Build 2.7.0"
CREDIT = "Programmiert von Obsidian"


@dataclass
class App:
    name: str
    installed: bool
    builtin: bool
    description_de: str
    description_en: str
    description_id: str


@dataclass
class SystemState:
    language: str = "de"
    current_user: str = "guest"
    system_mode: bool = False
    current_path: str = "/home/guest"
    heap_total: int = 64 * 1024
    heap_used: int = 8 * 1024
    ticks: int = 1500
    hostname: str = "cyralith"
    ip: str = "127.0.0.1"
    gateway: str = "-"
    layout: str = "DE / QWERTZ"
    dhcp: bool = False
    driver: str = "off"
    mac: str = "unavailable"
    link_up: bool = False
    nics: List[str] = field(default_factory=lambda: ["Intel PRO/1000 82540EM (PCI 00:03.0)"])
    ai_smart: bool = True
    startup_hints: bool = True
    require_program_approval: bool = True
    allow_legacy_commands: bool = True
    default_autoapprove: bool = False
    expert_mode: bool = False
    driver_autostart: bool = False
    driver_debug: bool = False
    autosave: bool = False
    verbose_diag: bool = False
    apps: Dict[str, App] = field(default_factory=dict)
    history: List[str] = field(default_factory=list)
    custom_commands: Dict[str, str] = field(default_factory=dict)
    programs: Dict[str, dict] = field(default_factory=dict)

    @property
    def heap_free(self) -> int:
        return self.heap_total - self.heap_used


class CyralithShell:
    def __init__(self) -> None:
        self.state = SystemState(
            apps={
                "lumen": App("lumen", True, True, "Nano-artiger Texteditor", "Nano-like text editor", "Editor teks mirip nano"),
                "files": App("files", True, True, "Dateibereich", "File area", "Area file"),
                "settings": App("settings", True, True, "Grundlegende Einstellungen", "Basic settings", "Pengaturan dasar"),
                "network": App("network", True, True, "Netzwerk- und Adapteruebersicht", "Network and adapter view", "Ringkasan jaringan dan adaptor"),
                "monitor": App("monitor", True, True, "Systemueberblick", "System overview", "Ringkasan sistem"),
                "browser": App("browser", False, False, "Web-Platzhalter", "Web placeholder", "Placeholder web"),
                "desktop": App("desktop", False, False, "Desktop-Platzhalter", "Desktop placeholder", "Placeholder desktop"),
                "paint": App("paint", False, False, "Zeichen-Platzhalter", "Drawing placeholder", "Placeholder menggambar"),
            }
        )
        self.passwords = {"guest": "guest", "system": "cyralith"}
        self.homes = {"guest": "/home/guest"}
        self.running = True
        self.commands: Dict[str, Callable[[List[str]], None]] = {
            "help": self.cmd_help,
            "quickstart": self.cmd_quickstart,
            "status": self.cmd_status,
            "history": self.cmd_history,
            "whoami": self.cmd_whoami,
            "users": self.cmd_users,
            "login": self.cmd_login,
            "elevate": self.cmd_elevate,
            "drop": self.cmd_drop,
            "passwd": self.cmd_passwd,
            "pwd": self.cmd_pwd,
            "fs": self.cmd_fs,
            "settings": self.cmd_settings,
            "network": self.cmd_network,
            "net": self.cmd_network,
            "nic": self.cmd_nic,
            "netup": self.cmd_netup,
            "netprobe": self.cmd_netprobe,
            "mac": self.cmd_mac,
            "diag": self.cmd_diag,
            "ping": self.cmd_ping,
            "apps": self.cmd_apps,
            "app": self.cmd_app,
            "cmd": self.cmd_cmd,
            "prog": self.cmd_prog,
            "which": self.cmd_which,
            "version": self.cmd_version,
            "welcome": self.cmd_welcome,
            "about": self.cmd_about,
            "ai": self.cmd_ai,
            "lang": self.cmd_lang,
            "clear": self.cmd_clear,
            "exit": self.cmd_exit,
        }

    def t(self, de: str, en: str, id: str | None = None) -> str:
        if self.state.language == "de":
            return de
        if self.state.language == "id":
            return id if id is not None else en
        return en

    def prompt(self) -> str:
        tail = ""
        home = self.homes[self.state.current_user]
        if self.state.current_path != home:
            tail = self.state.current_path[len(home):] if self.state.current_path.startswith(home) else self.state.current_path
        return f"cyralith({self.state.language})@{self.state.current_user}{tail}{'# ' if self.state.system_mode else '> '}"

    def print_banner(self) -> None:
        print("============================================================")
        print(VERSION)
        print(self.t("Danke, dass du Cyralith nutzt!", "Thank you for using Cyralith!", "Terima kasih sudah menggunakan Cyralith!"))
        print(self.t("Tippe 'help' fuer Hilfe.", "Type 'help' for help.", "Ketik 'help' untuk bantuan."))
        print(self.t("Programmiert von Obsidian.", "Programmed by Obsidian.", "Diprogram oleh Obsidian."))
        print("============================================================")

    def run(self) -> None:
        self.print_banner()
        print(self.t("Shell bereit. Apps, Nutzer, CyralithFS, Netzwerk, e1000-Grundlage und die neue Settings-App sind aktiv.", "Shell ready. Apps, users, CyralithFS, network, e1000 groundwork and the new settings app are active.", "Shell siap. Aplikasi, pengguna, CyralithFS, jaringan, dasar e1000, dan aplikasi Settings baru aktif."))
        self.cmd_quickstart([])
        while self.running:
            try:
                raw = input(self.prompt()).strip()
            except EOFError:
                print()
                break
            if not raw:
                continue
            self.state.history.append(raw)
            self.state.history = self.state.history[-20:]
            self.state.ticks += 1
            parts = raw.split()
            cmd = parts[0].lower()
            args = parts[1:]
            if cmd in self.commands:
                self.commands[cmd](args)
            else:
                print(self.t("Nicht direkt erkannt. Ich versuche die AI-Hilfe ...", "Not recognized directly. I will try AI help ...", "Tidak dikenali langsung. Saya akan mencoba bantuan AI ..."))
                self.cmd_ai(parts)

    def _needs_system(self) -> bool:
        if self.state.system_mode:
            return False
        print(self.t("Dafuer brauchst du System-Rechte. Nutze 'elevate <passwort>'.", "You need system rights. Use 'elevate <password>'.", "Kamu butuh hak sistem. Gunakan 'elevate <password>'."))
        return True

    def cmd_help(self, _: List[str]) -> None:
        rows = [
            ("help", "Zeigt diese Hilfe.", "Shows this help.", "Menampilkan bantuan ini."),
            ("quickstart", "Zeigt einen schnellen Einstieg.", "Shows a quick start.", "Menampilkan awal cepat."),
            ("status", "Kurzer Systemueberblick.", "Short system overview.", "Ringkasan sistem singkat."),
            ("diag", "Kurze Diagnose fuer System und Netzwerk.", "Short diagnosis for system and network.", "Diagnosis singkat untuk sistem dan jaringan."),
            ("whoami", "Zeigt den aktuellen Benutzer.", "Shows the current user.", "Menampilkan pengguna saat ini."),
            ("users", "Zeigt Nutzer und System-Modus.", "Shows users and system mode.", "Menampilkan pengguna dan mode sistem."),
            ("fs", "Erklaert CyralithFS kurz.", "Explains CyralithFS briefly.", "Menjelaskan CyralithFS secara singkat."),
            ("network", "Zeigt Netzwerk-Status.", "Shows network status.", "Menampilkan status jaringan."),
            ("nic", "Zeigt erkannte Netzwerkadapter.", "Shows detected network adapters.", "Menampilkan adaptor jaringan yang terdeteksi."),
            ("netup", "Startet den e1000-Pilottreiber.", "Starts the e1000 pilot driver.", "Memulai driver pilot e1000."),
            ("netprobe", "Sendet einen kleinen Rohdaten-Test.", "Sends a small raw packet test.", "Mengirim tes paket mentah kecil."),
            ("mac", "Zeigt die aktive MAC-Adresse.", "Shows the active MAC address.", "Menampilkan alamat MAC aktif."),
            ("app list", "Zeigt eingebaute und optionale Apps.", "Shows built-in and optional apps.", "Menampilkan aplikasi bawaan dan opsional."),
            ("app info <name>", "Zeigt Details zu einer App.", "Shows app details.", "Menampilkan detail aplikasi."),
            ("cmd new <name>", "Erstellt einen eigenen Befehl.", "Creates your own command.", "Membuat perintah sendiri."),
            ("prog info <name>", "Zeigt Rechte und Vertrauen eines Programms.", "Shows permissions and trust of a program.", "Menampilkan izin dan kepercayaan sebuah program."),
            ("which <name>", "Zeigt, woher ein Befehl kommt.", "Shows where a command comes from.", "Menampilkan asal sebuah perintah."),
            ("app install <name>", "Installiert optionale Apps im System-Modus.", "Installs optional apps in system mode.", "Memasang aplikasi opsional dalam mode sistem."),
            ("app run <name>", "Startet eine App oder ihren Platzhalter.", "Starts an app or its placeholder.", "Menjalankan aplikasi atau placeholder-nya."),
            ("settings", "Oeffnet die neue Einstellungszentrale mit Kategorien.", "Opens the new categorized settings center.", "Membuka pusat pengaturan baru dengan kategori."),
            ("lang <de|en|id>", "Wechselt die Sprache.", "Changes the language.", "Mengganti bahasa."),
            ("ping 127.0.0.1", "Testet Loopback.", "Tests loopback.", "Menguji loopback."),
            ("elevate <pw>", "Aktiviert den root-aehnlichen System-Modus.", "Activates the root-like system mode.", "Mengaktifkan mode sistem mirip root."),
            ("about", "Erklaert kurz die Idee hinter Cyralith.", "Explains the idea behind Cyralith.", "Menjelaskan ide di balik Cyralith."),
            ("ai <text>", "Nimmt normale Sprache entgegen.", "Accepts natural language.", "Menerima bahasa alami."),
            ("exit", "Beendet die Demo.", "Exits the demo.", "Menutup demo."),
        ]
        print(self.t("Cyralith Hilfe", "Cyralith help", "Bantuan Cyralith"))
        for cmd, de, en, id_text in rows:
            print(f"  {cmd:18} - {self.t(de, en, id_text)}")

    def cmd_quickstart(self, _: List[str]) -> None:
        lines = [
            "help",
            "status",
            "diag",
            "app list",
            "network",
            "nic",
            "netup",
            "netprobe",
            "settings",
            "settings expert",
            "elevate cyralith",
            "app install browser",
            "cmd new hallo",
            "prog info hallo",
        ]
        print(self.t("Schnellstart:", "Quick start:", "Mulai cepat:"))
        for item in lines:
            print(f"  {item}")

    def cmd_status(self, _: List[str]) -> None:
        installed = sum(1 for app in self.state.apps.values() if app.installed)
        print(self.t("Kurzueberblick:", "Quick overview:", "Ringkasan singkat:"))
        print(f"  {self.t('Benutzer', 'User', 'Pengguna')}: {self.state.current_user} [{'system' if self.state.system_mode else 'user'}]")
        print(f"  {self.t('Ort', 'Path', 'Lokasi')}: {self.state.current_path}")
        print(f"  {self.t('Freier Speicher', 'Free memory', 'Memori kosong')}: {self.state.heap_free} bytes")
        print(f"  {self.t('Netzwerk', 'Network', 'Jaringan')}: {self.state.hostname} / {self.state.ip} / NICs={len(self.state.nics)} / Driver={self.state.driver}")
        print(f"  {self.t('Expertenmodus', 'Expert mode', 'Mode ahli')}: {'on' if self.state.expert_mode else 'off'}")
        print(f"  {self.t('Installierte Apps', 'Installed apps', 'Aplikasi terpasang')}: {installed}")
        print(f"  {self.t('Ticks seit Start', 'Ticks since boot', 'Tick sejak boot')}: {self.state.ticks}")

    def cmd_diag(self, _: List[str]) -> None:
        print(self.t("Diagnose:", "Diagnostics:", "Diagnosis:"))
        if self.state.verbose_diag:
            print(self.t("  Modus: ausfuehrlich", "  Mode: verbose"))
        print("  Storage: RAM/demo")
        print(f"  NICs: {len(self.state.nics)}")
        print(f"  Driver: {self.state.driver}")
        print(f"  MAC: {self.state.mac}")
        print(f"  Link: {'up' if self.state.link_up else 'off'}")
        print(f"  Expert: {'on' if self.state.expert_mode else 'off'}")
        print(f"  Apps: {sum(1 for app in self.state.apps.values() if app.installed)}")

    def cmd_history(self, _: List[str]) -> None:
        print(self.t("Letzte Befehle:", "Recent commands:", "Perintah terakhir:"))
        if not self.state.history:
            print(self.t("  Noch keine Befehle.", "  No commands yet.", "  Belum ada perintah."))
            return
        for idx, item in enumerate(self.state.history, 1):
            print(f"  {idx}. {item}")

    def cmd_whoami(self, _: List[str]) -> None:
        print(f"{self.t('Aktiver Benutzer', 'Current user', 'Pengguna aktif')}: {self.state.current_user}")
        print(f"Home: {self.homes[self.state.current_user]}")
        print(self.t("Sitzung: System-Modus aktiv" if self.state.system_mode else "Sitzung: Normale Rechte", "Session: system mode active" if self.state.system_mode else "Session: normal rights", "Sesi: mode sistem aktif" if self.state.system_mode else "Sesi: hak normal"))

    def cmd_users(self, _: List[str]) -> None:
        print(self.t("Bekannte Benutzer:", "Known users:", "Pengguna yang dikenal:"))
        print("  - guest [user] /home/guest")
        print(self.t("  - system-mode [special] root-aehnlich per elevate <passwort>", "  - system-mode [special] root-like via elevate <password>", "  - system-mode [khusus] mirip root lewat elevate <password>"))

    def cmd_login(self, args: List[str]) -> None:
        if not args or args[0] != "guest":
            print(self.t("Derzeit gibt es in der Demo nur 'guest'.", "At the moment the demo only has 'guest'.", "Saat ini demo hanya punya 'guest'."))
            return
        self.state.current_user = "guest"
        self.state.current_path = self.homes["guest"]
        self.state.system_mode = False
        print(self.t("Benutzer gewechselt zu guest", "Switched user to guest", "Pengguna diganti ke guest"))

    def cmd_elevate(self, args: List[str]) -> None:
        if not args or args[0] != self.passwords["system"]:
            print(self.t("Nutze: elevate <passwort>", "Use: elevate <password>"))
            return
        self.state.system_mode = True
        print(self.t("System-Rechte aktiv.", "System rights active."))

    def cmd_drop(self, _: List[str]) -> None:
        self.state.system_mode = False
        print(self.t("Normale Rechte wieder aktiv.", "Normal rights active again."))

    def cmd_passwd(self, args: List[str]) -> None:
        if len(args) != 1:
            print(self.t("Nutze: passwd <neu>", "Use: passwd <new>"))
            return
        self.passwords[self.state.current_user] = args[0]
        print(self.t("Passwort geaendert.", "Password changed."))

    def cmd_pwd(self, _: List[str]) -> None:
        print(self.state.current_path)

    def cmd_fs(self, _: List[str]) -> None:
        print(self.t("CyralithFS im Ueberblick:", "CyralithFS overview:"))
        print("  /system  /home  /apps  /temp")
        print(self.t("  UNIX-inspiriert, aber ein eigenes CyralithFS.", "  UNIX-inspired, but its own CyralithFS."))
        print(self.t("  In der Demo lebt es im RAM; im Kernel gibt es eine fruehe Persistenzbasis.", "  In the demo it lives in RAM; in the kernel there is early persistence groundwork."))

    def _settings_header(self, title: str, subtitle: str) -> None:
        print("+==============================================================================+")
        print("|                              Cyralith Settings                               |")
        print("+==============================================================================+")
        print(f"| {title:<76}|")
        print(f"| {subtitle:<76}|")
        print("+------------------------------------------------------------------------------+")

    def cmd_settings(self, args: List[str]) -> None:
        page = args[0].lower() if args else "home"
        while True:
            if page == "home":
                self._settings_header(
                    self.t("Uebersicht", "Overview"),
                    self.t("Wie menuconfig, aber uebersichtlicher und leichter verstaendlich.", "Like menuconfig, but clearer and easier to understand."),
                )
                print(f"  1. {self.t('Allgemein', 'General'):22} : {('Deutsch / DE' if self.state.language == 'de' else ('Bahasa Indonesia / ID' if self.state.language == 'id' else 'English / EN'))}")
                print(f"  2. {self.t('Netzwerk', 'Network'):22} : {self.state.hostname}")
                print(f"  3. {self.t('Sicherheit', 'Security'):22} : {self.t('streng', 'strict') if self.state.require_program_approval else self.t('locker', 'relaxed')}")
                print(f"  4. {self.t('Expertenmodus', 'Expert mode'):22} : {'on' if self.state.expert_mode else 'off'}")
                print(f"  5. {self.t('Treiber & Tiefes', 'Drivers & low-level'):22} : {self.t('verfuegbar', 'available') if self.state.expert_mode else self.t('erst aktivieren', 'enable first')}")
                print(f"  6. {self.t('Speichern', 'Save'):22} : {self.t('Demo', 'Demo')}")
                print(f"  7. {self.t('Schliessen', 'Close'):22}")
                print("+------------------------------------------------------------------------------+")
                choice = input("settings> ").strip().lower()
                if choice in {'', '7', 'q', 'quit', 'exit'}:
                    break
                if choice == '1':
                    page = 'general'
                elif choice == '2':
                    page = 'network'
                elif choice == '3':
                    page = 'security'
                elif choice == '4':
                    self.state.expert_mode = not self.state.expert_mode
                    print(self.t("Expertenmodus umgeschaltet.", "Expert mode toggled."))
                elif choice == '5':
                    if not self.state.expert_mode:
                        print(self.t("Schalte zuerst den Expertenmodus ein.", "Enable expert mode first."))
                    else:
                        page = 'expert'
                elif choice == '6':
                    print(self.t("Demo: Einstellungen wuerden jetzt gespeichert.", "Demo: Settings would now be saved."))
                else:
                    print(self.t("Unbekannte Auswahl.", "Unknown choice."))
                continue

            if page == 'general':
                self._settings_header(self.t('Allgemein', 'General'), self.t('Sprache, Tastatur und Verhalten.', 'Language, keyboard and behavior.'))
                print(f"  1. {self.t('Sprache', 'Language'):22} : {'Deutsch' if self.state.language == 'de' else ('Bahasa Indonesia' if self.state.language == 'id' else 'English')}")
                print(f"  2. {self.t('Tastatur-Layout', 'Keyboard layout'):22} : {self.state.layout}")
                print(f"  3. {self.t('KI-Hilfe', 'AI help'):22} : {self.t('smart', 'smart') if self.state.ai_smart else self.t('einfach', 'basic')}")
                print(f"  4. {self.t('Start-Hinweise', 'Startup hints'):22} : {'on' if self.state.startup_hints else 'off'}")
                print(f"  5. {self.t('Zurueck', 'Back'):22}")
                choice = input("settings/general> ").strip().lower()
                if choice in {'', '5', 'q', 'back'}:
                    page = 'home'
                elif choice == '1':
                    self.state.language = 'en' if self.state.language == 'de' else ('id' if self.state.language == 'en' else 'de')
                    print(self.t("Sprache umgeschaltet.", "Language switched.", "Bahasa diganti."))
                elif choice == '2':
                    self.state.layout = 'US / QWERTY' if self.state.layout.startswith('DE') else 'DE / QWERTZ'
                    print(self.t("Tastatur-Layout umgeschaltet.", "Keyboard layout switched."))
                elif choice == '3':
                    self.state.ai_smart = not self.state.ai_smart
                    print(self.t("KI-Hilfe umgeschaltet.", "AI help switched."))
                elif choice == '4':
                    self.state.startup_hints = not self.state.startup_hints
                    print(self.t("Start-Hinweise umgeschaltet.", "Startup hints switched."))
                else:
                    print(self.t("Unbekannte Auswahl.", "Unknown choice."))
                continue

            if page == 'network':
                self._settings_header(self.t('Netzwerk', 'Network'), self.t('Name, DHCP und Adressen.', 'Name, DHCP and addresses.'))
                print(f"  1. {self.t('Computername', 'Computer name'):22} : {self.state.hostname}")
                print(f"  2. {'DHCP':22} : {'on' if self.state.dhcp else 'off'}")
                print(f"  3. {'IP':22} : {self.state.ip}")
                print(f"  4. {self.t('Gateway', 'Gateway'):22} : {self.state.gateway}")
                print(f"  5. {self.t('Zurueck', 'Back'):22}")
                choice = input("settings/network> ").strip().lower()
                if choice in {'', '5', 'q', 'back'}:
                    page = 'home'
                elif choice == '1':
                    if self._needs_system():
                        continue
                    value = input(self.t("Neuer Computername: ", "New computer name: ")).strip()
                    if value:
                        self.state.hostname = value
                        print(self.t("Computername aktualisiert.", "Computer name updated."))
                elif choice == '2':
                    if self._needs_system():
                        continue
                    self.state.dhcp = not self.state.dhcp
                    print(self.t("DHCP umgeschaltet.", "DHCP switched."))
                elif choice == '3':
                    if self._needs_system():
                        continue
                    value = input("New IP: ").strip()
                    if value:
                        self.state.ip = value
                        self.state.dhcp = False
                        print(self.t("IP aktualisiert.", "IP updated."))
                elif choice == '4':
                    if self._needs_system():
                        continue
                    value = input(self.t("Neues Gateway: ", "New gateway: ")).strip()
                    if value:
                        self.state.gateway = value
                        print(self.t("Gateway aktualisiert.", "Gateway updated."))
                else:
                    print(self.t("Unbekannte Auswahl.", "Unknown choice."))
                continue

            if page == 'security':
                self._settings_header(self.t('Sicherheit', 'Security'), self.t('Freigaben und Regeln fuer eigene Programme.', 'Approvals and rules for your own programs.'))
                print(f"  1. {self.t('Programm-Freigaben', 'Program approvals'):22} : {self.t('Pflicht', 'required') if self.state.require_program_approval else self.t('optional lokal', 'optional local')}")
                print(f"  2. {self.t('Legacy-Befehle', 'Legacy commands'):22} : {'on' if self.state.allow_legacy_commands else 'off'}")
                print(f"  3. {self.t('Auto-Freigabe neu', 'Auto-approve new'):22} : {'on' if self.state.default_autoapprove else 'off'}")
                print(f"  4. {self.t('Zurueck', 'Back'):22}")
                choice = input("settings/security> ").strip().lower()
                if choice in {'', '4', 'q', 'back'}:
                    page = 'home'
                elif choice == '1':
                    if self._needs_system():
                        continue
                    self.state.require_program_approval = not self.state.require_program_approval
                    print(self.t("Programm-Freigaben umgeschaltet.", "Program approvals switched."))
                elif choice == '2':
                    if self._needs_system():
                        continue
                    self.state.allow_legacy_commands = not self.state.allow_legacy_commands
                    print(self.t("Legacy-Befehle umgeschaltet.", "Legacy command policy switched."))
                elif choice == '3':
                    if self._needs_system():
                        continue
                    self.state.default_autoapprove = not self.state.default_autoapprove
                    print(self.t("Auto-Freigabe umgeschaltet.", "Auto-approval switched."))
                else:
                    print(self.t("Unbekannte Auswahl.", "Unknown choice."))
                continue

            if page == 'expert':
                self._settings_header(self.t('Expertenmodus', 'Expert mode'), self.t('Treiber, Auto-Save und tiefere Diagnose.', 'Drivers, auto-save and deeper diagnostics.'))
                print(f"  1. {self.t('Netzwerk-Autostart', 'Network auto-start'):22} : {'on' if self.state.driver_autostart else 'off'}")
                print(f"  2. {self.t('Treiber-Diagnose', 'Driver diagnostics'):22} : {'on' if self.state.driver_debug else 'off'}")
                print(f"  3. {self.t('Auto-Save', 'Auto-save'):22} : {'on' if self.state.autosave else 'off'}")
                print(f"  4. {self.t('Verbose Diagnose', 'Verbose diagnostics'):22} : {'on' if self.state.verbose_diag else 'off'}")
                print(f"  5. {self.t('Zurueck', 'Back'):22}")
                choice = input("settings/expert> ").strip().lower()
                if choice in {'', '5', 'q', 'back'}:
                    page = 'home'
                elif choice == '1':
                    if self._needs_system():
                        continue
                    self.state.driver_autostart = not self.state.driver_autostart
                    print(self.t("Netzwerk-Autostart umgeschaltet.", "Network auto-start switched."))
                elif choice == '2':
                    if self._needs_system():
                        continue
                    self.state.driver_debug = not self.state.driver_debug
                    print(self.t("Treiber-Diagnose umgeschaltet.", "Driver diagnostics switched."))
                elif choice == '3':
                    if self._needs_system():
                        continue
                    self.state.autosave = not self.state.autosave
                    print(self.t("Auto-Save umgeschaltet.", "Auto-save switched."))
                elif choice == '4':
                    if self._needs_system():
                        continue
                    self.state.verbose_diag = not self.state.verbose_diag
                    print(self.t("Verbose Diagnose umgeschaltet.", "Verbose diagnostics switched."))
                else:
                    print(self.t("Unbekannte Auswahl.", "Unknown choice."))
                continue

            page = 'home'

    def cmd_layout(self, args: List[str]) -> None:
        if not args or args[0] not in {"de", "us"}:
            print(self.t("Nutze: layout <de|us>", "Use: layout <de|us>"))
            return
        print(self.t(f"Layout gesetzt: {args[0]}", f"Layout set: {args[0]}"))

    def cmd_lang(self, args: List[str]) -> None:
        if not args or args[0] not in {"de", "en", "id"}:
            print(self.t("Nutze: lang <de|en|id>", "Use: lang <de|en|id>", "Gunakan: lang <de|en|id>"))
            return
        self.state.language = args[0]
        if args[0] == "de":
            print("Sprache gesetzt: Deutsch")
        elif args[0] == "en":
            print("Language set: English")
        else:
            print("Bahasa diatur: Indonesia")

    def cmd_network(self, _: List[str]) -> None:
        print(self.t("CyralithNet im Ueberblick:", "CyralithNet overview:"))
        print(f"  Backend: {'CyralithNet + e1000 pilot driver' if self.state.driver != 'off' else 'CyralithNet + PCI scan'}")
        print(f"  Hostname: {self.state.hostname}")
        print(f"  Address: {self.state.ip}")
        print(f"  Gateway: {self.state.gateway}")
        print(f"  DHCP: {'on' if self.state.dhcp else 'off'}")
        print(f"  {self.t('Erkannte NICs', 'Detected NICs')}: {len(self.state.nics)}")
        print(f"  {self.t('Treiber', 'Driver')}: {self.state.driver}")
        print(f"  MAC: {self.state.mac}")
        print(f"  {self.t('Link', 'Link')}: {'up' if self.state.link_up else 'off'}")
        if self.state.expert_mode:
            print(f"  {self.t('Expertenmodus', 'Expert mode')}: {'driver debug on' if self.state.driver_debug else 'compact'}")

    def cmd_nic(self, _: List[str]) -> None:
        print(self.t("Erkannte Netzwerkadapter:", "Detected network adapters:"))
        if not self.state.nics:
            print(self.t("  Keine erkannt.", "  None detected."))
            return
        for item in self.state.nics:
            print(f"  - {item}")
        if self.state.driver == "off":
            print(self.t("  Noch kein Treiber aktiv. Nutze netup.", "  No driver active yet. Use netup."))

    def cmd_netup(self, _: List[str]) -> None:
        if not self.state.nics:
            print(self.t("Keine passende NIC erkannt.", "No fitting NIC detected."))
            return
        self.state.driver = "Intel e1000 pilot driver"
        self.state.mac = "52:54:00:12:34:56"
        self.state.link_up = True
        print(self.t("e1000-Treiber gestartet.", "e1000 driver started."))
        if self.state.driver_debug:
            print(self.t("Treiber-Diagnose: MMIO initialisiert, MAC gelesen, Link aktiv.", "Driver diagnostics: MMIO initialized, MAC read, link active."))

    def cmd_netprobe(self, _: List[str]) -> None:
        if self.state.driver == "off":
            print(self.t("Noch kein aktiver Netzwerktreiber. Nutze zuerst netup.", "No active network driver yet. Use netup first."))
            return
        print(self.t("Rohdaten-Testframe gesendet.", "Raw probe frame sent."))

    def cmd_mac(self, _: List[str]) -> None:
        print(f"MAC: {self.state.mac}")

    def cmd_hostname(self, args: List[str]) -> None:
        if not args:
            print(self.state.hostname)
            return
        if self._needs_system():
            return
        self.state.hostname = args[0]
        print(self.t("Hostname aktualisiert.", "Hostname updated."))

    def cmd_dhcp(self, args: List[str]) -> None:
        if not args:
            print("on" if self.state.dhcp else "off")
            return
        if self._needs_system():
            return
        if args[0] == "on":
            self.state.dhcp = True
            self.state.ip = "10.0.2.15"
            self.state.gateway = "10.0.2.2"
        elif args[0] == "off":
            self.state.dhcp = False
        else:
            print(self.t("Nutze: dhcp <on|off>", "Use: dhcp <on|off>"))
            return
        print(self.t("DHCP aktualisiert.", "DHCP updated."))

    def cmd_ip(self, args: List[str]) -> None:
        if not args:
            print(self.state.ip)
            return
        if len(args) == 2 and args[0] == "set":
            if self._needs_system():
                return
            self.state.ip = args[1]
            self.state.dhcp = False
            print(self.t("IP-Adresse aktualisiert.", "IP updated."))
            return
        print(self.t("Nutze: ip oder ip set <adresse>", "Use: ip or ip set <address>"))

    def cmd_gateway(self, args: List[str]) -> None:
        if not args:
            print(self.state.gateway)
            return
        if len(args) == 2 and args[0] == "set":
            if self._needs_system():
                return
            self.state.gateway = args[1]
            print(self.t("Gateway aktualisiert.", "Gateway updated."))
            return
        print(self.t("Nutze: gateway oder gateway set <adresse>", "Use: gateway or gateway set <address>"))

    def cmd_ping(self, args: List[str]) -> None:
        if len(args) != 1:
            print(self.t("Nutze: ping <ziel>", "Use: ping <target>"))
            return
        target = args[0]
        if target in {"127.0.0.1", "localhost"}:
            print(f"PING {target}: loopback reachable (1 ms)")
            return
        if self.state.driver == "off":
            print(f"PING {target}: NIC erkannt, aber noch kein aktiver Treiber")
        else:
            print(f"PING {target}: e1000 driver active, use netprobe for a raw TX test")

    def cmd_apps(self, _: List[str]) -> None:
        self.cmd_app(["list"])

    def cmd_app(self, args: List[str]) -> None:
        if not args or args[0] == "list":
            print(self.t("Apps:", "Apps:"))
            for app in self.state.apps.values():
                status = self.t("installiert", "installed") if app.installed else self.t("optional", "optional")
                builtin = self.t(", intern", ", built-in") if app.builtin else ""
                desc = app.description_de if self.state.language == "de" else (app.description_id if self.state.language == "id" else app.description_en)
                print(f"  - {app.name} [{status}{builtin}] {desc}")
            return
        if len(args) < 2:
            print(self.t("Nutze: app <install|remove|info|run> <name>", "Use: app <install|remove|info|run> <name>"))
            return
        action, name = args[0], args[1]
        app = self.state.apps.get(name)
        if not app:
            print(self.t("Diese App kenne ich nicht.", "I do not know that app."))
            return
        if action == "install":
            if self._needs_system():
                return
            app.installed = True
            print(self.t("App ist jetzt installiert.", "App is now installed."))
        elif action == "remove":
            if self._needs_system():
                return
            if app.builtin:
                print(self.t("Interne Apps koennen nicht entfernt werden.", "Built-in apps cannot be removed."))
                return
            app.installed = False
            print(self.t("App wurde entfernt.", "App was removed."))
        elif action == "info":
            print(f"Title: {app.name}")
            print(f"State: {'installed' if app.installed else 'optional'}")
            print(f"Description: {app.description_de if self.state.language == 'de' else (app.description_id if self.state.language == 'id' else app.description_en)}")
        elif action in {"run", "open"}:
            if not app.installed:
                print(self.t("Diese App ist nicht installiert.", "That app is not installed."))
                return
            if name == "network":
                self.cmd_network([])
                self.cmd_nic([])
            elif name == "monitor":
                self.cmd_status([])
            elif name == "settings":
                self.cmd_settings([])
            elif name == "files":
                print(self.t("Datei-Bereich geoeffnet (Platzhalter in der Demo).", "File area opened (placeholder in the demo)."))
            elif name == "lumen":
                print(self.t("Lumen wuerde jetzt notes.txt oeffnen.", "Lumen would now open notes.txt."))
            else:
                print(self.t("App-Platzhalter geoeffnet.", "App placeholder opened."))
        else:
            print(self.t("Nutze: app <install|remove|info|run> <name>", "Use: app <install|remove|info|run> <name>"))


    def _ensure_demo_program(self, name: str, path: str) -> None:
        self.state.custom_commands[name] = path
        self.state.programs[name] = {
            "entry": path,
            "trust": "local",
            "caps": "fs-read,fs-write",
            "approved": True,
        }

    def cmd_cmd(self, args: List[str]) -> None:
        if not args:
            print(self.t("Nutze: cmd <list|new|add|show|remove>", "Use: cmd <list|new|add|show|remove>"))
            return
        action = args[0]
        if action == "list":
            if not self.state.custom_commands:
                print(self.t("Noch keine eigenen Befehle.", "No custom commands yet."))
                return
            for name, target in self.state.custom_commands.items():
                print(f"  {name} -> {target}")
            return
        if action == "new" and len(args) >= 2:
            name = args[1]
            path = f"~/ {name}.aos".replace(" ", "")
            self._ensure_demo_program(name, path)
            print(self.t(f"Neuer Befehl erstellt: {name} -> {path}", f"New command created: {name} -> {path}"))
            return
        if action == "add" and len(args) >= 3:
            self._ensure_demo_program(args[1], args[2])
            print(self.t("Eigener Befehl registriert.", "Custom command registered."))
            return
        if action == "show" and len(args) >= 2:
            name = args[1]
            if name not in self.state.custom_commands:
                print(self.t("Diesen Befehl kenne ich nicht.", "I do not know that command."))
                return
            print(f"{name} -> {self.state.custom_commands[name]}")
            self.cmd_prog(["info", name])
            return
        if action == "remove" and len(args) >= 2:
            name = args[1]
            self.state.custom_commands.pop(name, None)
            self.state.programs.pop(name, None)
            print(self.t("Eigener Befehl entfernt.", "Custom command removed."))
            return
        print(self.t("Nutze: cmd <list|new|add|show|remove>", "Use: cmd <list|new|add|show|remove>"))

    def cmd_prog(self, args: List[str]) -> None:
        if not args:
            args = ["list"]
        action = args[0]
        if action == "list":
            if not self.state.programs:
                print(self.t("Noch keine externen Programme.", "No external programs yet."))
                return
            for name in self.state.programs:
                print(f"  - {name}")
            return
        if len(args) < 2:
            print(self.t("Nutze: prog <info|caps|trust|approve|run> <name>", "Use: prog <info|caps|trust|approve|run> <name>"))
            return
        name = args[1]
        if name not in self.state.programs:
            print(self.t("Dieses Programm kenne ich nicht.", "I do not know that program."))
            return
        prog = self.state.programs[name]
        if action == "info":
            print(f"Program: {name}")
            print(f"  Entry: {prog['entry']}")
            print(f"  Trust: {prog['trust']}")
            print(f"  Permissions: {prog['caps']}")
            print(f"  Approval: {'yes' if prog['approved'] else 'no'}")
            return
        if action == "caps" and len(args) >= 3:
            prog['caps'] = args[2]
            print(self.t("Programm-Rechte aktualisiert.", "Program permissions updated."))
            return
        if action == "trust" and len(args) >= 3:
            prog['trust'] = args[2]
            print(self.t("Vertrauensstufe aktualisiert.", "Trust level updated."))
            return
        if action == "approve":
            prog['approved'] = True
            print(self.t("Programm freigegeben.", "Program approved."))
            return
        if action == "run":
            print(self.t("Demo: Externes Programm wuerde jetzt in der Sandbox laufen.", "Demo: External program would now run in the sandbox."))
            self.cmd_prog(["info", name])
            return
        print(self.t("Nutze: prog <info|caps|trust|approve|run> <name>", "Use: prog <info|caps|trust|approve|run> <name>"))

    def cmd_which(self, args: List[str]) -> None:
        if not args:
            print(self.t("Nutze: which <name>", "Use: which <name>"))
            return
        name = args[0]
        if name in self.state.apps:
            print(f"App: {name}")
        elif name in self.state.programs:
            print(f"External program: /apps/programs/{name}.app")
        elif name in self.commands:
            print(f"Shell command: {name}")
        else:
            print(self.t("Nicht gefunden.", "Not found."))

    def cmd_version(self, _: List[str]) -> None:
        print(VERSION)

    def cmd_welcome(self, _: List[str]) -> None:
        self.print_banner()

    def cmd_about(self, _: List[str]) -> None:
        print(self.t("Cyralith soll leicht verstaendlich wie Windows und modular wie Linux sein.", "Cyralith aims to be easy to understand like Windows and modular like Linux."))
        print(self.t("Gerade gibt es Shell, CyralithFS-Grundlage, Rechte, App-Modell, eine menueartige Settings-App, Lumen, Persistenz und einen ersten e1000-Pilottreiber.", "Right now it has a shell, CyralithFS groundwork, rights, an app model, a menu-like settings app, Lumen, persistence and a first e1000 pilot driver."))
        print(CREDIT)

    def cmd_ai(self, args: List[str]) -> None:
        text = " ".join(args).lower()
        if not text:
            print(self.t("AI-Tipp: Frag mich nach help, status, netup, diag oder app list.", "AI tip: ask me about help, status, netup, diag or app list."))
            return
        if any(word in text for word in ["netz", "network", "ping", "adapter", "nic", "treiber", "driver", "e1000", "mac"]):
            print(self.t("AI-Tipp: Nutze settings fuer Werte sowie network, nic, netup, mac oder netprobe.", "AI tip: use network, nic, netup, mac or netprobe."))
            return
        if any(word in text for word in ["app", "programm", "browser", "lumen"]):
            print(self.t("AI-Tipp: Nutze app list, app info <name> oder app run <name>.", "AI tip: use app list, app info <name> or app run <name>."))
            return
        if any(word in text for word in ["user", "benutzer", "rechte", "system"]):
            print(self.t("AI-Tipp: Nutze whoami, users, elevate cyralith oder diag.", "AI tip: use whoami, users, elevate cyralith or diag."))
            return
        if any(word in text for word in ["datei", "file", "ordner", "fs"]):
            print(self.t("AI-Tipp: Nutze fs, pwd oder im Kernel ls/cd/touch/write.", "AI tip: use fs, pwd or in the kernel ls/cd/touch/write."))
            return
        print(self.t("AI-Tipp: Probiere help, status, diag, network, app list oder about.", "AI tip: try help, status, diag, network, app list or about."))

    def cmd_clear(self, _: List[str]) -> None:
        print("\n" * 40)

    def cmd_exit(self, _: List[str]) -> None:
        self.running = False
        print(self.t("Cyralith Demo wird beendet.", "Cyralith demo is closing."))


def main() -> None:
    CyralithShell().run()


if __name__ == "__main__":
    main()
