ARCH=i386
CC=gcc
LD=ld
CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -Werror -Ikernel/src -Isrc
LDFLAGS=-m elf_i386 -T linker.ld
SRC=src/boot.s src/interrupt_stubs.s src/kernel.c src/console.c src/string.c src/keyboard.c src/shell.c src/ai_core.c src/memory.c src/task.c src/user.c src/editor.c src/aurorafs.c src/storage.c src/network.c src/app.c src/extprog.c src/idt.c src/interrupts.c src/timer.c
OBJ=$(patsubst src/%.c,build/%.o,$(filter %.c,$(SRC))) $(patsubst src/%.s,build/%.o,$(filter %.s,$(SRC)))
KERNEL=build/auroraos.kernel
ISO=build/auroraos.iso

all: $(KERNEL)

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.s | build
	$(CC) -m32 -c $< -o $@

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

iso: $(KERNEL)
	mkdir -p iso/boot
	cp $(KERNEL) iso/boot/auroraos.kernel
	grub-mkrescue -o $(ISO) iso

run: iso
	qemu-system-x86_64 -cdrom $(ISO)

clean:
	rm -rf build
	rm -f iso/boot/auroraos.kernel

.PHONY: all iso run clean
