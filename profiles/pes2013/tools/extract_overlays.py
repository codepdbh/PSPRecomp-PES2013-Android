#!/usr/bin/env python3
"""Extract this copy's OVER.cpk using its UTF/ITOC sizes and CRILAYLA streams.

Outputs stay in the ignored game directory. Format cross-check reference:
https://github.com/esperknight/CriPakTools/blob/master/CriPakTools/CPK.cs
No source from that implementation is included here.
"""
from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path


def utf_table(data: bytes) -> list[dict]:
    if data[:4] != b"@UTF" or len(data) < 32:
        raise ValueError("Invalid UTF table")
    size, rows_offset, strings_offset, blobs_offset, _name, columns, row_size, rows = (
        struct.unpack_from(">5I2HI", data, 4)
    )
    limit = size + 8
    rows_offset += 8
    strings_offset += 8
    blobs_offset += 8
    if limit > len(data) or rows_offset + rows * row_size > limit:
        raise ValueError("UTF table extends beyond packet")

    def string(offset: int) -> str:
        start = strings_offset + offset
        if not strings_offset <= start < blobs_offset:
            raise ValueError("UTF string offset out of bounds")
        end = data.index(b"\0", start, blobs_offset)
        return data[start:end].decode("utf-8")

    def value(kind: int, pos: int):
        formats = {0: ">B", 1: ">b", 2: ">H", 3: ">h", 4: ">I", 5: ">i",
                   6: ">Q", 7: ">q", 8: ">f", 10: ">I", 11: ">II"}
        if kind not in formats:
            raise ValueError(f"Unsupported UTF type {kind}")
        fmt = formats[kind]
        length = struct.calcsize(fmt)
        if pos + length > limit:
            raise ValueError("Truncated UTF value")
        result = struct.unpack_from(fmt, data, pos)
        if kind == 10:
            result = string(result[0])
        elif kind == 11:
            start, length_blob = blobs_offset + result[0], result[1]
            if start < blobs_offset or start + length_blob > limit:
                raise ValueError("UTF blob out of bounds")
            result = data[start:start + length_blob]
        else:
            result = result[0]
        return result, pos + length

    schema = []
    pos = 32
    for _ in range(columns):
        flags, name = struct.unpack_from(">BI", data, pos)
        pos += 5
        storage, kind = flags & 0xF0, flags & 0x0F
        constant = None
        if storage == 0x30:
            constant, pos = value(kind, pos)
        elif storage not in (0, 0x10, 0x50):
            raise ValueError(f"Unsupported UTF storage {storage:#x}")
        schema.append((string(name), storage, kind, constant))
    if pos > rows_offset:
        raise ValueError("UTF schema overlaps rows")
    result = []
    for row_index in range(rows):
        pos = rows_offset + row_index * row_size
        row = {}
        for name, storage, kind, constant in schema:
            if storage == 0x50:
                row[name], pos = value(kind, pos)
            else:
                row[name] = constant if storage == 0x30 else 0
        if pos > rows_offset + (row_index + 1) * row_size:
            raise ValueError("UTF row exceeds declared width")
        result.append(row)
    return result


def packet(data: bytes, offset: int, signature: bytes) -> list[dict]:
    if data[offset:offset + 4] != signature or offset + 16 > len(data):
        raise ValueError(f"Missing {signature!r} packet")
    size = struct.unpack_from("<Q", data, offset + 8)[0]
    if size > 16 * 1024 * 1024 or offset + 16 + size > len(data):
        raise ValueError("Invalid CPK metadata packet size")
    body = data[offset + 16:offset + 16 + size]
    if body[:4] != b"@UTF":
        key = 0x5F
        decoded = bytearray()
        for byte in body:
            decoded.append(byte ^ key)
            key = (key * 0x15) & 0xFF
        body = bytes(decoded)
    return utf_table(body)


def decompress(stream: bytes, expected: int) -> bytes:
    if stream[:8] != b"CRILAYLA":
        if len(stream) != expected:
            raise ValueError("Raw overlay size mismatch")
        return stream
    size, packed_size = struct.unpack_from("<II", stream, 8)
    if size + 256 != expected or expected > 64 * 1024 * 1024:
        raise ValueError("CRILAYLA output size disagrees with ITOC")
    prefix = 16 + packed_size
    if prefix + 256 != len(stream):
        raise ValueError("CRILAYLA stream length disagrees with ITOC")
    output = bytearray(expected)
    output[:256] = stream[prefix:prefix + 256]
    cursor, pool, remaining = prefix - 1, 0, 0

    def bits(count: int) -> int:
        nonlocal cursor, pool, remaining
        result = 0
        while count:
            if remaining == 0:
                if cursor < 16:
                    raise ValueError("Truncated CRILAYLA bitstream")
                pool, remaining = stream[cursor], 8
                cursor -= 1
            take = min(count, remaining)
            result = (result << take) | ((pool >> (remaining - take)) & ((1 << take) - 1))
            remaining -= take
            count -= take
        return result

    pos = expected - 1
    while pos >= 256:
        if bits(1) == 0:
            output[pos] = bits(8)
            pos -= 1
            continue
        distance = bits(13) + 3
        length = 3
        for width in (2, 3, 5, 8):
            extra = bits(width)
            length += extra
            if extra != (1 << width) - 1:
                break
        else:
            while True:
                extra = bits(8)
                length += extra
                if extra != 255:
                    break
        if pos + distance >= expected or length > pos - 255:
            raise ValueError("CRILAYLA backreference outside output")
        for _ in range(length):
            output[pos] = output[pos + distance]
            pos -= 1
    return bytes(output)


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    archive = profile / "game/PSP_GAME/USRDIR/OVER.cpk"
    data = archive.read_bytes()
    header = packet(data, 0, b"CPK ")[0]
    index = packet(data, header["ItocOffset"], b"ITOC")[0]
    entries = []
    for key in ("DataL", "DataH"):
        if isinstance(index.get(key), bytes) and index[key]:
            entries.extend(utf_table(index[key]))
    entries.sort(key=lambda entry: entry["ID"])
    if len(entries) != header["Files"] or len({entry["ID"] for entry in entries}) != len(entries):
        raise ValueError("CPK ITOC file count or IDs are inconsistent")
    align = header["Align"]
    if align <= 0:
        raise ValueError("Invalid CPK content alignment")
    offset = header["ContentOffset"]
    extracted = []
    destination = profile / "game/overlays"
    destination.mkdir(parents=True, exist_ok=True)
    seen = set()
    for entry in entries:
        size = entry["FileSize"]
        expected = entry.get("ExtractSize", size)
        if offset + size > len(data):
            raise ValueError("CPK file exceeds archive")
        output = decompress(data[offset:offset + size], expected)
        if output[:4] != b"MWo3":
            raise ValueError(f"Unexpected overlay header at ID {entry['ID']}")
        name = output[32:64].split(b"\0", 1)[0].decode("ascii")
        if not name.endswith(".ovl") or Path(name).name != name or "/" in name or "\\" in name or name in seen:
            raise ValueError("Unsafe or duplicate overlay filename")
        seen.add(name)
        words = struct.unpack_from("<7I", output, 4)
        payload_end = 64 + words[2] + words[3]
        if payload_end > len(output) or any(output[payload_end:]):
            raise ValueError(f"Overlay payload sizes or padding are inconsistent: {name}")
        (destination / name).write_bytes(output)
        extracted.append({"id": entry["ID"], "name": name, "archive_offset": offset,
                          "packed_size": size, "size": len(output),
                          "sha256": hashlib.sha256(output).hexdigest(),
                          "header_words": [f"0x{word:08X}" for word in words]})
        offset += ((size + align - 1) // align) * align
    manifest = {"archive_sha256": hashlib.sha256(data).hexdigest(), "files": extracted}
    (profile / "analysis/overlay_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Extracted {len(extracted)} overlays ({sum(item['size'] for item in extracted)} decoded bytes) to {destination}")


if __name__ == "__main__":
    main()
