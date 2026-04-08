# AuroraOS persistence setup

AuroraOS v15 can persist these things to a **virtual ATA hard disk**:
- AuroraFS files and folders
- AuroraFS rights and owners
- user passwords
- network config (hostname, IP, gateway, DHCP flag)
- app install state

## Important
The ISO itself is read-only.
Persistence only works when you attach a **virtual hard disk** in your VM.

## Quick test
1. boot AuroraOS from the ISO
2. attach a virtual hard disk
3. run:
   - `elevate aurora`
   - `app install browser`
   - `hostname aurora-box`
   - `savefs`
4. reboot
5. run:
   - `loadfs`
   - `app list`
   - `network`
