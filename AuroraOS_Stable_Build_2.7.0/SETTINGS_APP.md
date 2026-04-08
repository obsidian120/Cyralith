# AuroraOS Settings App

AuroraOS 2.7.0 keeps the clearer Settings center and prepares the source package for GitHub.

## Start
- `settings`
- `open settings`
- `app run settings`
- `settings general`
- `settings network`
- `settings security`
- `settings expert`

## Controls in the kernel
- Arrow Up / Down: select an entry
- Enter: open or change it
- `q` or `Ctrl+C`: go back
- from the overview page: `q` or `Ctrl+C` closes Settings

## Categories
### General
- language
- keyboard layout
- AI helper mode
- startup hints

### Network
- computer name
- DHCP
- IP address
- gateway

### Security
- program approval policy
- legacy wrappers without manifests
- auto-approve new local commands

### Expert mode
- network driver auto-start
- driver diagnostics
- auto-save toggle
- verbose diagnostics

## System rights
Network, Security, Expert, and Save actions require the root-like system mode:
- `elevate aurora`
