#!/usr/bin/env python3
"""Check the installed app and export a preview through its real executable."""

import argparse
import os
from pathlib import Path
import plistlib
import struct
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--bundled-qt", action="store_true")
    args = parser.parse_args()
    if args.bundled_qt and sys.platform not in ("darwin", "win32"):
        parser.error("--bundled-qt checks Windows and macOS deployment")
    prefix = args.prefix.resolve()

    if sys.platform == "darwin":
        bundle = prefix / "badoing-pet.app"
        executable = bundle / "Contents/MacOS/badoing-pet"
        with (bundle / "Contents/Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        assert info["CFBundleIdentifier"] == "org.badoing.pet"
        assert info["LSUIElement"] is True
        if args.bundled_qt:
            assert (bundle / "Contents/Frameworks").is_dir(), "Missing bundled frameworks"
            assert (bundle / "Contents/PlugIns/platforms/libqcocoa.dylib").is_file()
    elif sys.platform == "win32":
        executable = prefix / "bin/badoing-pet.exe"
        if args.bundled_qt:
            assert (prefix / "bin/Qt6Core.dll").is_file(), "Missing Qt6Core.dll"
            assert list(prefix.rglob("qwindows.dll")), "Missing Windows platform plugin"
    else:
        executable = prefix / "bin/badoing-pet"
        assert (prefix / "share/applications/badoing-pet.desktop").is_file()
        assert (prefix / "share/icons/hicolor/scalable/apps/badoing-pet.svg").is_file()

    for name in ("README.md", "INSTALL.md", "LICENSE", "docs/PORTING.md"):
        assert (prefix / "share/doc/badoing-cpp" / name).is_file(), f"Missing {name}"
    assert executable.is_file(), f"Missing executable: {executable}"

    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    if args.bundled_qt:
        env["QT_QPA_PLATFORM"] = "cocoa" if sys.platform == "darwin" else "windows"
        for name in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH",
                     "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH", "LD_LIBRARY_PATH"):
            env.pop(name, None)
        if sys.platform == "win32":
            system_root = Path(env["SystemRoot"])
            env["PATH"] = os.pathsep.join(map(str, (
                executable.parent, system_root / "System32", system_root,
            )))
        else:
            env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"

    with tempfile.TemporaryDirectory(prefix="badoing-installed-") as temporary:
        preview = Path(temporary) / "preview with spaces.png"
        subprocess.run([str(executable), "--render-preview", str(preview)],
                       env=env, check=True, timeout=30)
        data = preview.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", "Preview is not a PNG"
        assert struct.unpack(">II", data[16:24]) == (318, 92), "Wrong preview dimensions"
    print(f"Installed application passed: {executable}")


if __name__ == "__main__":
    main()
