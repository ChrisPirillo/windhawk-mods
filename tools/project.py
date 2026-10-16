"""Portable, standard-library-only entry point; no installation or UI input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def closure():
    h = hashlib.sha256()
    for folder in (ROOT / "tests/components", ROOT / "tests/manifests"):
        for p in sorted(folder.rglob("*")):
            if p.is_file():
                h.update(p.relative_to(ROOT).as_posix().encode())
                h.update(p.read_bytes())
    h.update((ROOT / "tests/matrix.json").read_bytes())
    return h.hexdigest()


def verify():
    entries = read(ROOT / "MANIFEST.json")
    for item in entries:
        p = (ROOT / item["name"]).resolve()
        if not p.is_relative_to(ROOT) or digest(p) != item["sha256"]:
            raise RuntimeError("Kit integrity mismatch: " + item["name"])
    state = read(ROOT / "STATE.json")
    assert digest(ROOT / "source/win32-ui-modernizer.wh.cpp") == state["source_sha256"]
    print(f"Verified {len(entries)} kit files; no tests or installed changes.")


def build(args):
    compiler = args.compiler
    if not compiler:
        default = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Windhawk/Compiler/bin/clang++.exe"
        compiler = str(default) if default.is_file() else shutil.which("clang++")
    if not compiler:
        raise RuntimeError("Supply --compiler with a Windows-targeting LLVM-MinGW clang++.")
    compiler = str(Path(compiler).resolve())
    build_dir = ROOT / "build"
    (build_dir / "temp").mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "TEMP": str(build_dir / "temp"), "TMP": str(build_dir / "temp")}
    records = []
    targets = {"x64": "x86_64-w64-mingw32", "x86": "i686-w64-mingw32"}
    for case in read(ROOT / "tests/matrix.json"):
        source = (ROOT / "tests/components" / case["source"]).resolve()
        if not source.is_relative_to(ROOT / "tests/components"):
            raise RuntimeError("Case source escapes the kit.")
        for arch in args.arch:
            name = case["name"] + "-" + arch
            folder = build_dir / "components" / name
            folder.mkdir(parents=True, exist_ok=True)
            exe = folder / "dwm.exe"  # Deliberately matches the mod's exclusion.
            command = [compiler, "-target", targets[arch], "-std=c++23", "-O1", "-static",
                       str(source), "-o", str(exe), "-luser32", "-lgdi32", "-lcomctl32", "-luxtheme", "-ldwmapi"]
            result = subprocess.run(command, env=env, capture_output=True, timeout=120)
            (folder / "build.log").write_bytes(result.stdout + result.stderr)
            record = {"name": name, "exit": result.returncode,
                      "executable": exe.relative_to(ROOT).as_posix()}
            if result.returncode == 0:
                record["executable_sha256"] = digest(exe)
            records.append(record)
            save(build_dir / "components-build.json", {"closure_sha256": closure(), "cases": records})
            print(f"build {name}: {result.returncode}", flush=True)
            if result.returncode:
                raise RuntimeError("Build failed; inspect " + str(folder / "build.log"))
            manifests = ROOT / "tests/manifests" / name
            if manifests.is_dir():
                for manifest in manifests.glob("*.manifest"):
                    shutil.copyfile(manifest, folder / manifest.name)
    print("Component builds complete. Nothing installed or launched.")


def run(args):
    if os.name != "nt":
        raise RuntimeError("Native Windows execution required; cross-compilation is not acceptance.")
    if not args.run_hidden:
        raise RuntimeError("Specify --run-hidden to authorize the kit's own hidden native fixtures.")
    evidence = read(ROOT / "build/components-build.json")
    if evidence["closure_sha256"] != closure():
        raise RuntimeError("Test sources changed after building; rebuild first.")
    records = []
    expected = len(read(ROOT / "tests/matrix.json")) * len(args.arch)
    selected = [x for x in evidence["cases"] if any(x["name"].endswith("-" + a) for a in args.arch)]
    if len(selected) != expected:
        raise RuntimeError("Requested architecture matrix has not been built.")
    for case in selected:
        exe = (ROOT / case["executable"]).resolve()
        if not exe.is_relative_to(ROOT / "build") or case["exit"] != 0 or digest(exe) != case["executable_sha256"]:
            raise RuntimeError("Missing, failed or changed owned fixture: " + case["name"])
        result = subprocess.run([str(exe)], cwd=exe.parent, capture_output=True,
                                timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        (exe.parent / "console.txt").write_bytes(result.stdout + result.stderr)
        records.append({"name": case["name"], "exit": result.returncode})
        save(ROOT / "build/components-results.json", {"cases": records, "installed_rendering_accepted": False})
        print(f"test {case['name']}: {result.returncode}", flush=True)
        if result.returncode:
            raise RuntimeError("Component failed; failures are not acceptance passes.")
    print(f"{len(records)} component cases passed. Injected UI/rendering remains separate.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["verify", "build-tests", "run-tests"])
    parser.add_argument("--compiler")
    parser.add_argument("--arch", nargs="+", choices=["x64", "x86"], default=["x64", "x86"])
    parser.add_argument("--run-hidden", action="store_true")
    args = parser.parse_args()
    if len(args.arch) != len(set(args.arch)):
        parser.error("Do not repeat architectures.")
    {"verify": lambda: verify(), "build-tests": lambda: build(args), "run-tests": lambda: run(args)}[args.action]()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, AssertionError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
