#!/usr/bin/env python3
"""Build local analysis ELFs for MWo3 overlays, preserving their declared slots.

These files model the 64-byte MWo3 header, text, data and BSS as separate ELF
segments. The original EBOOT section table independently checks each slot base.
Execution-time overlay loading and AOT registration are still separate work.
"""
from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path


def original_slots(elf: bytes) -> dict[str, int]:
    header = struct.unpack_from("<HHIIIIIHHHHHH", elf, 16)
    sections = [struct.unpack_from("<10I", elf, header[5] + i * header[10])
                for i in range(header[11])]
    string_section = sections[header[12]]
    strings = elf[string_section[4]:string_section[4] + string_section[5]]
    result = {}
    for section in sections:
        end = strings.index(b"\0", section[0])
        name = strings[section[0]:end].decode("ascii")
        if name.endswith(".ovl") and not name.startswith(".rel"):
            result[name] = section[3]
    return result


def wrap_overlay(data: bytes, base: int, text_size: int, data_size: int, bss_size: int) -> bytes:
    payload_size = 64 + text_size + data_size
    if payload_size > len(data) or any(data[payload_size:]):
        raise ValueError("Invalid MWo3 payload sizes or nonzero padding")
    text = data[64:64 + text_size]
    first = next((i for i in range(0, len(text), 4) if any(text[i:i + 4])), 0)
    entry = base + 64 + first
    strings = b"\0.ovl_header\0.text\0.data\0.bss\0.shstrtab\0"
    names = {name: strings.index(name.encode() + b"\0") for name in
             (".ovl_header", ".text", ".data", ".bss", ".shstrtab")}
    file_payload = 0x100
    string_offset = file_payload + payload_size
    section_offset = (string_offset + len(strings) + 3) & ~3
    output = bytearray(section_offset + 6 * 40)
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    struct.pack_into("<16sHHIIIIIHHHHHH", output, 0,
                     ident, 2, 8, 1, entry, 52, section_offset, 0x10A23001,
                     52, 32, 3, 40, 6, 5)
    segments = (
        (file_payload, base, 64, 64, 4),
        (file_payload + 64, base + 64, text_size, text_size, 5),
        (file_payload + 64 + text_size, base + 64 + text_size,
         data_size, data_size + bss_size, 6),
    )
    for i, (offset, address, size, memory_size, flags) in enumerate(segments):
        struct.pack_into("<8I", output, 52 + i * 32,
                         1, offset, address, address, size, memory_size, flags, 4)
    output[file_payload:file_payload + payload_size] = data[:payload_size]
    output[string_offset:string_offset + len(strings)] = strings
    sections = (
        (".ovl_header", 1, 2, base, file_payload, 64),
        (".text", 1, 6, base + 64, file_payload + 64, text_size),
        (".data", 1, 3, base + 64 + text_size, file_payload + 64 + text_size, data_size),
        (".bss", 8, 3, base + payload_size, file_payload + payload_size, bss_size),
        (".shstrtab", 3, 0, 0, string_offset, len(strings)),
    )
    for i, (name, kind, flags, address, offset, size) in enumerate(sections, 1):
        struct.pack_into("<10I", output, section_offset + i * 40,
                         names[name], kind, flags, address, offset, size, 0, 0, 4, 0)
    return bytes(output)


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    identity = json.loads((profile / "config/ulus10597.json").read_text())
    original = (profile / "game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF").read_bytes()
    if hashlib.sha256(original).hexdigest() != identity["elf_sha256"]:
        raise ValueError("EBOOT does not match the supported PES profile")
    slots = original_slots(original)
    manifest = json.loads((profile / "analysis/overlay_manifest.json").read_text())
    destination = profile / "analysis/overlay_elf"
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    for source in manifest["files"]:
        overlay = (profile / "game/overlays" / source["name"]).read_bytes()
        if hashlib.sha256(overlay).hexdigest() != source["sha256"] or overlay[:4] != b"MWo3":
            raise ValueError(f"Overlay identity mismatch: {source['name']}")
        index, base, text_size, data_size, bss_size, init_begin, init_end = struct.unpack_from("<7I", overlay, 4)
        if slots.get(source["name"]) != base:
            raise ValueError(f"Overlay base disagrees with EBOOT section: {source['name']}")
        if base < 0x08800000 or base + 64 + text_size + data_size + bss_size > 0x0A000000:
            raise ValueError("Overlay layout exceeds PSP user memory")
        output = destination / (source["name"] + ".elf")
        output.write_bytes(wrap_overlay(overlay, base, text_size, data_size, bss_size))
        records.append({"name": source["name"], "module_index": index,
                        "base": f"0x{base:08X}", "text_address": f"0x{base + 64:08X}",
                        "text_size": text_size, "data_size": data_size, "bss_size": bss_size,
                        "has_nonzero_text": any(overlay[64:64 + text_size]),
                        "header_init_range": [f"0x{init_begin:08X}", f"0x{init_end:08X}"]})
    result = {"address_model": "experimental MWo3 header + text + data + BSS",
              "runtime_loader_verified": False, "modules": records}
    (profile / "analysis/overlay_layout.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Validated {len(records)} slot bases against the EBOOT section table")
    print(f"Analysis ELF wrappers: {len(records)}; nonzero text: {sum(r['has_nonzero_text'] for r in records)}")


if __name__ == "__main__":
    main()
