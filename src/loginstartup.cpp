// SPDX-License-Identifier: MIT
#include "loginstartup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#ifdef Q_OS_WIN
#include <QTemporaryFile>
#include <qt_windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace {
QString loginDirectory()
{
    const QString override = qEnvironmentVariable("BADOING_LOGIN_DIR");
    if (!override.isEmpty()) return QDir(override).absolutePath();
#ifdef Q_OS_WIN
    PWSTR path = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DONT_VERIFY, nullptr, &path);
    const QString directory = SUCCEEDED(result) ? QString::fromWCharArray(path) : QString();
    CoTaskMemFree(path);
    return directory;
#elif defined(Q_OS_MACOS)
    return QDir::home().filePath(QStringLiteral("Library/LaunchAgents"));
#else
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("autostart"));
#endif
}

#ifdef Q_OS_WIN
QString quoteArgument(const QString& argument)
{
    QString quoted = QStringLiteral("\"");
    int slashes = 0;
    for (const QChar character : argument) {
        if (character == QLatin1Char('\\')) { ++slashes; continue; }
        quoted += QString(character == QLatin1Char('"') ? slashes * 2 + 1 : slashes, QLatin1Char('\\'));
        quoted += character;
        slashes = 0;
    }
    return quoted + QString(slashes * 2, QLatin1Char('\\')) + QLatin1Char('"');
}

bool saveShortcut(const QString& path, const QString& executable, const QStringList& arguments,
                  QString* error)
{
    QTemporaryFile temporary(QDir(QFileInfo(path).absolutePath()).filePath(QStringLiteral("badoing-XXXXXX.lnk")));
    if (!temporary.open()) {
        if (error) *error = QStringLiteral("Could not save %1: %2").arg(path, temporary.errorString());
        return false;
    }
    temporary.close();
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT result = initialized;
    IShellLinkW* shortcut = nullptr;
    IPersistFile* file = nullptr;
    if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE)
        result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void**>(&shortcut));
    const QString nativeExecutable = QDir::toNativeSeparators(executable);
    const QString workingDirectory = QDir::toNativeSeparators(QFileInfo(executable).absolutePath());
    QStringList quoted;
    for (const QString& argument : arguments) quoted.append(quoteArgument(argument));
    const QString commandLine = quoted.join(QLatin1Char(' '));
    if (SUCCEEDED(result)) result = shortcut->SetPath(reinterpret_cast<LPCWSTR>(nativeExecutable.utf16()));
    if (SUCCEEDED(result)) result = shortcut->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workingDirectory.utf16()));
    if (SUCCEEDED(result)) result = shortcut->SetArguments(reinterpret_cast<LPCWSTR>(commandLine.utf16()));
    if (SUCCEEDED(result)) result = shortcut->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file));
    const QString nativePath = QDir::toNativeSeparators(path);
    const QString temporaryPath = QDir::toNativeSeparators(temporary.fileName());
    if (SUCCEEDED(result)) result = file->Save(reinterpret_cast<LPCOLESTR>(temporaryPath.utf16()), TRUE);
    if (file) file->Release();
    if (shortcut) shortcut->Release();
    if (SUCCEEDED(initialized)) CoUninitialize();
    if (SUCCEEDED(result) && !MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()),
            reinterpret_cast<LPCWSTR>(nativePath.utf16()), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        result = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(result)) return true;
    if (error) *error = QStringLiteral("Could not create the login shortcut at %1 (Windows error 0x%2).")
        .arg(path).arg(static_cast<quint32>(result), 0, 16);
    return false;
}
#elif !defined(Q_OS_MACOS)
QString quoteArgument(QString argument)
{
    argument.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    argument.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    argument.replace(QStringLiteral("`"), QStringLiteral("\\`"));
    argument.replace(QStringLiteral("$"), QStringLiteral("\\$"));
    argument.replace(QStringLiteral("%"), QStringLiteral("%%"));
    argument.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    return QLatin1Char('"') + argument + QLatin1Char('"');
}
#endif
}

QString LoginStartup::entryFile()
{
    const QString directory = loginDirectory();
    if (directory.isEmpty()) return {};
#ifdef Q_OS_WIN
    return QDir(directory).filePath(QStringLiteral("BADOING.lnk"));
#elif defined(Q_OS_MACOS)
    return QDir(directory).filePath(QStringLiteral("org.badoing.pet.plist"));
#else
    return QDir(directory).filePath(QStringLiteral("badoing-pet.desktop"));
#endif
}

bool LoginStartup::isEnabled()
{
    const QString path = entryFile();
    if (!QFileInfo(path).isFile()) return false;
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    QSettings entry(path, QSettings::IniFormat);
    return entry.status() == QSettings::NoError
        && !entry.value(QStringLiteral("Desktop Entry/Hidden"), false).toBool()
        && entry.value(QStringLiteral("Desktop Entry/X-GNOME-Autostart-enabled"), true).toBool();
#else
    return true;
#endif
}

bool LoginStartup::setEnabled(bool enabled, const QStringList& arguments, QString* error)
{
    if (error) error->clear();
    const QString path = entryFile();
    if (path.isEmpty()) {
        if (error) *error = QStringLiteral("Could not find your login startup folder.");
        return false;
    }
    if (!enabled) {
        const QFileInfo existing(path);
        if ((!existing.exists() && !existing.isSymLink()) || QFile::remove(path)) return true;
        if (error) *error = QStringLiteral("Could not remove %1. Check folder permissions.").arg(path);
        return false;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QStringLiteral("Could not create the login startup folder for %1.").arg(path);
        return false;
    }
    const QString executable = QCoreApplication::applicationFilePath();
#ifdef Q_OS_WIN
    return saveShortcut(path, executable, arguments, error);
#else
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Could not save %1: %2").arg(path, file.errorString());
        return false;
    }
#ifdef Q_OS_MACOS
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("org.badoing.pet"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    xml.writeStartElement(QStringLiteral("array"));
    xml.writeTextElement(QStringLiteral("string"), executable);
    for (const QString& argument : arguments) xml.writeTextElement(QStringLiteral("string"), argument);
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    xml.writeEmptyElement(QStringLiteral("true"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("LimitLoadToSessionType"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("Aqua"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) {
        if (error) *error = QStringLiteral("Could not write the login entry at %1.").arg(path);
        return false;
    }
#else
    QStringList values{executable};
    values.append(arguments);
    if (executable.contains(QLatin1Char('='))) {
        if (error) *error = QStringLiteral("Desktop autostart requires an executable path without an equals sign.");
        return false;
    }
    QStringList quoted;
    for (const QString& value : values) {
        if (value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')) || value.contains(QLatin1Char('\t'))) {
            if (error) *error = QStringLiteral("Login arguments cannot contain tabs or newlines.");
            return false;
        }
        quoted.append(quoteArgument(value));
    }
    const QByteArray content = (QStringLiteral("[Desktop Entry]\nType=Application\nName=BADOING\nTerminal=false\nExec=")
        + quoted.join(QLatin1Char(' ')) + QLatin1Char('\n')).toUtf8();
    if (file.write(content) != content.size()) {
        if (error) *error = QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
#endif
    if (file.commit()) return true;
    if (error) *error = QStringLiteral("Could not save %1: %2").arg(path, file.errorString());
    return false;
#endif
}
