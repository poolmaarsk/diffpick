#pragma once

#include <QString>
#include <QStringList>

namespace Log {

constexpr qint64 kMaxBytes = 256 * 1024;

void disable();

// Selects the log file and keeps at most its last 256 KB. Logging is a no-op
// until initialize() succeeds.
bool initialize(const QString &directory);
QString filePath();

void info(const QString &message);
void error(const QString &message);
void command(const QString &program, const QStringList &arguments);

}  // namespace Log
