#!/usr/bin/env python3
"""Build the QSPI boot image from the FreeRTOS app ELF (stdlib only).

Image layout (consumed by sw/boot/boot.c):
  u32 magic 'TRIB' (0x54524942), u32 nseg, u32 entry
  per segment: u32 addr, u32 len, then len bytes padded to 4

Usage: mkbootimg.py <app.elf> <out.bin>
"""

import struct
import sys


def main():
    elf_path, out_path = sys.argv[1], sys.argv[2]
    data = open(elf_path, "rb").read()

    assert data[:4] == b"\x7fELF" and data[4] == 1 and data[5] == 1, "need ELF32 LE"
    e_entry, e_phoff = struct.unpack_from("<II", data, 0x18)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x2A)

    segs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = \
            struct.unpack_from("<IIIIII", data, off)
        if p_type != 1 or p_filesz == 0:  # PT_LOAD with content only
            continue
        segs.append((p_paddr, data[p_offset:p_offset + p_filesz]))

    out = struct.pack("<III", 0x54524942, len(segs), e_entry)
    for addr, payload in segs:
        out += struct.pack("<II", addr, len(payload))
        out += payload + b"\x00" * (-len(payload) % 4)

    open(out_path, "wb").write(out)
    total = sum(len(p) for _, p in segs)
    print(f"bootimg: {len(segs)} segments, {total} payload bytes, "
          f"entry 0x{e_entry:08x} -> {out_path}")


if __name__ == "__main__":
    main()
