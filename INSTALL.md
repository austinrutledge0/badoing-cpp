# Install, compile, and package badoing-cpp

## Install a download

Open the [latest release](https://github.com/austinrutledge0/badoing-cpp/releases/latest). Under **Assets**,
download your platform's package. GitHub's **Source code** archives contain source
files and require compilation. Release packages appear after the first successful
release workflow on `master`.

### Linux x86_64 on Ubuntu 24.04

Download `badoing-linux-ubuntu24.04-amd64.deb`. Open a terminal in its download
directory and run:

```sh
sudo apt install ./badoing-linux-ubuntu24.04-amd64.deb
badoing-pet
```

Apt installs the matching Qt runtime libraries and X11 and Wayland plugins.
The package also adds **BADOING** to your application menu. No compiler or Qt
development packages are required. This package targets Ubuntu 24.04 on x86_64.
Other Ubuntu releases may work if their packages satisfy the dependencies.
For other Linux distributions or architectures, use the source build instructions
below.

### Windows x64

Download `badoing-windows-x64.zip` and extract it to a permanent directory,
such as `%LOCALAPPDATA%\Programs\BADOING`. Open `bin\badoing-pet.exe` inside
that directory. Keep all extracted files together. The ZIP includes Qt DLLs,
platform and audio plugins, and the compiler runtime. You do not need to install
Qt or a compiler.

The Windows executable has no publisher signature. Windows may show an
unrecognized-app prompt. Check that you downloaded it from this repository's
release before deciding whether to run it.

### Update or remove a download

Quit the pet before updating. On Linux, download the new `.deb` and repeat
the `sudo apt install ./badoing-linux-ubuntu24.04-amd64.deb` command. On Windows,
replace the whole extracted directory with the new ZIP's contents. Keep the
same directory path to preserve any login startup entry. Updates retain your
settings and counter.

To start automatically, right-click the pet and check **Start at login**.
If you move the Windows directory, enable the option again from the new copy.
Before removing a download, uncheck **Start at login** and quit the pet.
Remove the Linux package with `sudo apt remove badoing-cpp`, or delete the
extracted Windows directory. Removal preserves your per-user settings.

Each release includes `SHA256SUMS`. To check a Linux download, compare its output
from `sha256sum badoing-linux-ubuntu24.04-amd64.deb` with that file. On Windows,
use `Get-FileHash .\badoing-windows-x64.zip -Algorithm SHA256` in PowerShell.

## Publish downloads from master

The [release workflow](.github/workflows/release.yml) uses GitHub Actions' `push`
event to build and publish packages whenever you push to `master`. GitHub hosts
the Linux and Windows build machines. No separate webhook server is required.

To enable it in [austinrutledge0/badoing-cpp](https://github.com/austinrutledge0/badoing-cpp):

1. Commit and push this source tree, including `.github/workflows/release.yml`,
   to the repository's `master` branch.
2. In [Settings > Actions > General](https://github.com/austinrutledge0/badoing-cpp/settings/actions),
   enable GitHub Actions and allow the actions used by the workflow if restricted.
3. Open [Actions > Publish Linux and Windows](https://github.com/austinrutledge0/badoing-cpp/actions/workflows/release.yml)
   to follow the build. After both platform jobs pass, open
   [Releases](https://github.com/austinrutledge0/badoing-cpp/releases) for downloads.

The publish job requests `contents: write` for GitHub's automatic `GITHUB_TOKEN`.
No personal access token or repository secret is required. Repository or
organization policies must allow that permission and the workflow's actions.
The compile jobs have read access to repository contents.

The Linux job builds with Ubuntu 24.04's Qt packages and creates a `.deb` with
runtime dependencies. It installs that package and runs the installed preview
and helper checks. The Windows job builds with MSVC and Qt 6.8.3, bundles Qt and
compiler runtimes, and runs the same checks on the extracted ZIP. Its preview
check removes Qt development paths from the child process environment.
Both jobs run CTest before packaging.

Each successful run creates a release tagged `master-RUN_NUMBER-RUN_ATTEMPT`
at the exact built commit, marks it **Latest**, and includes both packages,
checksums, and a link to the build log. Failed builds do not publish a release.
A newer run on the same branch cancels an unfinished run, and the publish step
skips older commits if `master` has moved. Previous releases remain available.

To rebuild without a push, select **Actions > Publish Linux and Windows > Run
workflow**, then select `master`. Manual runs on other branches skip the jobs.
Pull requests and tag pushes do not trigger this workflow. The separate
[build workflow](.github/workflows/build.yml) continues to check Linux, Windows,
and macOS; its macOS packages remain workflow artifacts. GitHub hosts those
macOS runners too, so you do not need your own Mac.

## Build from source

Run commands from the directory containing `CMakeLists.txt`. Build each release
on its target operating system with a matching compiler and Qt installation.
The instructions below use Qt 6. CMake also supports Qt 5.15 for source builds.

## Use the command helper

Install your platform's dependencies from the sections below first. Clone
[austinrutledge0/badoing-cpp](https://github.com/austinrutledge0/badoing-cpp)
with Git, then enter the checkout:

```sh
git clone --branch master https://github.com/austinrutledge0/badoing-cpp.git
cd badoing-cpp
```

On Linux or macOS, choose manual use or login startup:

For a Homebrew Qt installation on macOS, first run
`export CMAKE_PREFIX_PATH="$(brew --prefix qt)"` in your terminal.

```sh
./badoing install                 # Configure, build, and install; no startup change
./badoing run                     # Run the installed pet once
./badoing install --login         # Build, install, and start at future logins
./badoing login on --mute         # Enable or replace the login entry and its arguments
./badoing login status
./badoing login off
./badoing test                    # Build and run CTest
./badoing pull                    # Only pull the current Git branch, with --ff-only
./badoing update                  # Pull, build, and install; preserve startup choice
```

On Windows, open PowerShell in the checkout. Add your Qt kit to the environment
once per terminal, using your installed Qt path:

```powershell
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.8.3/msvc2022_64"
$env:PATH = "$env:CMAKE_PREFIX_PATH/bin;$env:PATH"
.\badoing.ps1 install
.\badoing.ps1 run
.\badoing.ps1 install -Login
.\badoing.ps1 login on -AppArgs @('--mute')
.\badoing.ps1 login status
.\badoing.ps1 login off
.\badoing.ps1 update
```

If PowerShell blocks a downloaded script, review it and use
`Unblock-File .\badoing.ps1` to remove its downloaded-file mark. Your organization's
PowerShell policy may still apply.

Neither helper installs compiler or Qt dependencies, asks for administrator
privileges, or changes Git branches. Updating uses `git pull --ff-only` and stops
if Git cannot update the current branch. Run updates from a checkout, not a
downloaded ZIP. Quit the pet before reinstalling, especially on Windows where a
running executable can prevent replacement.

### Configure the helper

The defaults install under `~/.local` on Linux, `~/Applications/BADOING` on macOS,
and `%LOCALAPPDATA%\Programs\BADOING` on Windows. The build directory is `build`
inside the checkout, and sound is enabled when Qt Multimedia is available.

Set these environment variables to change the defaults. Keep the same prefix
for later `run` and `login` commands:

| Variable | Purpose |
| --- | --- |
| `BADOING_PREFIX` | Installation directory, preferably an absolute path. |
| `BADOING_BUILD_DIR` | Build directory. Use a fresh directory when changing Qt or compilers. |
| `BADOING_SOUND` | `ON` or `OFF` at build time. |
| `CMAKE_PREFIX_PATH` | Location of your Qt SDK. |

For example, on Linux or macOS:

```sh
export BADOING_PREFIX="$HOME/apps/badoing"
export BADOING_BUILD_DIR="$PWD/build-silent"
export BADOING_SOUND=OFF
./badoing install
./badoing run
```

PowerShell also accepts `-Prefix`, `-BuildDir`, and `-Sound` directly:

```powershell
.\badoing.ps1 install -Prefix "$HOME/Apps/BADOING" -Sound OFF
.\badoing.ps1 run -Prefix "$HOME/Apps/BADOING" -AppArgs @('--mute')
```

Pass application arguments after `run` or `login on` in Bash, or with `-AppArgs`
in PowerShell. On a Linux Wayland desktop with XWayland enabled:

```sh
./badoing run -platform xcb
./badoing login on -platform xcb --mute
```

Theme selection is automatic. Run `./badoing run` or enable login startup without
theme arguments to follow the desktop theme. On Linux, a failed native theme
initialization falls back to Qt's built-in theme automatically. Set
`-platformtheme` or `QT_QPA_PLATFORMTHEME` only when you want an explicit override.

### How login startup works

You can also right-click the pet and check or uncheck **Start at login**.
The menu and command helpers use the same startup entry. The checkmark refreshes
when you open the menu, so command-line changes appear there too. A failed
change displays an error and restores the checkmark to the actual state.

The helpers ask the installed app to register its absolute path and the
arguments you supply. The menu registers the currently running copy and retains
explicit Qt platform, theme, and style arguments. Automatic theme fallback is
not saved as an override. Install the pet in a permanent location before
enabling startup.

Enabling startup does not start another pet immediately. Use `run` to start it now. `run`
leaves your startup choice unchanged; use `login off` to disable an earlier entry.

| Platform | Per-user login entry |
| --- | --- |
| Linux | `$XDG_CONFIG_HOME/autostart/badoing-pet.desktop`, or `~/.config/autostart/badoing-pet.desktop`. |
| Windows | `BADOING.lnk` in your user's Startup folder. |
| macOS | `~/Library/LaunchAgents/org.badoing.pet.plist`. |

These start the pet inside your desktop session after login. No system-wide
service is installed. Repeating `login on` replaces the same entry; repeating
`login off` is safe. `login status` checks the startup entry. Your
desktop's startup settings can override it.

Disabling startup leaves the current pet running. Quit through its menu. On
macOS the LaunchAgent has no restart policy, so quitting the pet lasts until the
next login. Run `login on` again after moving the installed application, and run
`login off` before removing it.

The mechanisms follow the [desktop autostart specification](https://specifications.freedesktop.org/autostart/latest/),
[Windows Startup folder guidance](https://support.microsoft.com/en-us/windows/experience/startup-boot/configure-startup-applications-in-windows),
and [Apple LaunchAgent guidance](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingLaunchdJobs.html).

## Build on Linux

Install a C++17 compiler, CMake 3.16 or newer, Ninja, and Qt development packages.
On Debian, Ubuntu, or Linux Mint:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-multimedia-dev
```

Other distribution commands are:

```sh
# Fedora
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtmultimedia-devel

# Arch or EndeavourOS
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-multimedia

# openSUSE
sudo zypper install gcc-c++ cmake ninja qt6-base-devel 'cmake(Qt6Multimedia)'

# Alpine, with its community repository enabled
sudo apk add build-base cmake ninja qt6-qtbase-dev qt6-qtmultimedia-dev
```

Install the matching Qt Wayland platform plugin for native Wayland, such as
`qt6-wayland` on Ubuntu or Arch. XWayland uses Qt's XCB plugin and its runtime
dependencies, including `libxcb-cursor0` on recent Ubuntu releases.

Compile, test, and start the pet:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
(cd build && ctest --output-on-failure)
./build/badoing-pet
```

### Install for your user

```sh
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/bin/badoing-pet"
```

The executable goes in `bin`, desktop integration files go in `share/applications`
and `share/icons`, and documentation goes in `share/doc/badoing-cpp` beneath the
prefix. Keep the matching Qt runtime packages installed.

The application-menu entry looks for `badoing-pet` in your desktop session's
`PATH`. If your session reads `~/.profile`, add this line there, then sign out
and back in:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

For a system-wide installation:

```sh
sudo cmake --install build --prefix /usr/local
```

Run the pet as your regular user.

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, CMake, and a
Qt 6 desktop kit for **MSVC 2022 64-bit**. Include Qt Multimedia for sound. The
Qt kit must match the compiler and architecture. Use Qt 6.3 or newer to bundle
runtime dependencies automatically.

Open PowerShell in the source directory. Set `qtPrefix` to your installed Qt kit:

```powershell
$qtPrefix = "C:/Qt/6.8.3/msvc2022_64"
$env:PATH = "$qtPrefix/bin;$env:PATH"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$qtPrefix"
cmake --build build --config Release --parallel
Push-Location build
ctest -C Release --output-on-failure
Pop-Location
& ./build/Release/badoing-pet.exe
```

Install a distributable directory:

```powershell
cmake --install build --config Release --prefix "$PWD/dist"
& ./dist/bin/badoing-pet.exe
```

CMake runs Qt's deployment tool to copy Qt DLLs and plugins into the installation.
MSVC builds also copy the compiler runtime DLLs next to the executable, so the
ZIP works without a separate Visual C++ Redistributable installation. Keep the
entire `dist` directory together.

Create a ZIP package:

```powershell
cmake --build build --config Release --target package
```

The archive appears under `build/packages`. Extract it before opening
`bin/badoing-pet.exe`. This build uses the shared Qt implementation rather than
the C# reference application.

## Build on macOS

Install the Xcode command-line tools if needed:

```sh
xcode-select --install
```

With Homebrew installed, install the build dependencies:

```sh
brew install cmake ninja qt
```

Compile and test with Apple's Clang compiler:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
(cd build && ctest --output-on-failure)
open ./build/badoing-pet.app
```

Install the bundle with its Qt frameworks and plugins:

```sh
cmake --install build --prefix "$PWD/dist"
open ./dist/badoing-pet.app
```

Copy the deployed `dist/badoing-pet.app` into `~/Applications` or `/Applications`.
Use the deployed bundle when moving to another machine. The build-directory
bundle can still depend on your development Qt installation.

Create a disk image:

```sh
cmake --build build --target package
```

The `.dmg` appears under `build/packages`. The application uses a menu-bar icon
and has no Dock icon. Quit through the pet's right-click menu or menu-bar menu.
The bundle uses local signing from Qt's deployment tool; publisher signing and
notarization require your Apple signing identity and are not configured here.
See [Qt's macOS deployment guide](https://doc.qt.io/qt-6/macos-deployment.html)
for release signing.

Build on Apple Silicon for an ARM64 application or on Intel for an x86_64
application. A universal binary requires a Qt installation with both
architectures and `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` at configuration.

## Change build options

Use a fresh build directory when changing the Qt major version, architecture,
or compiler. For a Qt SDK outside the normal search paths, pass
`-DCMAKE_PREFIX_PATH=/path/to/Qt/kit` when configuring.

To build without sound, add `-DBADOING_ENABLE_SOUND=OFF`. If Multimedia is
missing, CMake continues without sound and reports `sound=0`.

To select Qt 5.15 when Qt 6 is also installed:

```sh
cmake -S . -B build-qt5 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
cmake --build build-qt5 --parallel
(cd build-qt5 && ctest --output-on-failure)
```

On Debian or Ubuntu, Qt 5 development packages are `qtbase5-dev` and
`qtmultimedia5-dev`. Qt 5 builds and Qt 6 versions before 6.3 install the app
without automatically bundling Qt. Deploy these with the matching
`windeployqt` or `macdeployqt` tool before distributing them.

`BADOING_DEPLOY_QT` defaults to `ON` for supported Windows and macOS Qt 6 builds
and `OFF` on Linux. Set it to `OFF` when packaging dependencies yourself.
`BADOING_BUILD_TESTS=OFF` omits test executables. Tests are not installed.

## Verify an installation

The CTest suites check logic, real widgets and settings in temporary
folders, and command-line preview export. They use the offscreen Qt plugin.

For Linux, run the installed preview check with Python 3:

```sh
python3 scripts/verify-install.py "$HOME/.local"
```

For a Windows or macOS installation with bundled Qt:

```text
python scripts/verify-install.py dist --bundled-qt
```

The bundled check removes Qt development paths from its child process's
environment and launches the installed executable using the native platform
plugin. Run it in a desktop session. It writes only a temporary preview PNG.
Check dragging, sound, scaling, and window stacking manually on each desktop.

The GitHub Actions workflow runs these checks and uploads the platform packages.

## Remove an installation

If you enabled login startup, disable it first with `./badoing login off` on
Linux or macOS, or `.\badoing.ps1 login off` on Windows. Use the same prefix
configuration as your installation.

Quit the pet. On Windows, remove the extracted application directory. On macOS,
remove the installed `badoing-pet.app` bundle.

For the default Linux user installation:

```sh
rm -f -- "$HOME/.local/bin/badoing-pet" \
    "$HOME/.local/share/applications/badoing-pet.desktop" \
    "$HOME/.local/share/icons/hicolor/scalable/apps/badoing-pet.svg" \
    "$HOME/.local/share/doc/badoing-cpp/README.md" \
    "$HOME/.local/share/doc/badoing-cpp/INSTALL.md" \
    "$HOME/.local/share/doc/badoing-cpp/LICENSE" \
    "$HOME/.local/share/doc/badoing-cpp/docs/PORTING.md"
```

For custom prefixes, `build/install_manifest.txt` records installed files.
Removal preserves your settings. Find their location in the
[settings reference](README.md#settings). Start the app with `--reset-settings`
to reset your count and preferences.
