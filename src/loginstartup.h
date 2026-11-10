// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace LoginStartup {
QString entryFile();
bool isEnabled();
bool setEnabled(bool enabled, const QStringList& arguments, QString* error);
}
