#!/usr/bin/env python3
"""Analyze and translate overlay ELFs into isolated, experimental C++ namespaces.

Outputs remain in analysis/. The selected modules can be linked into the
diagnostic host; header and code identity are checked before AOT activation.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import struct
from pathlib import Path


def eboot_entries(data: bytes, begin: int, end: int) -> list[int]:
    """Cross-module direct calls and allocated data pointers in the EBOOT."""
    offset = struct.unpack_from("<I", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 46)
    entries = set()
    for index in range(count):
        _, kind, flags, base, at, length, *_ = struct.unpack_from("<10I", data, offset + index * size)
        if kind != 1 or not flags & 2:
            continue
        for delta in range(0, length - 3, 4):
            word = struct.unpack_from("<I", data, at + delta)[0]
            if flags & 4:
                if word >> 26 not in (2, 3):
                    continue
                target = ((base + delta + 4) & 0xF0000000) | ((word & 0x3FFFFFF) << 2)
            else:
                target = word
            if begin <= target < end and target % 4 == 0:
                entries.add(target)
    return sorted(entries)


def write_bank(profile: Path, modules: list[dict], selected: set[str]) -> None:
    output = profile / "analysis/overlay_codegen"
    declarations = []
    rows = []
    sources = []
    for module in modules:
        name = Path(module["name"]).stem
        payload = (profile / "game/overlays" / module["name"]).read_bytes()
        base = int(module["base"], 16)
        text_size = module["text_size"]
        checksum = 14695981039346656037
        for byte in payload[64:64 + text_size]:
            checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        callback = "nullptr"
        if name in selected:
            declarations.append(f"namespace psprecomp::pes_overlay_{name} {{ void register_generated_functions(Runtime &); }}")
            callback = f"&psprecomp::pes_overlay_{name}::register_generated_functions"
            sources.extend(sorted((output / name).glob("*.cpp")))
        header = ",".join(str(b) for b in payload[:64])
        memory_size = 64 + text_size + module["data_size"] + module["bss_size"]
        rows.append(f'    {{"{module["name"]}", {base}u, {text_size}u, {memory_size}u, {{{header}}}, {checksum}ull, {callback}}}')
    bank = '#include "overlay_bank.hpp"\n' + "\n".join(declarations)
    bank += '\nnamespace pes2013 {\nstd::span<const OverlayModule> generated_overlay_modules() {\n'
    bank += 'static const OverlayModule modules[] = {\n' + ",\n".join(rows) + '\n};\nreturn modules;\n}\n}\n'
    (output / "linked_overlay_bank.cpp").write_text(bank, encoding="utf-8")
    sources.append(output / "linked_overlay_bank.cpp")
    paths = ['    "${CMAKE_CURRENT_LIST_DIR}/' + source.relative_to(output).as_posix() + '"' for source in sources]
    (output / "linked_overlays.cmake").write_text("set(PES2013_OVERLAY_SOURCES\n" + "\n".join(paths) + "\n)\n", encoding="utf-8")


def tool(build: Path, name: str, relative: str = "") -> Path:
    for path in (build / relative / "Release" / (name + ".exe"),
                 build / relative / name, build / relative / (name + ".exe")):
        if path.is_file():
            return path
    raise FileNotFoundError(f"Build {name} first under {build}")


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    repo = profile.parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--all", action="store_true")
    selection.add_argument("--names", nargs="+")
    parser.add_argument("--tool-directory", type=Path, default=repo / "out/pes2013-probe")
    args = parser.parse_args()
    analyzer = tool(args.tool_directory, "psp_analyze")
    generator = tool(args.tool_directory, "pes2013_recomp", "profiles/pes2013")
    layout = json.loads((profile / "analysis/overlay_layout.json").read_text())
    original = (profile / "game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF").read_bytes()
    references = [original] + [path.read_bytes() for path in sorted((profile / "analysis/overlay_elf").glob("*.elf"))]
    modules = [module for module in layout["modules"] if module["has_nonzero_text"]]
    names = args.names or ["bootset", "title"]
    if not args.all:
        wanted = {name.removesuffix(".ovl") for name in names}
        modules = [module for module in modules if Path(module["name"]).stem in wanted]
        if {Path(module["name"]).stem for module in modules} != wanted:
            raise ValueError("Unknown overlay or overlay has no executable text")
    results = []
    for module in modules:
        name = Path(module["name"]).stem
        if not re.fullmatch(r"[A-Za-z0-9_]+", name):
            raise ValueError("Invalid module name")
        elf = profile / "analysis/overlay_elf" / (module["name"] + ".elf")
        report = profile / "analysis" / (name + "_report.json")
        output = profile / "analysis/overlay_codegen" / name
        log = profile / "analysis" / (name + "_codegen.log")
        cross_entries = set()
        for reference in references:
            cross_entries.update(eboot_entries(reference, int(module["text_address"], 16),
                                              int(module["text_address"], 16) + module["text_size"]))
        entry_file = profile / "analysis" / (name + "_cross_entries.txt")
        entry_file.write_text("".join(f"0x{entry:08X}\n" for entry in sorted(cross_entries)), encoding="utf-8")
        with log.open("w", encoding="utf-8") as stream:
            subprocess.run([str(analyzer), str(elf), str(report)],
                           stdout=stream, stderr=subprocess.STDOUT, check=True)
            subprocess.run([str(generator), str(elf), "--auto", str(output),
                            "0x08804000", "0x4000", "--generic", f"--entries-file={entry_file}"],
                           stdout=stream, stderr=subprocess.STDOUT, check=True)
        namespace = "pes_overlay_" + name
        for source in list(output.glob("*.cpp")) + list(output.glob("*.hpp")):
            content = source.read_text(encoding="utf-8")
            if "namespace psprecomp {" not in content:
                raise ValueError(f"Unexpected codegen namespace in {source}")
            content = content.replace("namespace psprecomp {", "namespace psprecomp::" + namespace + " {")
            # Overlay code must return to the resolver at cross-module calls.
            # Distinct names keep it out of Runtime's ordinary AOT chaining table.
            content = content.replace('"recomp_unit_', '"overlay_' + name + '_unit_')
            if source.name == "generated_units.hpp":
                # Moving the generator's forward declarations into a nested
                # namespace would declare different, incomplete CPU/runtime types.
                content = content.replace('#include "psprecomp/guest_memory.hpp"',
                                          '#include "psprecomp/runtime.hpp"')
                content = content.replace("class Runtime;", "using ::psprecomp::Runtime;")
                content = content.replace("struct AllegrexContext;", "using ::psprecomp::AllegrexContext;")
            source.write_text(content, encoding="utf-8")
        analysis = json.loads(report.read_text())
        generation = json.loads((output / "auto_codegen_report.json").read_text())
        result = {"name": module["name"], "namespace": "psprecomp::" + namespace,
                  "candidate_entry": analysis["entry_runtime"],
                  "function_seeds": generation["discovered_function_seeds"],
                  "unsupported_occurrences": analysis["automatic_analysis"]["unsupported_instruction_occurrences"],
                  "translation_units": generation["translation_units"]}
        results.append(result)
        print(f"{module['name']}: {result['function_seeds']} seeds, "
              f"{result['translation_units']} C++ units, {result['unsupported_occurrences']} unsupported", flush=True)
    summary = {"runtime_integration_complete": False, "modules": results}
    (profile / "analysis/overlay_codegen_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    write_bank(profile, layout["modules"], {Path(module["name"]).stem for module in modules})
    print(f"Translated {len(results)} overlays; selected modules are ready to link after CMake reconfiguration")


if __name__ == "__main__":
    main()
