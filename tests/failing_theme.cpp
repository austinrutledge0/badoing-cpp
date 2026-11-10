// SPDX-License-Identifier: MIT
// Inject a fatal failure in the isolated theme process, before Qt initializes.
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

__attribute__((constructor)) static void failThemeProbe()
{
    const char* probe = std::getenv("BADOING_INTERNAL_THEME_PROBE");
    const char* failure = std::getenv("BADOING_TEST_THEME_FAILURE");
    if (!probe || std::strcmp(probe, "1") != 0 || !failure) return;
    const char* theme = std::getenv("QT_QPA_PLATFORMTHEME");
    if (std::strcmp(failure, "all") == 0 || !theme || !*theme)
        kill(getpid(), SIGKILL);
}
