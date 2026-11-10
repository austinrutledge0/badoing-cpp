// SPDX-License-Identifier: MIT
#include "themestartup.h"

#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <sys/resource.h>

namespace {
constexpr auto probeVariable = "BADOING_INTERNAL_THEME_PROBE";

bool probeTheme(const QStringList& arguments, QProcessEnvironment environment, QString* error)
{
    environment.insert(QString::fromLatin1(probeVariable), QStringLiteral("1"));
    QProcess probe;
    probe.setProcessEnvironment(environment);
    probe.start(QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget(), arguments);
    const bool finished = probe.waitForStarted(2000) && probe.waitForFinished(5000);
    if (!finished) {
        probe.kill();
        probe.waitForFinished(1000);
    }
    if (finished && probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0)
        return true;
    *error = QString::fromLocal8Bit(probe.readAllStandardError()).trimmed();
    if (error->isEmpty()) *error = probe.errorString();
    return false;
}
}
#endif

ThemeStartup prepareDesktopTheme(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    if (qgetenv(probeVariable) == "1") {
        // A broken native theme must not leave a core dump at every login.
        const rlimit noCore{0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        return ThemeStartup::Probe;
    }

    QStringList arguments;
    bool explicitTheme = !qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME");
    for (int i = 1; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        arguments.append(argument);
        if (argument == QStringLiteral("-platformtheme") || argument == QStringLiteral("--platformtheme"))
            explicitTheme = true;
    }
    if (explicitTheme) return ThemeStartup::Run;

    auto environment = QProcessEnvironment::systemEnvironment();
    QString nativeError;
    if (probeTheme(arguments, environment, &nativeError)) return ThemeStartup::Run;

    environment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"), QStringLiteral("generic"));
    QString fallbackError;
    if (!probeTheme(arguments, environment, &fallbackError)) {
        qCritical().noquote() << "BADOING could not initialize the desktop:" << nativeError
                             << "\nBuilt-in theme also failed:" << fallbackError;
        return ThemeStartup::Failed;
    }
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    qWarning() << "BADOING: desktop theme initialization failed; using Qt's built-in theme for this launch.";
#else
    Q_UNUSED(argc);
    Q_UNUSED(argv);
#endif
    return ThemeStartup::Run;
}
