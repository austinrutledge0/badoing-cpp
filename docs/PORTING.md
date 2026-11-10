# Porting notes

## Design

The application uses C++17 and Qt Widgets on Linux, Windows, and macOS. Qt 6 is
preferred; Qt 5.15 remains supported for source builds. CMake selects the Qt
version. The C# application is retained as reference and is not used at runtime.

Qt Core, Gui, and Widgets are required. Qt Multimedia is optional. When found,
`QSoundEffect` plays the embedded PCM WAV through the system's Qt audio backend.
When omitted at build time, the pet keeps all visual and interaction behavior
and runs silently. A missing or unavailable audio device must not stop the pet.

## Original assets

The provided Windows executable contains two managed resources:

| Resource | Original format | Role |
| --- | --- | --- |
| `frames.png` | RGBA PNG, 4260 x 240 pixels | Ten horizontal 426 x 240 source frames. |
| `badoing.wav` | Stereo 16-bit PCM, 44,100 Hz, approximately 0.439 seconds | Click sound. |

They were recovered statically from the supplied .NET executable without
launching it. The Windows executable is not required to build or run this port.
Qt's resource compiler embeds the recovered files into the local build.

Each source frame uses the original crop `(30, 106, 318, 92)`. The geometric
silhouette comes from the supplied C# outline. The frame timing is 70, 70, 60,
70, 70, 60, 70, 70, 60, and 70 milliseconds: one 670-millisecond animation.
Artwork outside the silhouette is excluded from the pet's visible/input region
where the Qt platform supports window masks. Nearest-neighbor drawing preserves
the source's pixel texture.

The bundled assets are third-party media. The MIT license for the new C++ code
does not change their ownership or grant media redistribution rights. The
supplied C# source is a reference input, not newly MIT-licensed port code.

## X11 and XWayland

On X11, Qt can position a top-level window, read the available screen geometry,
and apply a shaped window mask. The application uses the screen's available
work area to keep the pet visible and position its base near the bottom edge.
This avoids hard-coding GNOME, KDE, XFCE, Cinnamon, or a particular panel height.

Available geometry is supplied by Qt/the window manager. A desktop that does
not report reserved panel space can produce a different result. The pet is
neither a panel applet nor a window-manager dock; it does not reserve work area.
A desktop may override its request to stay above other windows.

Wayland sessions often offer XWayland. `QT_QPA_PLATFORM=xcb` runs this same
application through Qt's X11 backend in that session. XWayland must be enabled,
`DISPLAY` must refer to it, and the matching Qt XCB plugin must be installed.
An XWayland window is still subject to the host compositor's policies.

## Native Wayland

Typical Wayland desktop shells deliberately give the compositor control over
where top-level windows appear. Qt documents that a program cannot normally set
or query a top-level window's global position with the XDG shell interface.
Therefore this port cannot promise any of the following on native Wayland:

- Restoration of an exact global desktop position.
- Snapping itself to the exact taskbar or panel edge.
- Placement above all other applications.
- Identical shaped-window/input-mask behavior across compositors.

The port requests a native interactive system move for dragging where supported.
This lets the compositor handle the operation instead of assuming that global
window coordinates can be changed. A compositor may impose its own tiling,
stacking, or decoration behavior. Use XWayland when desktop-pet placement is more
important than native Wayland operation.

No compositor-specific layer-shell extension is required or bundled. Such an
extension could improve placement on some Wayland compositors, but would not
provide a universal GNOME/KDE/wlroots solution. Desktop extensions or optional
platform backends belong in a later, separately tested feature.

## Windows and macOS

Windows builds use the GUI subsystem and Qt's Windows entry point. The pet is a
frameless tool window so it does not open a console or add a taskbar button.
The existing screen work-area logic handles the Windows taskbar through Qt's
available geometry.

macOS builds produce an app bundle with the identifier `org.badoing.pet`.
`LSUIElement` keeps it out of the Dock. Qt's tool-window type and
`WA_MacAlwaysShowToolWindow` keep the pet visible when another app is active.
Its tray menu appears in the macOS menu bar. The pet's right-click menu remains
available when a tray cannot be shown.

Both platforms use Qt's translucent background and frameless window support.
Panel snapping targets the bottom of the available work area. It does not
attach the pet to the operating system's taskbar or Dock, and does not implement
special handling for full-screen spaces or Mission Control.

Installation bundles Qt dependencies by default on these platforms when Qt 6.3
or newer is available. CPack creates a Windows ZIP or macOS disk image. Building
and deploying happen on the target operating system. Linux continues to use
distribution Qt libraries by default. Publisher signing is not configured.

## Persistence and interaction

Settings use QSettings in INI format under Qt's `GenericConfigLocation`, in a
`badoing-pet` subdirectory. On Linux this is `$XDG_CONFIG_HOME` or `~/.config`.
On Windows it is `%LOCALAPPDATA%`, and on macOS it is `~/Library/Preferences`.
The app requires this per-user configuration directory to be writable and reports a
startup error if it cannot create it. Building or installing in a read-only
application directory does not prevent normal settings saves. QSettings
synchronization uses atomic saving rather than overwriting a configuration file
in the application directory. Changes are coalesced with a 350-millisecond save
delay and flushed on normal exit. A sudden crash can lose the most recent
fraction of a second of changes. A local `instance.lock` beside the settings
file prevents multiple desktop-pet instances; it is not an autostart mechanism.

Mouse-capture cancellation is treated as the end of a drag. A later mouse move
must not continue dragging after a button was released elsewhere. A real saved
position is tracked separately from a default `(0, 0)` position so the top-left
corner remains a valid location on positioning-capable backends.

The counter and settings describe only local pet interactions. There is no
network client, account, telemetry endpoint, updater, global input hook, or use
of elevated privileges in the application. The **Start at login** menu option
and the command helpers share the native startup implementation. It creates or
removes the per-user entry without changing the pet settings file. The optional
`badoing` and `badoing.ps1` helpers use Git and CMake for source updates and
installation, and request a login entry only when instructed. They leave
the app's normal run-once behavior available. The preview command writes only
the PNG path explicitly supplied on the command line and does not create a
settings directory, start sound, or acquire the desktop-pet instance lock.

## Troubleshooting

### CMake cannot find Qt

Install development packages, not only Qt runtime libraries. Point
`CMAKE_PREFIX_PATH` at the intended Qt prefix if using a standalone Qt SDK.
Use a fresh build directory after switching between Qt 5 and Qt 6.

### Qt cannot load the XCB or Wayland platform plugin

Use the platform plugin from the same Qt installation as the linked libraries.
A plugin from a Python virtual environment or another SDK can be incompatible.
For local diagnosis, launch with:

```sh
QT_DEBUG_PLUGINS=1 ./build/badoing-pet
```

Read the reported missing library/plugin and install the corresponding package
from your distribution. Qt's XCB backend has several runtime dependencies,
including XCB cursor support on recent Qt releases; Ubuntu/Debian package that
runtime as `libxcb-cursor0`. Native Wayland may require a separate Qt Wayland
plugin package. Do not work around a plugin error by running the pet as root.

### GTK reports a missing antialiasing settings key

Linux startup checks the desktop's theme in a short-lived child process. The
check initializes Qt and a menu without reading pet settings, showing a window,
starting sound, or acquiring the instance lock. A theme crash during this check
does not abort the main application. The check disables core dumps in the child.

If desktop integration fails, startup checks Qt's built-in generic theme and
uses it for that launch. If both fail, startup reports the errors and exits.
The checks have time limits, and theme selection is checked again on the next
launch so the app can follow a changed desktop theme.

This handles the `org.gnome.settings-daemon.plugins.xsettings` missing
`antialiasing` key crash reproduced on Ubuntu 26.04 with native Wayland from a
VS Code Snap terminal. No theme argument is needed:

```sh
./badoing run
./badoing login on
```

An explicit `-platformtheme` argument or nonempty `QT_QPA_PLATFORMTHEME` takes
precedence and skips automatic selection. If you previously enabled startup
with a theme override, rerun `./badoing login on` to follow the desktop theme.
You can still force the built-in theme when desired:

```sh
./badoing run -platformtheme generic
```

Windows and macOS use Qt's normal native theme initialization.

### No sound

Check whether CMake enabled the Multimedia module, whether mute is on, and
whether your desktop has a working audio output. A sound-disabled build is an
expected fallback when Qt Multimedia was unavailable at configuration time.
After installing the matching Multimedia development package, reconfigure and
rebuild. The app does not include its own PipeWire or PulseAudio service.

### No tray icon

Tray availability is controlled by the desktop. The right-click pet menu is the
fallback. The pet does not require a GNOME tray extension to start.

### The pet does not stay above windows or restore its position

Check whether Qt selected native Wayland. This is a compositor limitation, not
a settings-file permission issue. For compatible positioning, try the XCB
backend in an XWayland session. On X11, check your window-manager rules.

## Validation

The cross-platform changes were built locally on Ubuntu 26.04 x86_64 with GCC
15.2 and Qt 5.15.18 without sound, and with Qt 6.10.2 with Multimedia. The original three
CTest suites passed in both configurations. Qt 6's audio-service initialization
required a test run outside the sandbox that blocks local audio IPC. No physical
audio playback was tested.

Integration checks cover counters, malformed settings, initial mute persistence,
saved positions, preview rendering, click and drag behavior, capture-loss
recovery, menu sizes, and mute. Each test store has an explicit temporary
directory, independent of operating-system environment variables. The default
store still uses Qt's native per-user configuration directory.

The command-line suite launches the real executable and checks preview export,
invalid option combinations, and output-path errors. The installation script
checks installed files and exports a 318 by 92 PNG through the installed
executable. On Windows and macOS it also checks bundled Qt files and removes
development Qt search paths from the child process environment.

The GitHub Actions matrix defines native Qt 6 builds with and without sound on
Linux, Windows, Apple Silicon macOS, and Intel macOS. Each job tests, installs,
verifies, and packages the app. A separate Linux job checks Qt 5.15. The workflow
must run on GitHub before those native results can be claimed.

The helper checks run the installed pet once, enable and replace a login entry,
inspect its native format, and disable it twice. A separate temporary startup
directory keeps these checks away from real login settings. On Linux, when GIO
is available, the generated desktop entry also launches a preview export with
spaces and reserved characters in the output path. `BADOING_LOGIN_DIR` provides
the startup-directory override used by these tests.

Widget checks cover the login menu's initial state, enabling, reopening the pet,
external changes, disabling without closing the pet, and error reporting when
the startup directory cannot be written. Command helpers call the app's
`--login on`, `--login off`, and `--login status` control path, which uses only
Qt Core and does not require a display. Status returns 0 when enabled and 3
when disabled; startup changes return 0 on success and 1 on failure.

Linux also runs `pet-theme-startup`, which injects fatal child-process failures
to check automatic recovery, explicit argument and environment overrides, and
clean failure when no theme initializes. The real Wayland GTK crash was checked
with automatic selection, and a healthy virtual X11 launch did not need fallback.

Windows and macOS builds, native Wayland, real window-manager behavior, monitor
scaling, and full-screen spaces were not tested in this Linux workspace.
Headless checks do not establish desktop behavior on every platform.

## Primary references

- [Qt QWindow: startSystemMove](https://doc.qt.io/qt-6/qwindow.html#startSystemMove)
- [Qt Widgets: Wayland peculiarities](https://doc.qt.io/qt-6/application-windows.html#wayland-peculiarities)
- [Qt for X11: platform-plugin dependencies](https://doc.qt.io/qt-6/linux-requirements.html)
- [Qt translucent windows](https://doc.qt.io/qt-6/qwidget.html#creating-translucent-windows)
- [Qt standard paths](https://doc.qt.io/qt-6/qstandardpaths.html)
- [Qt application deployment](https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html)
