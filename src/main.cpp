// SPDX-License-Identifier: MIT
#include "petwindow.h"
#include "settings.h"
#include "themestartup.h"
#include "loginstartup.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QMenu>
#include <QString>
#include <exception>
#include <cstring>

int main(int argc, char* argv[])
{
    // Login management needs no display, theme plugin, sound, or pet settings.
    if (argc > 1 && std::strcmp(argv[1], "--login") == 0) {
        QCoreApplication core(argc, argv);
        const auto arguments = core.arguments();
        if (arguments.size() == 3 && arguments[2] == QStringLiteral("status"))
            return LoginStartup::isEnabled() ? 0 : 3;
        if (arguments.size() < 3 || (arguments[2] != QStringLiteral("on")
            && arguments[2] != QStringLiteral("off"))
            || (arguments[2] == QStringLiteral("off") && arguments.size() != 3)) {
            qCritical() << "Usage: badoing-pet --login on [app arguments...] | off | status";
            return 2;
        }
        QString error;
        if (LoginStartup::setEnabled(arguments[2] == QStringLiteral("on"), arguments.mid(3), &error))
            return 0;
        qCritical().noquote() << error;
        return 1;
    }
    QStringList loginArguments;
    for (int i = 1; i + 1 < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QStringLiteral("-platform") || argument == QStringLiteral("--platform")
            || argument == QStringLiteral("-platformtheme") || argument == QStringLiteral("--platformtheme")
            || argument == QStringLiteral("-style") || argument == QStringLiteral("--style")) {
            loginArguments << argument << QString::fromLocal8Bit(argv[++i]);
        }
    }
    const auto startup = prepareDesktopTheme(argc, argv);
    if (startup == ThemeStartup::Failed) return 1;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    if (startup == ThemeStartup::Probe) {
        QMenu menu;
        menu.ensurePolished();
        return 0;
    }
    QApplication::setApplicationName(QStringLiteral("badoing-pet"));
    QApplication::setApplicationDisplayName(QStringLiteral("BADOING"));
    QApplication::setApplicationVersion(QStringLiteral(BADOING_VERSION));
#ifdef Q_OS_LINUX
    QApplication::setDesktopFileName(QStringLiteral("badoing-pet"));
#endif
    app.setQuitOnLastWindowClosed(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("An offline desktop pet for Linux, Windows, and macOS."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption mute(QStringLiteral("mute"), QStringLiteral("Start muted (saved on exit)."));
    const QCommandLineOption reset(QStringLiteral("reset-settings"), QStringLiteral("Reset saved settings, then start."));
    const QCommandLineOption preview(QStringLiteral("render-preview"),
        QStringLiteral("Write a transparent PNG, without starting the pet or reading settings."),
        QStringLiteral("path"));
    parser.addOption(mute);
    parser.addOption(reset);
    parser.addOption(preview);
    const QCommandLineOption login(QStringLiteral("login"),
        QStringLiteral("Manage login startup; put this option first. Status exits 0 if enabled, 3 if disabled."),
        QStringLiteral("on|off|status"));
    parser.addOption(login);
    parser.process(app);
    if (parser.isSet(login)) {
        qCritical() << "Put --login before other options.";
        return 2;
    }
    if (!parser.positionalArguments().isEmpty()) parser.showHelp(2);
    if (parser.isSet(preview) && (parser.isSet(mute) || parser.isSet(reset))) {
        qCritical() << "--render-preview cannot be combined with --mute or --reset-settings.";
        return 2;
    }

    try {
        if (parser.isSet(preview)) {
            QString error;
            if (!PetWindow::renderPreview(parser.value(preview), &error)) {
                qCritical().noquote() << "Could not write preview:" << error;
                return 1;
            }
            return 0;
        }
        SettingsStore store;
        if (!QDir().mkpath(store.directory())) {
            QMessageBox::critical(nullptr, QStringLiteral("BADOING"),
                QStringLiteral("Could not create the settings directory:\n%1").arg(store.directory()));
            return 1;
        }
        QLockFile lock(QDir(store.directory()).filePath(QStringLiteral("instance.lock")));
        lock.setStaleLockTime(0); // Running for days is normal; dead-process locks are still detected.
        if (!lock.tryLock(0)) {
            if (lock.error() == QLockFile::LockFailedError) {
                qWarning() << "BADOING is already running for this config directory.";
                return 0;
            }
            QMessageBox::critical(nullptr, QStringLiteral("BADOING"),
                QStringLiteral("Could not create the single-instance lock in:\n%1\nCheck folder permissions.")
                    .arg(store.directory()));
            return 1;
        }
        QString error;
        if (parser.isSet(reset) && !store.reset(&error)) {
            QMessageBox::critical(nullptr, QStringLiteral("BADOING"), error);
            return 1;
        }
        PetSettings state = store.load(&error);
        if (!error.isEmpty()) qWarning().noquote() << error;
        if (parser.isSet(mute)) state.muted = true;
        PetWindow pet(store, state, loginArguments);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &pet, &PetWindow::flushSettings);
        pet.show();
        return app.exec();
    } catch (const std::exception& error) {
        qCritical() << "BADOING could not start:" << error.what();
        return 1;
    }
}
