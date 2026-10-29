#!/usr/bin/env python3
"""Collect EBOOT function entries referenced by the game's overlay modules."""
import json
import struct
from pathlib import Path
from translate_overlays import eboot_entries


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    original = (profile / "game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF").read_bytes()
    offset = struct.unpack_from("<I", original, 32)[0]
    size, count = struct.unpack_from("<HH", original, 46)
    program_offset = struct.unpack_from("<I", original, 28)[0]
    program_size, program_count = struct.unpack_from("<HH", original, 42)
    executable_loads = []
    for index in range(program_count):
        kind, _, base, _, file_size, _, flags, _ = struct.unpack_from("<8I", original, program_offset + index * program_size)
        if kind == 1 and flags & 1:
            executable_loads.append((base, base + file_size))
    ranges = []
    for index in range(count):
        _, kind, flags, base, _, length, *_ = struct.unpack_from("<10I", original, offset + index * size)
        if kind == 1 and flags & 6 == 6 and any(start <= base and base + length <= end for start, end in executable_loads):
            ranges.append((base, base + length))
    entries = set()
    sources = {}
    for path in sorted((profile / "analysis/overlay_elf").glob("*.elf")):
        data = path.read_bytes()
        found = set()
        for begin, end in ranges:
            found.update(eboot_entries(data, begin, end))
        entries.update(found)
        sources[path.name] = len(found)
    output = profile / "analysis/overlay_eboot_entries.txt"
    output.write_text("".join(f"0x{entry:08X}\n" for entry in sorted(entries)), encoding="utf-8")
    (profile / "analysis/overlay_entry_sources.json").write_text(json.dumps(sources, indent=2) + "\n")
    print(f"Collected {len(entries)} cross-module EBOOT entries from {len(sources)} overlays")


if __name__ == "__main__":
    main()
