// SPDX-License-Identifier: MIT
#pragma once

enum class ThemeStartup { Run, Probe, Failed };

ThemeStartup prepareDesktopTheme(int argc, char* argv[]);
