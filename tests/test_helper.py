#!/usr/bin/env python3
"""Exercise run-once and login registration without changing real login settings."""

import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time


def main():
    prefix = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parent.parent
    windows = sys.platform == "win32"
    powershell = shutil.which("pwsh") or shutil.which("powershell") if windows else None
    with tempfile.TemporaryDirectory(prefix="badoing helper ") as temporary:
        folder = Path(temporary)
        login_dir = folder / "login entries"
        env = os.environ | {
            "BADOING_PREFIX": str(prefix),
            "BADOING_LOGIN_DIR": str(login_dir),
        }
        if sys.platform.startswith("linux"):
            env["QT_QPA_PLATFORM"] = "offscreen"

        def invoke(command, *args, success=True, extra_env=None):
            call_env = env | (extra_env or {})
            if windows:
                assert powershell, "PowerShell is required on Windows"
                call_env["BADOING_HELPER_SCRIPT"] = str(repo / "badoing.ps1")
                call_env["BADOING_HELPER_CALL"] = json.dumps({
                    "command": command,
                    "action": args[0] if command == "login" else None,
                    "args": args[1:] if command == "login" else args,
                })
                script = (
                    "$call = $env:BADOING_HELPER_CALL | ConvertFrom-Json; "
                    "$params = @{Command=$call.command; AppArgs=@($call.args)}; "
                    "if ($call.action) { $params.LoginAction=$call.action }; "
                    "& $env:BADOING_HELPER_SCRIPT @params"
                )
                argv = [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script]
            else:
                argv = ["bash", str(repo / "badoing"), command, *args]
            result = subprocess.run(argv, env=call_env, cwd=folder,
                                    capture_output=True, text=True, timeout=45)
            assert (result.returncode == 0) == success, result.stdout + result.stderr
            return result.stdout

        assert "Disabled" in invoke("login", "status")
        preview = folder / ('preview $ & 50%.png' if windows else 'preview $`"quote\\50%.png')
        invoke("run", "--render-preview", str(preview))
        assert preview.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
        assert not login_dir.exists(), "Running once must not create login registration"

        arguments = ["--render-preview", str(preview)]
        invoke("login", "on", *arguments,
               extra_env={"QT_QPA_PLATFORM": "not-a-real-display-backend"})
        invoke("login", "on", *arguments)
        assert "Enabled" in invoke("login", "status")
        files = list(login_dir.iterdir())
        assert len(files) == 1, f"Repeated enable must create only one entry: {files}"
        if sys.platform == "darwin":
            info = plistlib.loads(files[0].read_bytes())
            assert info["ProgramArguments"] == [str(prefix / "badoing-pet.app/Contents/MacOS/badoing-pet"), *arguments]
            assert info["RunAtLoad"] is True
            assert not info.get("KeepAlive"), "Quitting must not restart the pet"
        elif windows:
            link_env = env | {"BADOING_TEST_LINK": str(files[0])}
            result = subprocess.run([
                powershell, "-NoProfile", "-Command",
                "$s=(New-Object -ComObject WScript.Shell).CreateShortcut($env:BADOING_TEST_LINK); "
                "@{target=$s.TargetPath;arguments=$s.Arguments} | ConvertTo-Json -Compress",
            ], env=link_env, capture_output=True, text=True, check=True, timeout=15)
            shortcut = json.loads(result.stdout)
            assert Path(shortcut["target"]) == prefix / "bin/badoing-pet.exe"
            assert str(preview) in shortcut["arguments"]
        else:
            content = files[0].read_text()
            assert "Type=Application" in content and "Exec=" in content
            assert "50%%.png" in content, "Desktop field-code percent signs must be escaped"
            validator = shutil.which("desktop-file-validate")
            if validator:
                subprocess.run([validator, str(files[0])], check=True)
            launcher = shutil.which("gio")
            if launcher:
                preview.unlink()
                subprocess.run([launcher, "launch", str(files[0])], env=env,
                               check=True, timeout=15)
                for _ in range(30):
                    if preview.exists():
                        break
                    time.sleep(0.1)
                assert preview.is_file(), "The desktop entry did not launch with the expected arguments"

        before = files[0].read_bytes()
        invoke("login", "on", success=False,
               extra_env={"BADOING_PREFIX": str(folder / "not installed")})
        assert files[0].read_bytes() == before, "Failed enable must preserve existing registration"
        invoke("login", "off")
        invoke("login", "off")
        assert not list(login_dir.iterdir())
        assert "Disabled" in invoke("login", "status")
    print("Run-once and login registration checks passed.")


if __name__ == "__main__":
    main()
