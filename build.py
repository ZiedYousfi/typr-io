#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import zipfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IS_WINDOWS = os.name == "nt"
PLATFORM_TAG = "windows" if IS_WINDOWS else "linux"
BUILD_ROOT = ROOT / "build"
DEFAULT_BUILD_DIR = BUILD_ROOT / PLATFORM_TAG
VENDOR_LICENSES_DIR = ROOT / "vendor" / "licenses"
OBJ_DIR_NAME = "obj"
BIN_DIR_NAME = "bin"
LIB_DIR_NAME = "lib"
LIB_FILENAME = "libaxidev-io.a"

COMMON_SOURCES = [
    Path("src/c_api.c"),
    Path("src/core/context.c"),
    Path("src/core/log.c"),
    Path("src/internal/utf.c"),
    Path("src/vendor/stb_ds_impl.c"),
    Path("src/mouse/mouse_common.c"),
    Path("src/keyboard/common/key_utils.c"),
    Path("src/keyboard/common/keymap.c"),
]
UNIT_TEST_SOURCES = [
    Path("tests/test_key_utils.c"),
    Path("tests/test_c_api.c"),
]
INTEGRATION_TEST_SOURCES = [
    Path("tests/test_integration_sender.c"),
    Path("tests/test_integration_listener.c"),
]
EXAMPLE_SOURCE = Path("examples/example_c.c")
LINUX_PERMISSION_HELPER = Path("scripts/setup_uinput_permissions.sh")
COMPILE_COMMANDS_FILENAME = "compile_commands.json"


def split_flags(value: str | None) -> list[str]:
    if not value:
        return []
    return shlex.split(value, posix=not IS_WINDOWS)


def detect_arch() -> str:
    machine = platform.machine().lower()
    mapping = {
        "amd64": "x64",
        "x86_64": "x64",
        "x64": "x64",
        "arm64": "arm64",
        "aarch64": "arm64",
    }
    return mapping.get(machine, machine or "unknown")


def run_command(command: list[str]) -> None:
    print("+", " ".join(command))
    subprocess.run(command, check=True, cwd=ROOT)


def pkg_config_flags(pkg_config: str, flag: str, packages: list[str]) -> list[str]:
    try:
        output = subprocess.check_output(
            [pkg_config, flag, *packages],
            cwd=ROOT,
            text=True,
            stderr=subprocess.PIPE,
        ).strip()
    except subprocess.CalledProcessError as exc:
        message = exc.stderr.strip() or str(exc)
        raise SystemExit(f"pkg-config failed for {' '.join(packages)}: {message}") from exc

    return split_flags(output)


def ensure_linux_dynamic_link_flags(flags: list[str], source: str) -> None:
    forbidden_prefixes = ("-Wl,-Bstatic", "-Wl,--whole-archive")
    forbidden_exact = {"-static", "--static"}

    for flag in flags:
        if flag in forbidden_exact:
            raise SystemExit(
                f"{source} requested static linkage on Linux via {flag}. "
                "Linux dependencies must stay dynamically linked for legal/compliance reasons."
            )
        if flag.startswith(forbidden_prefixes):
            raise SystemExit(
                f"{source} requested static linkage on Linux via {flag}. "
                "Linux dependencies must stay dynamically linked for legal/compliance reasons."
            )
        if flag.endswith(".a") or ".a." in flag or flag.endswith(".a)"):
            raise SystemExit(
                f"{source} referenced a static archive on Linux via {flag}. "
                "Linux dependencies must stay dynamically linked for legal/compliance reasons."
            )
        if flag.startswith("-l:") and flag.endswith(".a"):
            raise SystemExit(
                f"{source} requested a static archive on Linux via {flag}. "
                "Linux dependencies must stay dynamically linked for legal/compliance reasons."
            )


@dataclass
class BuildConfig:
    build_dir: Path
    obj_dir: Path
    bin_dir: Path
    lib_dir: Path
    library_path: Path
    compiler: str
    archiver: str
    cppflags: list[str]
    cflags: list[str]
    ldflags: list[str]
    platform_libs: list[str]
    library_sources: list[Path]


def make_config(build_dir: Path) -> BuildConfig:
    compiler = os.environ.get("CC")
    if not compiler:
        compiler = "clang" if IS_WINDOWS else "cc"

    archiver = os.environ.get("AR")
    if not archiver:
        archiver = "llvm-ar" if IS_WINDOWS else "ar"

    cppflags = ["-Iinclude", "-Isrc", "-Ivendor", *split_flags(os.environ.get("CPPFLAGS"))]

    cflags = split_flags(os.environ.get("CFLAGS"))
    if not cflags:
        cflags = ["-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter"]

    ldflags = split_flags(os.environ.get("LDFLAGS"))
    ldlibs = split_flags(os.environ.get("LDLIBS"))

    library_sources = list(COMMON_SOURCES)
    platform_libs = list(ldlibs)

    if IS_WINDOWS:
        cppflags.extend(["-D_CRT_SECURE_NO_WARNINGS", "-DAXIDEV_IO_STATIC"])
        library_sources.extend(
            [
                Path("src/internal/thread_win32.c"),
                Path("src/keyboard/common/windows_keymap.c"),
                Path("src/keyboard/sender/sender_windows.c"),
                Path("src/keyboard/listener/listener_windows.c"),
                Path("src/mouse/mouse_windows.c"),
            ]
        )
        platform_libs.extend(["-luser32", "-lkernel32"])
    else:
        pkg_config = os.environ.get("PKG_CONFIG", "pkg-config")
        cppflags.append("-DAXIDEV_IO_STATIC")
        ensure_linux_dynamic_link_flags(ldflags, "LDFLAGS")
        ensure_linux_dynamic_link_flags(ldlibs, "LDLIBS")

        # Legal/compliance policy:
        # These Linux dependencies must remain dynamically linked. We only
        # discover their compile/link flags from the system at build time and
        # rely on the platform loader to resolve the shared libraries at
        # runtime. Do not switch this to pkg-config --static, ship .a files, or
        # force -static/-Wl,-Bstatic in environment overrides.
        cppflags.extend(
            pkg_config_flags(pkg_config, "--cflags", ["libinput", "libudev", "xkbcommon"])
        )
        linux_shared_libs = pkg_config_flags(
            pkg_config, "--libs", ["libinput", "libudev", "xkbcommon"]
        )
        ensure_linux_dynamic_link_flags(linux_shared_libs, "pkg-config --libs")
        platform_libs.extend(linux_shared_libs)
        platform_libs.append("-pthread")
        library_sources.extend(
            [
                Path("src/internal/thread_pthread.c"),
                Path("src/keyboard/common/linux_layout.c"),
                Path("src/keyboard/common/linux_keysym.c"),
                Path("src/keyboard/sender/sender_uinput.c"),
                Path("src/keyboard/listener/listener_linux.c"),
                Path("src/mouse/mouse_linux.c"),
            ]
        )

    obj_dir = build_dir / OBJ_DIR_NAME
    bin_dir = build_dir / BIN_DIR_NAME
    lib_dir = build_dir / LIB_DIR_NAME

    return BuildConfig(
        build_dir=build_dir,
        obj_dir=obj_dir,
        bin_dir=bin_dir,
        lib_dir=lib_dir,
        library_path=lib_dir / LIB_FILENAME,
        compiler=compiler,
        archiver=archiver,
        cppflags=cppflags,
        cflags=cflags,
        ldflags=ldflags,
        platform_libs=platform_libs,
        library_sources=library_sources,
    )


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def object_path(config: BuildConfig, source: Path) -> Path:
    return config.obj_dir / source.with_suffix(".o")


def binary_path(config: BuildConfig, source: Path) -> Path:
    suffix = ".exe" if IS_WINDOWS else ""
    return config.bin_dir / f"{source.stem}{suffix}"


def compile_source(config: BuildConfig, source: Path) -> Path:
    obj_path = object_path(config, source)
    ensure_parent(obj_path)
    run_command(compile_source_arguments(config, source, obj_path))
    return obj_path


def compile_source_arguments(
    config: BuildConfig, source: Path, output_path: Path | None = None
) -> list[str]:
    obj_path = output_path if output_path is not None else object_path(config, source)
    return [
        config.compiler,
        *config.cppflags,
        *config.cflags,
        "-c",
        str(source),
        "-o",
        str(obj_path),
    ]


def all_compilation_sources(config: BuildConfig) -> list[Path]:
    ordered_sources = [
        *config.library_sources,
        *UNIT_TEST_SOURCES,
        *INTEGRATION_TEST_SOURCES,
        EXAMPLE_SOURCE,
    ]
    unique_sources: list[Path] = []
    seen: set[Path] = set()
    for source in ordered_sources:
        if source in seen:
            continue
        seen.add(source)
        unique_sources.append(source)
    return unique_sources


def write_compile_commands(config: BuildConfig) -> Path:
    database_path = ROOT / COMPILE_COMMANDS_FILENAME
    entries = []

    for source in all_compilation_sources(config):
        output_path = object_path(config, source)
        entries.append(
            {
                "directory": str(ROOT),
                "file": str((ROOT / source).resolve()),
                "arguments": compile_source_arguments(config, source, output_path),
                "output": str((ROOT / output_path).resolve()),
            }
        )

    with database_path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(entries, handle, indent=2)
        handle.write("\n")

    print(f"Created {database_path}")
    return database_path


def build_library(config: BuildConfig) -> Path:
    objects = [compile_source(config, source) for source in config.library_sources]
    config.lib_dir.mkdir(parents=True, exist_ok=True)
    run_command([config.archiver, "rcs", str(config.library_path), *map(str, objects)])
    return config.library_path


def build_binary(config: BuildConfig, source: Path) -> Path:
    library_path = build_library(config)
    output_path = binary_path(config, source)
    ensure_parent(output_path)
    run_command(
        [
            config.compiler,
            *config.cppflags,
            *config.cflags,
            str(source),
            str(library_path),
            *config.ldflags,
            *config.platform_libs,
            "-o",
            str(output_path),
        ]
    )
    return output_path


def run_binary(binary: Path) -> None:
    run_command([str(binary)])


def clean(config: BuildConfig) -> None:
    for path in (BUILD_ROOT, ROOT / "dist", ROOT / "packages"):
        if path.exists():
            print(f"Removing {path}")
            shutil.rmtree(path)


def copy_tree(source_dir: Path, destination_dir: Path) -> None:
    shutil.copytree(source_dir, destination_dir, dirs_exist_ok=True)


def copy_file(source: Path, destination: Path) -> None:
    ensure_parent(destination)
    shutil.copy2(source, destination)


def write_zip_tree(archive_path: Path, source_dir: Path) -> None:
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for file_path in source_dir.rglob("*"):
            if not file_path.is_file():
                continue

            archive_name = file_path.relative_to(source_dir).as_posix()
            info = zipfile.ZipInfo.from_file(file_path, archive_name)
            info.create_system = 3
            info.external_attr = (file_path.stat().st_mode & 0xFFFF) << 16
            info.compress_type = zipfile.ZIP_DEFLATED

            with file_path.open("rb") as handle:
                archive.writestr(info, handle.read())


def package_output(config: BuildConfig, version: str, arch: str) -> Path:
    library_path = build_library(config)
    dist_dir = ROOT / "dist"
    packages_dir = ROOT / "packages"
    archive_base = f"axidev-io-{version}-{PLATFORM_TAG}-{arch}"

    if dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True, exist_ok=True)
    packages_dir.mkdir(parents=True, exist_ok=True)

    copy_tree(ROOT / "include", dist_dir / "include")
    copy_tree(VENDOR_LICENSES_DIR, dist_dir / "vendor" / "licenses")
    copy_file(library_path, dist_dir / "lib" / LIB_FILENAME)
    copy_file(ROOT / "docs" / "consumers" / "README.md", dist_dir / "docs" / "consumers" / "README.md")
    copy_file(ROOT / "README.md", dist_dir / "README.md")
    copy_file(ROOT / "LICENSE", dist_dir / "LICENSE")

    if IS_WINDOWS:
        archive_path = packages_dir / f"{archive_base}.zip"
        write_zip_tree(archive_path, dist_dir)
    else:
        archive_path = packages_dir / f"{archive_base}.tar.gz"
        with tarfile.open(archive_path, "w:gz") as archive:
            archive.add(dist_dir, arcname=".")

    print(f"Created {archive_path}")
    return archive_path


def package_integration_tests_output(config: BuildConfig, version: str, arch: str) -> Path:
    binaries = [build_binary(config, source) for source in INTEGRATION_TEST_SOURCES]
    dist_dir = ROOT / "dist"
    packages_dir = ROOT / "packages"
    archive_base = f"axidev-io-integration-tests-{version}-{PLATFORM_TAG}-{arch}"

    if dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True, exist_ok=True)
    packages_dir.mkdir(parents=True, exist_ok=True)

    for binary in binaries:
        copy_file(binary, dist_dir / "bin" / binary.name)

    copy_tree(VENDOR_LICENSES_DIR, dist_dir / "vendor" / "licenses")
    copy_file(ROOT / "docs" / "consumers" / "README.md", dist_dir / "docs" / "consumers" / "README.md")
    copy_file(ROOT / "README.md", dist_dir / "README.md")
    copy_file(ROOT / "LICENSE", dist_dir / "LICENSE")

    if not IS_WINDOWS:
        copy_file(LINUX_PERMISSION_HELPER, dist_dir / "scripts" / LINUX_PERMISSION_HELPER.name)

    archive_path = packages_dir / f"{archive_base}.zip"
    write_zip_tree(archive_path, dist_dir)
    print(f"Created {archive_path}")
    return archive_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build axidev-io without make.")
    parser.add_argument(
        "command",
        nargs="?",
        default="build",
        choices=[
            "build",
            "compile-commands",
            "test",
            "test-unit",
            "test-integration",
            "unit-binaries",
            "integration-binaries",
            "package-integration-tests",
            "example",
            "clean",
            "package",
        ],
    )
    parser.add_argument(
        "--build-dir",
        default=str(DEFAULT_BUILD_DIR),
        help="Build output directory. Defaults to build/<platform>.",
    )
    parser.add_argument(
        "--version",
        default="dev",
        help="Version label for the package command.",
    )
    parser.add_argument(
        "--arch",
        default=detect_arch(),
        help="Architecture label for the package command.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = make_config(Path(args.build_dir))

    if args.command == "clean":
        clean(config)
        return 0

    if args.command == "build":
        build_library(config)
        return 0

    if args.command == "compile-commands":
        write_compile_commands(config)
        return 0

    if args.command == "unit-binaries":
        for source in UNIT_TEST_SOURCES:
            build_binary(config, source)
        return 0

    if args.command == "integration-binaries":
        for source in INTEGRATION_TEST_SOURCES:
            build_binary(config, source)
        return 0

    if args.command == "test":
        args.command = "test-unit"

    if args.command == "test-unit":
        binaries = [build_binary(config, source) for source in UNIT_TEST_SOURCES]
        for binary in binaries:
            run_binary(binary)
        return 0

    if args.command == "test-integration":
        binaries = [build_binary(config, source) for source in INTEGRATION_TEST_SOURCES]
        for binary in binaries:
            run_binary(binary)
        return 0

    if args.command == "example":
        build_binary(config, EXAMPLE_SOURCE)
        return 0

    if args.command == "package":
        package_output(config, args.version, args.arch)
        return 0

    if args.command == "package-integration-tests":
        package_integration_tests_output(config, args.version, args.arch)
        return 0

    raise SystemExit(f"Unsupported command: {args.command}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode) from exc
