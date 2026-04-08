# AuroraOS Stable Build 2.7.0

Programmiert von Obsidian.

AuroraOS ist ein fruehes, aber bereits benutzbares Betriebssystem-Projekt mit Fokus auf:
- einfache Bedienung
- modulare Architektur
- deutsch/englische Bedienung
- AuroraFS
- Lumen als eingebautem Editor
- Settings-Zentrale statt verstreuter Einstellungsbefehle
- sichere externe Programme mit Manifesten

## Stand dieses Builds
Dieses Build ist als **GitHub-ready Source Package** aufgeraeumt:
- nachvollziehbare Ordnerstruktur
- keine zufaelligen Versionsordner im Paketnamen
- `.gitignore` fuer Build-Artefakte und Python-Cache
- aktualisierte Dokumentation
- Build-Ausgabe getrennt vom Quellpaket

## Ordnerstruktur
- `kernel/` - Kernel, Shell, AuroraFS, Netzwerk, Settings, Lumen
- `hosted_ai_shell/` - Python-Demo fuer das Shell-/AI-Verhalten
- `docs/` - Vision, Architektur und Roadmap

## Schnellstart
### Hosted Shell
```bash
cd hosted_ai_shell
python3 main.py
```

### Kernel bauen
```bash
cd kernel
make clean
make
```

### ISO bauen
Dafuer brauchst du lokal unter Linux oder WSL zusaetzlich GRUB-/ISO-Tools.
```bash
cd kernel
make iso
```

## Gute erste Tests in der VM
- `help`
- `quickstart`
- `settings`
- `status`
- `whoami`
- `fs`
- `network`
- `app list`
- `edit note.txt`

## Hinweise
- Dieses Paket ist ein **Source Package** fuer GitHub und Weiterentwicklung.
- Persistenz braucht eine **virtuelle Festplatte**.
- Externe Programme sind noch kein voller Userspace, sondern ein fruehes, kontrolliertes Programm-Modell.
