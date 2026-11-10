<p align="center">
  <img src="assets/sloth.png" alt="BADOING desktop pet" width="320">
</p>

# badoing-cpp

A desktop pet for Linux, Windows, and macOS, built with C++ and Qt Widgets.
Click for a badoing, drag to move, and right-click for settings and quit.
The artwork and audio are embedded, and the app works offline.

## Install a download

Open the [latest release](https://github.com/austinrutledge0/badoing-cpp/releases/latest) and expand **Assets**.
Choose the package for your system:

| System | Download | Install and run |
| --- | --- | --- |
| Linux x86_64, Ubuntu 24.04 | `badoing-linux-ubuntu24.04-amd64.deb` | Run `sudo apt install ./badoing-linux-ubuntu24.04-amd64.deb`, then open **BADOING** from your application menu or run `badoing-pet`. Apt installs the Qt dependencies. |
| Windows x64 | `badoing-windows-x64.zip` | Extract the ZIP and open `bin/badoing-pet.exe`. Keep the extracted files together; Qt and compiler runtimes are included. |

Downloads appear after the first successful release workflow on `master`.
See the [download guide](INSTALL.md#install-a-download) for updates and removal.
For other Linux distributions, macOS, or development, build from source.

Follow the [installation and build guide](INSTALL.md) for platform-specific
build instructions. The same source tree builds on all three operating systems.

## Use the command helper

After installing the build dependencies, use the helper from your checkout.
On Linux or macOS:

```sh
./badoing install          # Build and install for your user
./badoing run              # Run once
./badoing login on --mute  # Start muted at future logins
./badoing login off        # Stop starting at login
./badoing update           # Pull, rebuild, and reinstall
```

For installation and login startup in one command, use `./badoing install --login`.
On Windows, use `.\badoing.ps1` with the same commands, and use `-Login` instead
of `--login`. Startup is opt-in. See the
[helper guide](INSTALL.md#use-the-command-helper) for configuration and Windows examples.

## Desktop support

| Platform | Integration |
| --- | --- |
| Linux on X11 or XWayland | Transparent pet, saved position, dragging, panel-edge snapping, and an application-menu entry. |
| Linux on native Wayland | Interactive system dragging where supported. The compositor controls placement and stacking. |
| Windows | GUI executable without a console window, a tray menu, saved position, and snapping near the taskbar work-area edge. |
| macOS | App bundle with a menu-bar icon. The pet stays visible when another app is active and does not add a Dock icon. |

Builds require C++17, CMake 3.16 or newer, and Qt 6 or Qt 5.15 or newer.
Automatic Qt runtime deployment for Windows and macOS requires Qt 6.3 or newer.
Qt Multimedia is optional. If it is missing, animation and controls still work
without sound.

Window placement, transparency, and stacking depend on the desktop. On macOS,
Mission Control, full-screen spaces, and Dock placement need native desktop
checks. On native Wayland, exact saved position and panel docking are unavailable.
See the [porting notes](docs/PORTING.md) for details and validation limits.

The app follows your desktop's active theme automatically. On Linux it checks
theme initialization in an isolated process before opening the pet. If desktop
integration crashes, it uses Qt's built-in theme for that launch. You do not need
to select a theme. To choose a different one, use Qt's `-platformtheme` option
or `QT_QPA_PLATFORMTHEME`; explicit choices take precedence.

## Controls

| Action | Result |
| --- | --- |
| Left click | Increment the counter, animate the mouth, and play the sound unless muted. |
| Left drag | Move the pet. Drop near the bottom work-area edge to snap into place where positioning is supported. |
| Right click | Open the count, mute, size, login startup, positioning, and quit menu. |
| Tray or macOS menu-bar icon | Access the same menu when the desktop provides it. |

Small, normal, and large sizes preserve the artwork's proportions. If the tray
is unavailable, use the pet's right-click menu.

Check **Start at login** to start this copy of the pet when you sign in.
Uncheck it to disable future starts. The current pet keeps running. This option
uses the same per-user startup entry as `./badoing login on` and `login off`,
and refreshes its checkmark whenever you open the menu.

## Settings

The application stores count, mute, size, docking state, and supported position
information in one per-user INI file:

| Platform | Default settings file |
| --- | --- |
| Linux | `~/.config/badoing-pet/settings.ini`, or `$XDG_CONFIG_HOME/badoing-pet/settings.ini` |
| Windows | `%LOCALAPPDATA%\badoing-pet\settings.ini` |
| macOS | `~/Library/Preferences/badoing-pet/settings.ini` |

Qt selects the platform's configuration directory. An `instance.lock` beside the
settings file prevents duplicate pets using that directory. The app requires
write access to the configuration directory. It does not write settings beside
the executable. Login startup is enabled only when you check **Start at login**
or request it through the helper.

## Command-line options

Pass these options to the executable for your platform:

```text
badoing-pet --help
badoing-pet --mute
badoing-pet --reset-settings
badoing-pet --render-preview preview.png
```

On Windows, the executable is `badoing-pet.exe`. Inside a macOS bundle, it is
`badoing-pet.app/Contents/MacOS/badoing-pet`.

`--mute` starts muted and saves that preference. `--reset-settings` clears saved
settings before starting. `--render-preview` writes a transparent PNG and exits
without reading settings, starting sound, or acquiring the instance lock. It
cannot be combined with `--mute` or `--reset-settings`.

On a Wayland desktop with XWayland enabled, select Qt's XCB backend for saved
positions and panel snapping:

```sh
QT_QPA_PLATFORM=xcb ./build/badoing-pet
```

## Builds and packages

The [release workflow](.github/workflows/release.yml) runs on pushes to `master`
and can be started manually from GitHub's **Actions** tab. GitHub's Linux and
Windows runners build with sound, run tests, and verify the packaged app before
publishing both downloads and `SHA256SUMS` to a new GitHub Release. A failed
platform job prevents publication, leaving the previous release available.
See [release automation](INSTALL.md#publish-downloads-from-master) for setup.

The [build workflow](.github/workflows/build.yml) builds Qt 6 with sound enabled
and disabled on Linux, Windows, and both Apple Silicon and Intel macOS runners.
It runs logic, widget, and command-line tests, checks an installed copy, and
uploads platform packages as workflow artifacts. A separate Linux job checks
Qt 5.15 compatibility. The workflow runs after this source is pushed to GitHub.

The release workflow publishes a Linux `.deb` built against Ubuntu's Qt runtime
packages and a Windows ZIP. The build workflow also produces Linux tar archives
and macOS disk images as workflow artifacts. GitHub supplies the macOS runners,
so a local Mac is not required for those builds. Packages have no publisher
signing configured.

## Source layout

- `src/`: shared application and behavior code.
- `assets/`: embedded animation and sound recovered from the supplied Windows executable.
- `tests/`: logic, settings, interaction, resource, and command-line checks.
- `scripts/verify-install.py`: check the installed executable and its preview output.
- `badoing` and `badoing.ps1`: build, install, run, and manage login startup.
- `packaging/`: Linux desktop files and the macOS bundle manifest.
- `reference/`: the supplied Windows source and README.
- `docs/PORTING.md`: platform behavior and troubleshooting.

## Credits and licensing

This C++ port is based on [danmm1994/BADOING](https://github.com/danmm1994/BADOING).

New C++ port code is provided under the [MIT license](LICENSE). The supplied
Windows source and bundled artwork and audio retain their original owners'
rights. The supplied README credits **Sassy the Sasquatch / The Big Lez Show**
for the character artwork and audio. This is an unofficial fan port. The code
license does not grant a license to the third-party media.
