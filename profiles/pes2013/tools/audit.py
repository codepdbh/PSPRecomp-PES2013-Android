#!/usr/bin/env python3
"""Inventory local PES inputs and compare imports with literal host registrations.

This is static triage, not proof that a host implementation behaves correctly.
The overlay inventory reads the preserved MWo3 headers in the CRILAYLA streams;
it does not decompress or recompile overlay code.
"""
from __future__ import annotations

import csv
import hashlib
import json
import re
import struct
from collections import Counter
from pathlib import Path


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    repo = profile.parents[1]
    analysis = profile / "analysis"
    identity = json.loads((profile / "config/ulus10597.json").read_text())
    elf = profile / "game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF"
    if hashlib.sha256(elf.read_bytes()).hexdigest() != identity["elf_sha256"]:
        raise ValueError("The executable does not match this PES profile")
    with (analysis / "pes2013_report_imports.csv").open(newline="") as source:
        imports = list(csv.DictReader(source))
    registrations: dict[tuple[str, int], str] = {}
    sources = sorted((repo / "profiles/vcs/host").glob("*.cpp"))
    literal = re.compile(r'register_hle\(\s*"([^"\n]+)"\s*,\s*(0x[0-9a-fA-F]+)u?')
    all_source = ""
    for source in sources:
        text = source.read_text(encoding="utf-8")
        all_source += text.lower()
        for match in literal.finditer(text):
            registrations[(match[1], int(match[2], 16))] = str(source.relative_to(repo))
    for item in imports:
        key = (item["library"], int(item["nid"], 16))
        item["registration_source"] = registrations.get(key, "")
        item["static_status"] = (
            "literal_registration_found" if key in registrations else
            "nid_mentioned_manual_review" if item["nid"].lower() in all_source else
            "no_registration_found"
        )
    with (analysis / "hle_audit.csv").open("w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=list(imports[0]))
        writer.writeheader()
        writer.writerows(imports)

    archive = profile / "game/PSP_GAME/USRDIR/OVER.cpk"
    data = archive.read_bytes()
    overlays = []
    for match in re.finditer(rb"[A-Za-z0-9_]+\.ovl\x00", data):
        start = match.start() - 32
        if start < 0 or data[start:start + 4] != b"MWo3":
            continue
        words = struct.unpack_from("<7I", data, start + 4)
        overlays.append({
            "name": match[0][:-1].decode("ascii"),
            "header_offset_in_archive": f"0x{start:08X}",
            "header_words": [f"0x{value:08X}" for value in words],
            "candidate_load_address": f"0x{words[1]:08X}",
        })
    load_counts = Counter(item["candidate_load_address"] for item in overlays)
    summary = {
        "disc_id": identity["disc_id"],
        "elf_sha256": identity["elf_sha256"],
        "import_count": len(imports),
        "import_static_status_counts": dict(Counter(item["static_status"] for item in imports)),
        "unmatched_by_library": dict(Counter(item["library"] for item in imports
            if item["static_status"] != "literal_registration_found")),
        "overlay_count": len(overlays),
        "overlay_extraction_manifest_present": (analysis / "overlay_manifest.json").is_file(),
        "shared_candidate_load_addresses": {key: count for key, count in load_counts.items() if count > 1},
        "limitations": [
            "Literal registration matches do not verify service semantics or aliases.",
            "MWo3 field names other than the signature are inferred, not validated by execution.",
            "Overlay extraction is reported separately; overlay code has not been translated.",
            "The EBOOT AOT corpus alone does not include the dynamically loaded overlay code.",
        ],
        "overlays": overlays,
    }
    (analysis / "portability_audit.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in summary.items() if key != "overlays"}, indent=2))


if __name__ == "__main__":
    main()
