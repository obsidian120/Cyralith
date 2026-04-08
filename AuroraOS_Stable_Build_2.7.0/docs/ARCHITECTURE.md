# Architektur

## Überblick

AuroraOS ist in vier große Ebenen geteilt:

```text
+-----------------------------------------------------------+
| UX Layer                                                  |
| Desktop / Launcher / Settings / Search / Voice / Widgets |
+-----------------------------------------------------------+
| AI Orchestration Layer                                    |
| Intent Engine / Policy Engine / Context / Automation     |
+-----------------------------------------------------------+
| System Services                                            |
| FS / Package / Network / Window Server / Audio / IPC Bus |
+-----------------------------------------------------------+
| Kernel                                                     |
| Scheduler / Memory / Drivers / Security / Syscalls       |
+-----------------------------------------------------------+
```

## Warum diese Trennung wichtig ist

Ein echtes AI-Betriebssystem darf die AI **nicht in den Kernel ziehen**.
Der Kernel muss klein, deterministisch und überprüfbar bleiben.
Die AI läuft als privilegiert begrenzter Systemdienst.

## Kernel-Aufgaben

- Hardware initialisieren
- Speicher verwalten
- Prozesse/Threads planen
- Syscalls bereitstellen
- Treiber koordinieren
- IPC-Grundlagen bereitstellen
- Rechte und Isolation durchsetzen

## Systemdienste

Diese Ebene enthält klassische OS-Bausteine:

- Dateisystemdienst
- Netzwerkdienst
- Audio
- Window Server / Compositor
- Paket- und Modulverwaltung
- Settings-Service
- Logging / Diagnostics

## AI-Orchestrator

Die zentrale Neuerung:

### Komponenten
- **Intent Parser**: versteht Nutzereingaben
- **System Mapper**: übersetzt Intents in konkrete Systemaufrufe
- **Context Store**: merkt offenen Arbeitskontext
- **Policy Guard**: prüft Berechtigungen und Risiken
- **Explainer**: erklärt Folgen einer Aktion in einfacher Sprache

### Beispielablauf

Benutzer: `Mach den Bildschirm dunkler und öffne WLAN`.

1. Intent Parser erkennt zwei Teilaufgaben.
2. Context Store erkennt, dass es um Systemeinstellungen geht.
3. System Mapper übersetzt in:
   - Theme-Service: `set dark mode`
   - Settings-App: `open network panel`
4. Policy Guard bestätigt, dass keine Administratorfreigabe nötig ist.
5. UX zeigt optional eine Vorschau oder führt direkt aus.

## Modulsystem

AuroraOS soll ein deklaratives Modulsystem bekommen.
Jedes Modul beschreibt:

- Name
- Version
- Capabilities
- benötigte Dienste
- UI-Einstiegspunkte
- AI-exponierte Aktionen

Beispiel:

```json
{
  "name": "network-settings",
  "version": "0.1.0",
  "capabilities": ["net.read", "net.configure"],
  "actions": [
    "open_network_panel",
    "toggle_wifi"
  ]
}
```

## Sicherheitsmodell

Jede AI-Aktion wird in drei Klassen eingeteilt:

1. **Informativ** – sofort erlaubt
2. **Konfigurierend** – erlaubt, aber protokolliert
3. **Privilegiert / kritisch** – nur mit expliziter Bestätigung

## UX-Prinzip

AuroraOS soll parallel zwei Bedienpfade anbieten:

- klassische Maus/Tastatur-Navigation
- natürliche Sprache / Kommandoleiste / Spracheingabe

Beide Pfade müssen denselben Systemkern benutzen. Dadurch bleibt das System verständlich und konsistent.
