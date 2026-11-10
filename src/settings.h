// SPDX-License-Identifier: MIT
#pragma once

#include <QPoint>
#include <QString>
#include <QtGlobal>

struct PetSettings {
    quint64 count = 0;
    int width = 355;
    bool muted = false;
    bool docked = true;
    bool hasPosition = false;
    QPoint position;
    QString screenName;
};

// One INI file in the platform's per-user config directory.
class SettingsStore {
public:
    SettingsStore();
    explicit SettingsStore(QString directory);
    QString directory() const;
    QString fileName() const;
    PetSettings load(QString* error = nullptr) const;
    bool save(const PetSettings& state, QString* error = nullptr) const;
    bool reset(QString* error = nullptr) const;

private:
    QString directory_;
};
