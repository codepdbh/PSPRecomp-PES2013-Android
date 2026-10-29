#!/usr/bin/env python3
"""Analyze and translate overlay ELFs into isolated, experimental C++ namespaces.

Outputs remain in analysis/. They are not linked into the game: runtime overlay
activation, old-code removal and dispatch-layout integration are still pending.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


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
        with log.open("w", encoding="utf-8") as stream:
            subprocess.run([str(analyzer), str(elf), str(report)],
                           stdout=stream, stderr=subprocess.STDOUT, check=True)
            subprocess.run([str(generator), str(elf), "--auto", str(output),
                            "0x08804000", "0x4000", "--generic"],
                           stdout=stream, stderr=subprocess.STDOUT, check=True)
        namespace = "pes_overlay_" + name
        for source in list(output.glob("*.cpp")) + list(output.glob("*.hpp")):
            content = source.read_text(encoding="utf-8")
            if "namespace psprecomp {" not in content:
                raise ValueError(f"Unexpected codegen namespace in {source}")
            content = content.replace("namespace psprecomp {", "namespace psprecomp::" + namespace + " {")
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
                  "function_seeds": analysis["function_seed_count"],
                  "unsupported_occurrences": analysis["automatic_analysis"]["unsupported_instruction_occurrences"],
                  "translation_units": generation["translation_units"]}
        results.append(result)
        print(f"{module['name']}: {result['function_seeds']} seeds, "
              f"{result['translation_units']} C++ units, {result['unsupported_occurrences']} unsupported", flush=True)
    summary = {"runtime_integration_complete": False, "modules": results}
    (profile / "analysis/overlay_codegen_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Translated {len(results)} overlays in separate namespaces; outputs are not yet linked")


if __name__ == "__main__":
    main()
