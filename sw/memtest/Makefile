# Bare-metal M2 memtest for classic MicroBlaze (no BSP).
# Toolchain from the Vivado install; override VIVADO_GNU as needed.

VIVADO_GNU ?= $(HOME)/Vivado/2025.2/gnu/microblaze/lin/bin
CC = $(VIVADO_GNU)/mb-gcc

CFLAGS = -O2 -Wall -mlittle-endian -nostdlib -nostartfiles -ffreestanding

memtest.elf: memtest.c start.S link.ld
	$(CC) $(CFLAGS) -Wl,-T,link.ld -o $@ start.S memtest.c

clean:
	rm -f memtest.elf
