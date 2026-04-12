# Roadmap

## Phase 1 – Fundament
- [x] VGA-Konsole
- [x] interne Shell
- [x] erste Intent-Routing-Schicht
- [x] klare Architektur fuer AI-first Design

## Phase 2 – Technische OS-Basis
- [x] IDT-Grundlage
- [x] PIC-Remap
- [x] Interrupt-basierte Tastatur
- [x] einfacher Heap / Speicherallokation
- [x] PIT-Timer
- [x] kooperativer Service-Scheduler mit Laufzeitmetriken
- [x] Paging-Grundlage mit aktivem x86-Page-Directory, Frame-Allocator und Page-Fault-Tracking

## Phase 3 – Benutzbares System
- [x] RAM-Dateisystem (CyralithFS)
- [x] kooperatives Prozessmodell mit PID, Status, eigener Speicherregion und Shell-Steuerung
- [x] User-Space-nahe Skript-/Manifest-Programme
- [x] Systemdienste
- [x] Paket-/Modulverwaltung in einfacher Form (`app` / `prog` / `pkg`)
- [x] Deutsch / Englisch in der Shell-Basis

## Phase 4 – Desktop und AI-Service
- [ ] GUI-Kompositor
- [ ] Fensterverwaltung
- [x] Settings-App (Shell-basiert)
- [x] Suche / Launcher (`launch`)
- [x] lokaler AI-Orchestrator / Intent-Router
- [x] Policy Engine fuer sichere Aktionen und Programmfreigaben

## Phase 5 – Alltagstauglichkeit
- [x] Netzwerk-Basis mit NIC-Erkennung, e1000-Pilot und Shell-Werkzeugen
- [x] Dateimanager-Basis (`files`, `ls`, `cp`, `mv`, `find`)
- [ ] Browser-Anbindung
- [ ] Update-Mechanismus
- [x] Crash-Report / Diagnostik / Action-Log / Doctor
- [x] Nutzerkonten und Rechte

## Phase 6 – Differenzierung
- [x] natuerliche Sprache fuer Systemsteuerung (regelbasiert, lokal)
- [x] Task-Automatisierung (`job add`, `jobs`, `job cancel`)
- [x] erklaerbare Aktionen (`actionlog`)
- [x] Recovery-Modus mit Diagnose-Assistent (`doctor`, `recover`)
- [ ] modulare Desktops und Workflows
