#!/usr/bin/env python3
"""Check experimental overlay C++ with the Android ARM64 compiler, without linking."""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import subprocess
import sys
from pathlib import Path


def main() -> None:
    profile = Path(__file__).resolve().parents[1]
    repo = profile.parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be between 1 and 8")
    host = "windows-x86_64" if sys.platform == "win32" else "darwin-x86_64" if sys.platform == "darwin" else "linux-x86_64"
    toolchain = args.ndk / "toolchains/llvm/prebuilt" / host
    compiler = toolchain / "bin" / ("clang++.exe" if sys.platform == "win32" else "clang++")
    if not compiler.is_file():
        raise FileNotFoundError(compiler)
    summary = json.loads((profile / "analysis/overlay_codegen_summary.json").read_text())
    files = []
    for module in summary["modules"]:
        directory = profile / "analysis/overlay_codegen" / Path(module["name"]).stem
        files.extend(sorted(directory.glob("*.cpp")))
    if not files:
        raise ValueError("Translate the overlays first")

    def check(source: Path):
        result = subprocess.run([str(compiler), "--target=aarch64-none-linux-android26",
            "--sysroot=" + str(toolchain / "sysroot"), "-std=c++20", "-fsyntax-only",
            "-I" + str(repo / "include"), "-I" + str(source.parent), str(source)],
            capture_output=True, text=True)
        return source, result

    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        pending = [pool.submit(check, source) for source in files]
        for count, future in enumerate(concurrent.futures.as_completed(pending), 1):
            source, result = future.result()
            if result.returncode != 0:
                failures.append({"file": str(source.relative_to(repo)), "error": result.stderr})
            if count % 64 == 0 or count == len(files):
                print(f"Checked {count}/{len(files)} overlay C++ files; failures: {len(failures)}", flush=True)
    report = {"target": "aarch64-none-linux-android26", "files_checked": len(files),
              "modules_checked": len(summary["modules"]), "failures": failures,
              "runtime_integration_complete": False}
    (profile / "analysis/overlay_syntax_check.json").write_text(json.dumps(report, indent=2) + "\n")
    if failures:
        print(failures[0]["error"], file=sys.stderr)
        raise SystemExit(1)
    print("All overlay C++ files passed ARM64 syntax checking; linking and execution remain pending")


if __name__ == "__main__":
    main()
