// SPDX-License-Identifier: MIT
#include "petlogic.h"
#include "petwindow.h"
#include "settings.h"
#include "loginstartup.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void settleEvents()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

void sendMouse(PetWindow& pet, QEvent::Type type, const QPoint& local,
               const QPoint& global, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    // This constructor is shared by Qt 5.15 and Qt 6. No QtTest module is needed.
    QMouseEvent event(type, QPointF(local), QPointF(local), QPointF(global),
                      button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&pet, &event);
}

void click(PetWindow& pet)
{
    const QPoint local(pet.width() / 2, pet.height() / 2);
    const QPoint global = pet.mapToGlobal(local);
    sendMouse(pet, QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton);
    sendMouse(pet, QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton);
}

QAction* actionWithText(PetWindow& pet, const QString& text)
{
    for (QAction* action : pet.findChildren<QAction*>()) {
        if (action->text() == text) return action;
    }
    return nullptr;
}

void testSettings(SettingsStore& store)
{
    QString error;
    PetSettings state;
    state.count = std::numeric_limits<quint64>::max();
    state.width = 470;
    state.muted = true;
    state.docked = false;
    state.hasPosition = true;
    state.position = QPoint(0, 0);
    state.screenName = QStringLiteral("integration-screen");
    check(store.save(state, &error), "settings save succeeds in the isolated config folder");
    check(error.isEmpty(), "successful settings save reports no error");

    const PetSettings loaded = store.load(&error);
    check(error.isEmpty(), "saved settings load without an error");
    check(loaded.count == std::numeric_limits<quint64>::max(),
          "INI round trip preserves the full unsigned 64-bit counter");
    check(loaded.width == 470 && loaded.muted && !loaded.docked,
          "size, mute, and docking settings survive a round trip");
    check(loaded.hasPosition && loaded.position == QPoint(0, 0),
          "an explicitly saved origin remains a valid position");
    check(loaded.screenName == state.screenName, "the preferred monitor is retained");

    SettingsStore independent(QDir(store.directory()).filePath(QStringLiteral("independent")));
    PetSettings separate;
    separate.count = 17;
    check(independent.save(separate, &error) && store.load().count == state.count,
          "saving a second store does not change the first store");
    check(store.reset(&error) && independent.load().count == 17,
          "reset removes only the selected store's settings");

    state.width = 123456;
    check(store.save(state, &error), "invalid in-memory width can be saved safely");
    check(store.load().width == 355, "unsupported saved widths normalize to the normal size");

    {
        QSettings settings(store.fileName(), QSettings::IniFormat);
        settings.clear();
        settings.setValue(QStringLiteral("pet/count"), QStringLiteral("not-a-number"));
        settings.setValue(QStringLiteral("pet/width"), -999);
        settings.setValue(QStringLiteral("window/hasPosition"), true);
        settings.setValue(QStringLiteral("window/position"), QStringLiteral("not-a-point"));
        settings.sync();
        check(settings.status() == QSettings::NoError, "the malformed-value fixture was written");
    }
    const PetSettings invalid = store.load();
    check(invalid.count == 0 && invalid.width == 355 && !invalid.hasPosition,
          "malformed count, unsupported width, and invalid position fall back safely");
    check(!invalid.muted && invalid.docked, "missing settings retain their documented defaults");
    {
        QSettings settings(store.fileName(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("pet/count"), QStringLiteral("18446744073709551616"));
        settings.setValue(QStringLiteral("window/hasPosition"), true);
        settings.setValue(QStringLiteral("window/position"),
                          QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::min()));
        settings.sync();
    }
    const PetSettings extreme = store.load();
    check(extreme.count == 0, "a decimal counter larger than uint64 does not wrap");
    check(extreme.hasPosition && extreme.position == QPoint(1000000, -1000000),
          "extreme saved coordinates are bounded before window placement");
    check(store.reset(&error) && !QFile::exists(store.fileName()),
          "reset removes only the isolated settings file");
}

void testPreview(SettingsStore& store, const QString& temporaryRoot)
{
    const QString output = QDir(temporaryRoot).filePath(QStringLiteral("preview.png"));
    QString error;
    check(PetWindow::renderPreview(output, &error), "embedded resources render a preview successfully");
    const QImage preview(output);
    check(!preview.isNull() && preview.size() == QSize(318, 92),
          "preview PNG has the original 318 by 92 crop dimensions");
    if (!preview.isNull()) {
        check(preview.hasAlphaChannel(), "preview PNG retains an alpha channel");
        check(qAlpha(preview.pixel(0, 0)) == 0 && qAlpha(preview.pixel(317, 0)) == 0,
              "the silhouette removes the background at both upper corners");
        check(qAlpha(preview.pixel(170, 44)) > 0,
              "the silhouette preserves visible artwork inside the character");
    }
    check(!QFile::exists(store.fileName()), "rendering a preview creates no settings file");

    const QString missingParent = QDir(temporaryRoot).filePath(QStringLiteral("absent/preview.png"));
    error.clear();
    check(!PetWindow::renderPreview(missingParent, &error) && !error.isEmpty(),
          "preview reports an unusable output path instead of silently succeeding");
    check(!QFile::exists(missingParent) && !QFile::exists(store.fileName()),
          "failed preview leaves no output file or settings behind");
}

void testInitialSettings(SettingsStore& store)
{
    PetSettings state;
    state.muted = true; // Equivalent to a first launch with --mute.
    {
        PetWindow pet(store, state);
        pet.flushSettings(); // Also used by aboutToQuit, which need not close the widget.
        check(QFile::exists(store.fileName()) && store.load().muted,
              "initial mute is saved on application quit without a click or widget close");
    }
    QString error;
    check(store.reset(&error), "initial-settings check removes its saved settings");
}

void testLoginMenu(SettingsStore& store, const QString& temporaryRoot)
{
    PetSettings state;
    state.muted = true;
    PetWindow pet(store, state, {QStringLiteral("-platform"), QStringLiteral("offscreen")});
    pet.show();
    settleEvents();
    QAction* login = actionWithText(pet, QStringLiteral("Start at login"));
    check(login && login->isCheckable() && !login->isChecked(),
          "the login menu item starts unchecked when no startup entry exists");
    if (!login) return;

    login->trigger();
    check(login->isChecked() && LoginStartup::isEnabled() && QFile::exists(LoginStartup::entryFile()),
          "checking Start at login creates a native startup entry");
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    {
        QFile entry(LoginStartup::entryFile());
        check(entry.open(QIODevice::ReadOnly)
                  && entry.readAll().contains("\"-platform\" \"offscreen\""),
              "the menu retains the explicitly selected Qt display backend in its login command");
    }
#endif
    {
        PetWindow reopened(store, state);
        QAction* restored = actionWithText(reopened, QStringLiteral("Start at login"));
        check(restored && restored->isChecked(), "a reopened pet reads the existing login entry");
    }
    QString error;
    check(LoginStartup::setEnabled(false, {}, &error), "external login configuration can disable startup");
    auto* menu = qobject_cast<QMenu*>(login->parent());
    check(menu != nullptr, "the login option belongs to the shared pet and tray menu");
    if (menu) {
        menu->popup(QPoint(0, 0));
        settleEvents();
        check(!login->isChecked(), "opening the menu refreshes changes made outside the pet");
        menu->hide();
    }
    login->trigger();
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    {
        QFile entry(LoginStartup::entryFile());
        check(entry.open(QIODevice::Append) && entry.write("Hidden=true\n") > 0,
              "a desktop settings change can hide the login entry");
    }
    if (menu) {
        menu->popup(QPoint(0, 0));
        settleEvents();
        check(!login->isChecked(), "the menu honors a login entry disabled by the desktop");
        menu->hide();
        login->trigger();
    }
#endif
    login->trigger();
    check(!login->isChecked() && !LoginStartup::isEnabled() && pet.isVisible(),
          "unchecking login startup removes the entry and leaves the current pet running");

    const QByteArray originalDirectory = qgetenv("BADOING_LOGIN_DIR");
    const QString blockedPath = QDir(temporaryRoot).filePath(QStringLiteral("not-a-startup-directory"));
    QFile blocked(blockedPath);
    check(blocked.open(QIODevice::WriteOnly), "the login permission-error fixture can be created");
    blocked.close();
    qputenv("BADOING_LOGIN_DIR", QFile::encodeName(blockedPath));
    bool errorShown = false;
    QTimer::singleShot(0, [&errorShown] {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                errorShown = !message->text().isEmpty();
                message->accept();
            }
        }
    });
    login->trigger();
    check(!login->isChecked() && errorShown,
          "a failed startup change shows an error and restores the unchecked menu state");
    qputenv("BADOING_LOGIN_DIR", originalDirectory);
}

void testWindow(SettingsStore& store)
{
    PetSettings state;
    state.count = 41;
    state.muted = true; // Pointer tests do not require an audio device.
    state.docked = false;
    state.hasPosition = true;
    state.position = QPoint(0, 0);
    if (QApplication::primaryScreen()) state.screenName = QApplication::primaryScreen()->name();
    QString error;
    check(store.save(state, &error), "the interactive test starts with known saved settings");

    {
        PetWindow pet(store, store.load());
        pet.show();
        settleEvents();
        check(pet.pos() == QPoint(0, 0), "an undocked pet restores a real saved origin");
        check(pet.size() == QSize(355, 103), "normal size preserves the original aspect ratio");

        click(pet);
        pet.flushSettings();
        check(store.load().count == 42, "one complete left click increments and persists the counter once");

        QPoint local(pet.width() / 2, pet.height() / 2);
        QPoint global = pet.mapToGlobal(local);
        const QPoint beforeCancel = pet.pos();
        sendMouse(pet, QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton);
        QEvent ungrab(QEvent::UngrabMouse);
        QApplication::sendEvent(&pet, &ungrab);
        sendMouse(pet, QEvent::MouseMove, local + QPoint(90, 40), global + QPoint(90, 40),
                  Qt::NoButton, Qt::LeftButton);
        sendMouse(pet, QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton);
        pet.flushSettings();
        check(pet.pos() == beforeCancel && store.load().count == 42,
              "lost mouse capture cancels both dragging and click activation");

        sendMouse(pet, QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton);
        sendMouse(pet, QEvent::MouseMove, local + QPoint(90, 40), global + QPoint(90, 40),
                  Qt::NoButton, Qt::NoButton);
        sendMouse(pet, QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton);
        pet.flushSettings();
        check(pet.pos() == beforeCancel && store.load().count == 42,
              "mouse movement without a held button also cancels stale pointer state");

        const QPoint delta(qMax(30, QApplication::startDragDistance() + 5), 35);
        const QPoint beforeDrag = pet.pos();
        sendMouse(pet, QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton);
        sendMouse(pet, QEvent::MouseMove, local + delta, global + delta, Qt::NoButton, Qt::LeftButton);
        sendMouse(pet, QEvent::MouseButtonRelease, local, global + delta,
                  Qt::LeftButton, Qt::NoButton);
        settleEvents();
        pet.flushSettings();
        const PetSettings dragged = store.load();
        check(pet.pos() == beforeDrag + delta && dragged.position == pet.pos() && !dragged.docked,
              "a real drag moves, undocks, and saves the pet position");
        check(dragged.count == 42, "dragging does not count as a click");

        QAction* large = actionWithText(pet, QStringLiteral("Large"));
        QAction* small = actionWithText(pet, QStringLiteral("Small"));
        check(large && small, "both size choices are exposed through ordinary menu actions");
        if (large && small) {
            large->trigger();
            pet.flushSettings();
            check(pet.size() == QSize(470, 136) && store.load().width == 470,
                  "the large menu choice resizes and persists the pet");
            small->trigger();
            pet.flushSettings();
            check(pet.size() == QSize(260, 75) && store.load().width == 260
                      && small->isChecked() && !large->isChecked(),
                  "size actions remain exclusive and save the selected small size");
        }

#if BADOING_HAS_MULTIMEDIA
        QAction* mute = actionWithText(pet, QStringLiteral("Mute"));
        check(mute && mute->isChecked(), "the mute action reflects the persisted setting");
        if (mute) {
            mute->trigger();
            pet.flushSettings();
            check(!store.load().muted, "unmuting through the menu persists the changed setting");
            mute->trigger();
            pet.flushSettings();
            check(store.load().muted, "muting again persists without requiring actual audio output");
        }
#endif
    }

    state.count = std::numeric_limits<quint64>::max();
    {
        PetWindow pet(store, state);
        click(pet);
        pet.flushSettings();
        check(store.load().count == std::numeric_limits<quint64>::max(),
              "a real click at the maximum count saturates instead of wrapping the saved counter");
    }
}
}

int main(int argc, char** argv)
{
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        std::cerr << "FAIL: could not create an isolated test directory\n";
        return EXIT_FAILURE;
    }
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config"));
    qputenv("BADOING_LOGIN_DIR", QFile::encodeName(QDir(temporary.path()).filePath(QStringLiteral("login"))));
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    try {
        SettingsStore store(config);
        check(store.directory() == config,
              "all integration settings use the explicit temporary directory");
        const SettingsStore defaults;
        check(defaults.directory() == QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericConfigLocation)).filePath(QStringLiteral("badoing-pet")),
              "default settings use the platform's native per-user config location");
        testSettings(store);
        testPreview(store, temporary.path());
        testInitialSettings(store);
        testWindow(store);
        testLoginMenu(store, temporary.path());
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }
    if (failures) return EXIT_FAILURE;
    std::cout << "All integration checks passed.\n";
    return EXIT_SUCCESS;
}
