#!/usr/bin/env python3
"""Load a packaged CX module from an isolated staging directory."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path
import subprocess
import sys


def load_module(path: Path) -> None:
    if sys.platform == "win32":
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        load_library = kernel32.LoadLibraryExW
        load_library.argtypes = (ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint)
        load_library.restype = ctypes.c_void_p
        free_library = kernel32.FreeLibrary
        free_library.argtypes = (ctypes.c_void_p,)
        free_library.restype = ctypes.c_int
        handle = load_library(str(path), None, 0x00001100)  # DLL_LOAD_DIR | SYSTEM32
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        if not free_library(handle):
            raise ctypes.WinError(ctypes.get_last_error())
        return

    ctypes.CDLL(str(path), mode=os.RTLD_NOW | os.RTLD_LOCAL)


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--isolated-child":
        module_path = Path(sys.argv[2]).resolve(strict=True)
        load_module(module_path)
        print(f"PASS loaded {module_path.name}")
        return 0

    if len(sys.argv) != 2:
        print("usage: package_runtime_load.py <staged-module>", file=sys.stderr)
        return 2

    module_path = Path(sys.argv[1]).resolve(strict=True)
    if sys.platform == "win32":
        windows_root = os.environ.get("SystemRoot", os.environ.get("WINDIR", r"C:\Windows"))
        environment = {
            "PATH": str(Path(windows_root) / "System32") + ";" + windows_root,
            "SystemRoot": windows_root,
            "WINDIR": windows_root,
        }
    else:
        environment = {"PATH": "/usr/bin:/bin", "LD_LIBRARY_PATH": ""}
    result = subprocess.run(
        [sys.executable, str(Path(__file__).resolve()), "--isolated-child", str(module_path)],
        cwd=module_path.parent,
        env=environment,
        check=False,
    )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
