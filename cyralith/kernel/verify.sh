#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
make clean >/dev/null
make >/dev/null
grub-file --is-x86-multiboot build/cyralith.kernel
find iso -maxdepth 3 -type f | sort
