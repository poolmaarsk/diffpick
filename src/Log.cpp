#include "Log.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace Log {

namespace {

QMutex s_mutex;
QString s_path;

QString quoted(const QString &argument)
{
    if (!argument.contains(QLatin1Char(' ')) && !argument.contains(QLatin1Char('\t')) &&
        !argument.contains(QLatin1Char('"')))
    {
        return argument;
    }

    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

void append(const QString &level, const QString &message)
{
    QMutexLocker locker(&s_mutex);
    if (s_path.isEmpty())
    {
        return;
    }

    QFile file(s_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " [" << level << "] "
           << message << '\n';
}

}  // namespace

void disable()
{
    QMutexLocker locker(&s_mutex);
    s_path.clear();
}

bool initialize(const QString &directory)
{
    QMutexLocker locker(&s_mutex);
    s_path.clear();
    if (!QDir().mkpath(directory))
    {
        return false;
    }

    const QString path = QDir(directory).filePath(QStringLiteral("diffpick.log"));
    QFile file(path);
    if (file.exists() && file.size() > kMaxBytes)
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }

        file.seek(file.size() - kMaxBytes);
        QByteArray tail = file.read(kMaxBytes);
        file.close();
        const qsizetype newline = tail.indexOf('\n');
        if (newline >= 0)
        {
            tail.remove(0, newline + 1);
        }

        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            file.write(tail) != tail.size())
        {
            return false;
        }

        file.close();
    }
    else if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        return false;
    }

    file.close();
    s_path = path;
    return true;
}

QString filePath()
{
    QMutexLocker locker(&s_mutex);
    return s_path;
}

void info(const QString &message)
{
    append(QStringLiteral("INFO"), message);
}

void error(const QString &message)
{
    append(QStringLiteral("ERROR"), message);
}

void command(const QString &program, const QStringList &arguments)
{
    QStringList parts{quoted(program)};
    for (const QString &argument : arguments)
    {
        parts << quoted(argument);
    }

    append(QStringLiteral("COMMAND"), parts.join(QLatin1Char(' ')));
}

}  // namespace Log
