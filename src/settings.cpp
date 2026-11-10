// SPDX-License-Identifier: MIT
#include "settings.h"
#include "petlogic.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>
#include <algorithm>
#include <utility>

SettingsStore::SettingsStore()
    : SettingsStore(QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                        .filePath(QStringLiteral("badoing-pet")))
{
}

SettingsStore::SettingsStore(QString directory)
    : directory_(std::move(directory))
{
}

QString SettingsStore::directory() const
{
    return directory_;
}

QString SettingsStore::fileName() const
{
    return QDir(directory()).filePath(QStringLiteral("settings.ini"));
}

PetSettings SettingsStore::load(QString* error) const
{
    if (error) error->clear();
    QSettings settings(fileName(), QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    PetSettings state;
    bool valid = false;
    const auto count = settings.value(QStringLiteral("pet/count"), QStringLiteral("0"))
                           .toString().toULongLong(&valid);
    if (valid) state.count = count;
    state.width = badoing::normalizedWidth(settings.value(QStringLiteral("pet/width"), 355).toInt());
    state.muted = settings.value(QStringLiteral("pet/muted"), false).toBool();
    state.docked = settings.value(QStringLiteral("window/docked"), true).toBool();
    const auto position = settings.value(QStringLiteral("window/position"));
    state.hasPosition = settings.value(QStringLiteral("window/hasPosition"), false).toBool()
                     && position.canConvert<QPoint>();
    if (state.hasPosition) {
        const auto saved = position.toPoint();
        state.position = QPoint(std::clamp(saved.x(), -1000000, 1000000),
                                std::clamp(saved.y(), -1000000, 1000000));
    }
    state.screenName = settings.value(QStringLiteral("window/screen")).toString();
    if (settings.status() != QSettings::NoError && error) {
        *error = QStringLiteral("Could not read settings from %1. Defaults may be used.")
                     .arg(fileName());
    }
    return state;
}

bool SettingsStore::save(const PetSettings& state, QString* error) const
{
    if (error) error->clear();
    if (!QDir().mkpath(directory())) {
        if (error) *error = QStringLiteral("Could not create %1.").arg(directory());
        return false;
    }
    QSettings settings(fileName(), QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setAtomicSyncRequired(true);
    // Store the full counter as decimal text, including values above signed 64-bit.
    settings.setValue(QStringLiteral("pet/count"), QString::number(state.count));
    settings.setValue(QStringLiteral("pet/width"), badoing::normalizedWidth(state.width));
    settings.setValue(QStringLiteral("pet/muted"), state.muted);
    settings.setValue(QStringLiteral("window/docked"), state.docked);
    settings.setValue(QStringLiteral("window/hasPosition"), state.hasPosition);
    settings.setValue(QStringLiteral("window/position"), state.position);
    settings.setValue(QStringLiteral("window/screen"), state.screenName);
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    if (error) *error = QStringLiteral("Could not save settings to %1. Check folder permissions and free space.")
                            .arg(fileName());
    return false;
}

bool SettingsStore::reset(QString* error) const
{
    if (error) error->clear();
    if (!QFile::exists(fileName()) || QFile::remove(fileName())) return true;
    if (error) *error = QStringLiteral("Could not remove %1.").arg(fileName());
    return false;
}
