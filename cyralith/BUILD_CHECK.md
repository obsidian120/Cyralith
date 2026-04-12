# Cyralith build check

- Kernel build: successful via `make` in `kernel/`
- Output verified: `kernel/build/cyralith.kernel`
- Output format: ELF 32-bit i386

## Added in this package
- Snake game (`snake`, `games`, `app run snake`)
- Expanded network driver choices (`intel`, `e1000`, `pcnet`, `rtl8139`, `virtio`)
- Updated help topics for games and network drivers

## Limits of verification here
- No GRUB ISO boot test in this environment
- No QEMU runtime gameplay test in this environment
- No VirtualBox hardware test in this environment


## Arcade / Snake score update
- kernel `make` successful in container
- output: `kernel/build/cyralith.kernel`
- file type: ELF 32-bit LSB executable, Intel i386
- `kernel/iso/boot/grub/grub.cfg` now included in repo tree
- `make run` switched to `-display curses` for text-first setups


## System Reliability Update
- Added persistent recovery state with boot counters, startup failure tracking, unexpected shutdown tracking, and safe mode
- Added persistent system log with `log`, `log boot`, `log warn`, `log errors`, and `log clear`
- Added `health`, `bootinfo`, `safemode`, and clean `shutdown` support
- `savefs` / `loadfs` now include reliability + log state along with arcade scores
- Kernel build rechecked successfully after the update
