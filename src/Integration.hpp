#pragma once

#include <QString>

namespace Integration {

struct Result
{
    bool ok = false;
    QString message;
};

QString executablePath();
Result remove();
Result install(const QString &executable, const QString &icon = QString());

}  // namespace Integration
