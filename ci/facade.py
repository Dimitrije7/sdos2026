#!/usr/bin/env python3
"""SDOS command facade: the single implementation behind ``make <command>``.

Students, agents, and CI run the same commands; workflow files call ``make``
and contain no build logic of their own. See docs/commands.md for the contract.

Build commands run inside the pinned container from toolchain/Dockerfile, which
``make bootstrap`` builds. Set ``SDOS_NATIVE=1`` to use tools installed on the
host instead.

Exit status: 0 success, 1 a check or build failed, 2 usage error,
3 refused for safety, 4 the command or target is not available yet.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence

try:
    from ci import container, ti_reference
except ImportError:  # run as a script: ci/ is on sys.path, the repository root is not
    import container  # type: ignore[no-redef]
    import ti_reference  # type: ignore[no-redef]

ROOT = Path(__file__).resolve().parents[1]
EXIT_OK, EXIT_FAILED, EXIT_USAGE, EXIT_REFUSED, EXIT_NOT_YET = 0, 1, 2, 3, 4

# Commands that exist in the contract but whose implementation belongs to a
# later task. They fail explicitly instead of pretending to work.
NOT_YET = {
    "package": "Task 022 packages validated firmware and host artifacts",
    "deploy": "Task 030 deploys to a board through dspctl",
    "target-smoke": "Task 038 runs on-board smoke tests",
}

# Programs the host presets need, with the minimum version where it matters.
HOST_TOOLS = (
    ("cmake", "3.22"),
    ("ninja", "1.10"),
    ("gcc-12", None),
    ("clang-15", None),
)
BOARD_TOOLS = (("aarch64-linux-gnu-gcc-12", None),)
TI_ROOTS = (("SDOS_TI_C6000_ROOT", "cl6x"), ("SDOS_TI_C7000_ROOT", "cl7x"))

PYTHON_TEST_MODULES = (
    "ci.test_validate_hardware",
    "ci.test_validate_licenses",
    "ci.test_validate_versions",
    "ci.test_facade",
    "ci.test_container",
    "ci.test_workflows",
    "ci.test_firmware_elf",
    "ci.test_image",
    "tests.target.test_sdos_board",
    "tests.target.test_dsp_ipc",
    "tests.target.test_course_smoke",
)
VALIDATORS = (
    "ci/validate_hardware.py",
    "ci/validate_licenses.py",
    "ci/validate_versions.py",
    "ci/validate_workflows.py",
)
DOCKERFILES = ("toolchain/Dockerfile",)
# Course-owned portable C. DSP-only sources need TI headers, which cppcheck
# cannot read, so they are checked by the TI compiler with warnings as errors.
CPPCHECK = [
    "cppcheck", "--quiet", "--error-exitcode=1", "--std=c99", "--inline-suppr",
    "--enable=warning,style,performance,portability", "--suppress=missingIncludeSystem",
    "--suppress=unusedFunction", "--suppress=unmatchedSuppression",
    "-I", "firmware/common/protocol/include", "-I", "firmware/common/runtime/include",
    "-I", "host/libsdos_dsp/include", "-I", "algorithms/fir_q15/include", "-I", "tests/unit/include",
    "-D__GNUC__", "-DSDOS_CORES_MACROS_ONLY",
    "firmware/common/protocol", "firmware/common/runtime", "host", "algorithms",
]


def lint_steps(root: Path = ROOT) -> list[list[str]]:
    shell = sorted(str(p.relative_to(root)) for p in (root / "ci").rglob("*.sh"))
    workflows = sorted(str(p.relative_to(root)) for p in (root / ".github" / "workflows").glob("*.yml"))
    steps = [["hadolint", *DOCKERFILES], ["actionlint", *workflows], ["zizmor", "--offline", *workflows],
             ["yamllint", "--strict", ".github", "compose.yaml", ".yamllint.yaml", ".markdownlint-cli2.yaml"],
             ["markdownlint-cli2", "**/*.md"]]
    if shell:
        steps.append(["shellcheck", *shell])
    steps.append(CPPCHECK)
    return steps


class UsageError(Exception):
    pass


class NotYet(Exception):
    pass


def presets(root: Path = ROOT) -> list[str]:
    data = json.loads((root / "CMakePresets.json").read_text(encoding="utf-8"))
    cmake = [item["name"] for item in data["configurePresets"] if not item.get("hidden")]
    return cmake + ti_reference.presets()


def groups(names: Sequence[str]) -> dict[str, list[str]]:
    return {
        "host": [n for n in names if n.startswith("host-")],
        "board-host": [n for n in names if n.startswith("aarch64-")],
        "firmware": [n for n in names if re.match(r"c66x_|c71x_", n)],
        "ti-reference": [n for n in names if n.startswith(ti_reference.PREFIX)],
        "all": list(names),
    }


def expand(selection: str, names: Sequence[str]) -> list[str]:
    """Turn ``host-gcc-debug``, ``host``, or ``a,b`` into preset names."""
    known = groups(names)
    result: list[str] = []
    for part in filter(None, (item.strip() for item in selection.split(","))):
        if part in known:
            result.extend(known[part])
        elif part in names:
            result.append(part)
        else:
            raise UsageError(
                f"unknown preset '{part}'. Groups: {', '.join(known)}. Presets: {', '.join(names)}"
            )
    if not result:
        raise UsageError("no preset selected")
    return list(dict.fromkeys(result))


class Runner:
    def __init__(self, dry_run: bool) -> None:
        self.dry_run = dry_run

    def __call__(self, argv: Sequence[str]) -> int:
        print("+ " + " ".join(argv), flush=True)
        if self.dry_run:
            return 0
        return subprocess.run(list(argv), cwd=ROOT, check=False).returncode


def _version(program: str) -> str | None:
    try:
        out = subprocess.run([program, "--version"], capture_output=True, text=True, check=False)
    except OSError:
        return None
    match = re.search(r"\d+\.\d+(\.\d+)?", out.stdout + out.stderr)
    return match.group(0) if match else "unknown"


def _at_least(found: str, wanted: str) -> bool:
    def parts(value: str) -> tuple[int, ...]:
        return tuple(int(p) for p in value.split(".") if p.isdigit())

    return parts(found) >= parts(wanted)


IN_CONTAINER_COMMANDS = ("bootstrap", "configure", "build", "test", "quality", "image")
IMAGE_RECIPE = ROOT / "image" / "recipe.json"


def _ti_notice() -> str:
    lines = [
        "The DSP firmware presets need TI's C6000 and C7000 compilers. They are free of",
        "charge but proprietary: TI's terms are shown on these pages and in the manifest",
        "file each installer installs. The project cannot redistribute them, so your",
        "machine downloads them from TI once you accept the terms:",
    ]
    lines += [f"  {item['id']}: {item['terms_page']}" for item in container.ti_terms()]
    lines.append("To accept and add them: make bootstrap TI=accept")
    lines.append("Building without them now; host and board-host presets work.")
    return "\n".join(lines)


TI_NOTICE = _ti_notice()


def _native() -> bool:
    return os.environ.get("SDOS_IN_CONTAINER") == "1" or os.environ.get("SDOS_NATIVE") == "1"


def _container_args(args: argparse.Namespace) -> list[str]:
    forwarded = [args.command, "--preset", args.preset]
    if getattr(args, "ti", ""):
        forwarded += ["--ti", args.ti]
    if args.werror:
        forwarded.append("--werror")
    return ["python3", "ci/facade.py", *forwarded]


def in_container(args: argparse.Namespace, run: Runner) -> int:
    """Build the image for bootstrap, or rerun this command inside it."""
    if args.command == "bootstrap" and args.ti == "accept" and not run.dry_run:
        print(f"recorded TI terms acceptance in {container.record_ti_acceptance()}")
    ti = args.ti == "accept" or container.ti_accepted()
    tag = container.image_tag(ti=ti)
    rootless = True if run.dry_run else None  # a dry run does not query Docker
    if not run.dry_run and not container.docker_available():
        print(
            "Docker is not available. Install Docker Engine or Docker Desktop, or set "
            "SDOS_NATIVE=1 to use host tools (see docs/commands.md).",
            file=sys.stderr,
        )
        return EXIT_FAILED
    if args.command == "bootstrap":
        if not ti:
            print(TI_NOTICE, file=sys.stderr)
        elif not run.dry_run:
            print("checking the TI SDK archive in the input cache (3.7 GB on first use)", flush=True)
            container.fetch_input(container.PSDK_INPUT)
        code = run(container.build_command(ti=ti))
        if code == 0 and not run.dry_run:
            _remove_stale_images(run)
        return code or run(container.run_command(tag, _container_args(args), rootless))
    if args.command == "image" and not run.dry_run:
        base = json.loads(IMAGE_RECIPE.read_text(encoding="utf-8"))["base"]
        print(f"checking the base image {base} in the input cache (1.1 GB on first use)", flush=True)
        container.fetch_input(base)
    if not run.dry_run and not container.image_present(tag):
        if not ti and container.image_present(container.image_tag(ti=True)):
            print(
                f"the pinned image {tag} is not built, but the TI image is. This account has not "
                "accepted TI's terms: run `make bootstrap TI=accept` (it reuses the built image)",
                file=sys.stderr,
            )
        else:
            print(f"the pinned image {tag} is not built; run `make bootstrap` first", file=sys.stderr)
        return EXIT_FAILED
    return run(container.run_command(tag, _container_args(args), rootless))


def _remove_stale_images(run: Runner) -> None:
    """Untag images built from earlier inputs; both current variants stay."""
    stale = container.stale_tags({container.image_tag(ti=False), container.image_tag(ti=True)})
    if stale:
        run(["docker", "image", "rm", *stale])


def cmd_bootstrap(args: argparse.Namespace, run: Runner) -> int:
    missing = []
    for program, minimum in HOST_TOOLS + BOARD_TOOLS:
        version = _version(program) if shutil.which(program) else None
        if version is None or (minimum and not _at_least(version, minimum)):
            missing.append(program)
            state = f"missing (need {minimum or 'any version'})" if version is None else f"{version}, need {minimum}"
        else:
            state = version
        print(f"{program:28} {state}")
    for variable, compiler in TI_ROOTS:
        root = os.environ.get(variable, "")
        ok = bool(root) and (Path(root) / "bin" / compiler).exists()
        print(f"{variable:28} {root if ok else 'not set; firmware presets need it (Task 019)'}")
    if missing:
        print(
            "bootstrap: missing " + ", ".join(missing) + ". Run without SDOS_NATIVE=1 "
            "to use the pinned container, which provides every tool.",
            file=sys.stderr,
        )
        return EXIT_FAILED
    if _ti_installed():
        print("bootstrap: host, board-host, and firmware presets can build")
    else:
        print("bootstrap: host and board-host presets can build")
    return EXIT_OK


def _configure(preset: str, run: Runner, werror: bool) -> int:
    return run(["cmake", "--preset", preset, f"-DSDOS_WERROR={'ON' if werror else 'OFF'}"])


def _build(preset: str, run: Runner, werror: bool) -> int:
    if preset.startswith(ti_reference.PREFIX):
        return ti_reference.build(preset, run, run.dry_run)
    return _configure(preset, run, werror) or run(["cmake", "--build", "--preset", preset])


def _for_each(selection: list[str], step) -> int:
    failed = [preset for preset in selection if step(preset) != 0]
    if failed:
        print("failed: " + ", ".join(failed), file=sys.stderr)
        return EXIT_FAILED
    return EXIT_OK


def cmd_configure(args: argparse.Namespace, run: Runner) -> int:
    cmake = [p for p in args.selection if not p.startswith(ti_reference.PREFIX)]
    return _for_each(cmake, lambda p: _configure(p, run, args.werror))


def cmd_build(args: argparse.Namespace, run: Runner) -> int:
    return _for_each(args.selection, lambda p: _build(p, run, args.werror))


def _test(preset: str, run: Runner, werror: bool) -> int:
    """Build, run the tests, and for the coverage preset report coverage."""
    code = _build(preset, run, werror) or run(["ctest", "--preset", preset])
    if code == 0 and preset.endswith("-coverage"):
        code = run([sys.executable, "ci/coverage.py", f"build/{preset}"])
    return code


def cmd_test(args: argparse.Namespace, run: Runner) -> int:
    off_host = [p for p in args.selection if not p.startswith("host-")]
    if off_host:
        raise NotYet(
            "tests run on host-* presets; " + ", ".join(off_host) + " need a board "
            "or emulator, which Task 033 adds"
        )
    return _for_each(args.selection, lambda p: _test(p, run, args.werror))


def _ti_installed() -> bool:
    return all(Path(os.environ.get(v, "/nonexistent"), "bin", c).exists() for v, c in TI_ROOTS)


def cmd_quality(args: argparse.Namespace, run: Runner) -> int:
    steps = [[sys.executable, path] for path in VALIDATORS] + lint_steps()
    steps.append([sys.executable, "-m", "unittest", *PYTHON_TEST_MODULES])
    failed = [" ".join(step[1:]) for step in steps if run(step) != 0]
    args.werror = True
    host = [p for p in args.selection if p.startswith("host-")]
    firmware = [p for p in args.selection if not p.startswith("host-")]
    if _for_each(host, lambda p: _test(p, run, True)) != 0:
        failed.append("host build and tests")
    if _for_each(firmware, lambda p: _build(p, run, True)) != 0:
        failed.append("firmware build")
    if failed:
        print("quality failed: " + "; ".join(failed), file=sys.stderr)
        return EXIT_FAILED
    return EXIT_OK


def cmd_image(args: argparse.Namespace, run: Runner) -> int:
    """Build what the recipe installs, then the image from the pinned base."""
    needed = ["aarch64-gcc-release"]
    if _ti_installed():
        needed += [p for p in presets() if re.match(r"c(66|71)x_\d-(freertos|baremetal)-release$", p)]
    else:
        print("image: TI tools are not installed, so the image carries no DSP firmware", file=sys.stderr)
    if _for_each(needed, lambda p: _build(p, run, True)) != 0:
        return EXIT_FAILED
    recipe = json.loads(IMAGE_RECIPE.read_text(encoding="utf-8"))
    item = {i["id"]: i for i in json.loads(container.MANIFEST.read_text(encoding="utf-8"))["inputs"]}[recipe["base"]]
    base = Path(os.environ.get("SDOS_INPUT_CACHE", str(container.INPUT_CACHE))) / item["source"]["file_name"]
    return run([sys.executable, "image/build.py", "--base", str(base), "--out", "build/image"])


def cmd_clean(args: argparse.Namespace, run: Runner) -> int:
    print("+ remove build/", flush=True)
    if not run.dry_run:
        shutil.rmtree(ROOT / "build", ignore_errors=True)
    return EXIT_OK


def cmd_not_yet(args: argparse.Namespace, run: Runner) -> int:
    if args.command in ("deploy", "target-smoke") and not args.board:
        raise UsageError(f"{args.command} needs BOARD=<name>")
    raise NotYet(f"'{args.command}' is not implemented yet: {NOT_YET[args.command]}")


HANDLERS = {
    "bootstrap": cmd_bootstrap,
    "configure": cmd_configure,
    "build": cmd_build,
    "test": cmd_test,
    "quality": cmd_quality,
    "clean": cmd_clean,
    "image": cmd_image,
    **{name: cmd_not_yet for name in NOT_YET},
}


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="make", description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=sorted(HANDLERS))
    parser.add_argument("--preset", default="host-gcc-debug", help="preset, group, or comma list")
    parser.add_argument("--board", default="", help="board name for deploy and target-smoke")
    parser.add_argument("--dry-run", action="store_true", help="print commands without running them")
    parser.add_argument("--werror", action="store_true", help="treat compiler warnings as errors")
    parser.add_argument("--ti", default="", choices=["", "accept"], help="accept TI's compiler terms")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    try:
        args = parse_args(sys.argv[1:] if argv is None else argv)
    except SystemExit as exc:
        return EXIT_USAGE if exc.code else EXIT_OK
    try:
        if args.command in ("configure", "build", "test", "quality"):
            selection = args.preset
            if args.command == "quality":
                # Release TI references only: TI ships release libraries, so a debug
                # reference recompiles the whole PDK; `make build PRESET=ti-reference` covers it.
                selection = "host,firmware," + ",".join(
                    p for p in ti_reference.presets() if p.endswith("-release")
                ) if _ti_installed() else "host"
            args.selection = expand(selection, presets())
        run = Runner(args.dry_run)
        if args.command in IN_CONTAINER_COMMANDS and not _native():
            return in_container(args, run)
        return HANDLERS[args.command](args, run)
    except UsageError as exc:
        print(f"usage error: {exc}", file=sys.stderr)
        return EXIT_USAGE
    except NotYet as exc:
        print(f"not available yet: {exc}", file=sys.stderr)
        return EXIT_NOT_YET


if __name__ == "__main__":
    raise SystemExit(main())
