"""Post-build script for PlatformIO (and standalone CLI) to copy ESP32 family binaries into firmware/<target>/."""

import os
import shutil
import sys
from pathlib import Path


BOOTLOADER_OFFSETS = {
    "esp32": ("0x1000", 4096),
    "esp32s2": ("0x1000", 4096),
    "esp32s3": ("0x0000", 0),
    "esp32c3": ("0x0000", 0),
}


def get_bin_specs(env_name: str) -> list[tuple[str, str, int]]:
    """Return binary filenames and flash offsets for the given ESP32 family."""
    boot_hex, boot_dec = BOOTLOADER_OFFSETS.get(env_name, ("0x0000", 0))
    return [
        ("bootloader.bin", boot_hex, boot_dec),
        ("partitions.bin", "0x8000", 32768),
        ("boot_app0.bin", "0xE000", 57344),
        ("firmware.bin", "0x10000", 65536),
    ]


def copy_binaries(
    project_dir: Path, build_dir: Path, progname: str, framework_dir: Path, env_name: str
) -> bool:
    """Copy bootloader, partitions, boot_app0 and firmware binaries into firmware/<env_name>/."""
    dest_dir = project_dir / "firmware" / env_name
    dest_dir.mkdir(parents=True, exist_ok=True)

    sources = {
        "bootloader.bin": build_dir / "bootloader.bin",
        "partitions.bin": build_dir / "partitions.bin",
        "boot_app0.bin": framework_dir / "tools" / "partitions" / "boot_app0.bin",
        "firmware.bin": build_dir / f"{progname}.bin",
    }

    print("\n" + "=" * 68)
    print(f"[copy_firmware.py] Esportazione binari [{env_name}] per GitHub Pages")
    print(f"Destinazione: {dest_dir}")
    print("-" * 68)

    missing = []
    for filename, offset_hex, offset_dec in get_bin_specs(env_name):
        src = sources[filename]
        dst = dest_dir / filename

        if not src.is_file():
            missing.append((filename, src))
            print(f"  [ERRORE] Mancante: {src}")
            continue

        shutil.copy2(src, dst)
        size_bytes = dst.stat().st_size
        print(
            f"  [OK] {filename:<16} | {size_bytes:>8} byte | "
            f"Offset: {offset_hex:<7} ({offset_dec})"
        )

    print("=" * 68 + "\n")

    if missing:
        print(
            f"[copy_firmware.py] ERRORE [{env_name}]: uno o piu file binari non trovati.",
            file=sys.stderr,
        )
        return False

    return True


def _run_pio_mode() -> None:
    """Register SCons post-action when invoked by PlatformIO."""
    Import("env")  # noqa: F821 - injected by PlatformIO/SCons

    def post_build_action(source, target, env):  # noqa: ARG001
        project_dir = Path(env.subst("$PROJECT_DIR"))
        build_dir = Path(env.subst("$BUILD_DIR"))
        progname = env.subst("${PROGNAME}")
        env_name = env.subst("$PIOENV")
        framework_dir = Path(
            env.PioPlatform().get_package_dir("framework-arduinoespressif32")
        )

        ok = copy_binaries(project_dir, build_dir, progname, framework_dir, env_name)
        if not ok:
            env.Exit(1)

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_build_action)


def _run_standalone_mode() -> int:
    """Allow manual execution via `python copy_firmware.py` for all built environments."""
    project_dir = Path(__file__).resolve().parent
    pio_build_root = project_dir / ".pio" / "build"

    if not pio_build_root.is_dir():
        print(
            f"Cartella di build non trovata: {pio_build_root}. Esegui prima `pio run`.",
            file=sys.stderr,
        )
        return 1

    env_dirs = [d for d in pio_build_root.iterdir() if d.is_dir()]
    if not env_dirs:
        print("Nessun ambiente compilato trovato in .pio/build/.", file=sys.stderr)
        return 1

    pio_home = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    framework_dir = pio_home / "packages" / "framework-arduinoespressif32"

    all_ok = True
    for build_dir in env_dirs:
        if not copy_binaries(project_dir, build_dir, "firmware", framework_dir, build_dir.name):
            all_ok = False

    return 0 if all_ok else 1


if "Import" in globals():
    _run_pio_mode()
elif __name__ == "__main__":
    sys.exit(_run_standalone_mode())
