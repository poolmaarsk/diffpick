#include "PastedText.hpp"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace PastedText {

namespace {

const QLatin1String kFolderFormat("yyyyMMdd-HHmmss-zzz");

QString fileName(Slot slot)
{
    switch (slot)
    {
    case Slot::Left:
        return QStringLiteral("Text (left).txt");
    case Slot::Middle:
        return QStringLiteral("Text (middle).txt");
    case Slot::Right:
        return QStringLiteral("Text (right).txt");
    }

    return {};
}

// Time stamp of a subfolder made by save(); invalid for any other name.
QDateTime folderTime(const QString &name)
{
    const QDateTime time = QDateTime::fromString(name, kFolderFormat);
    return time.isValid() && time.toString(kFolderFormat) == name ? time : QDateTime();
}

}  // namespace

QString defaultDir()
{
#ifdef Q_OS_WIN
    return QDir::cleanPath(QDir::tempPath()) + QStringLiteral("/diffpick");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
           QStringLiteral("/diffpick");
#endif  // Q_OS_WIN
}

QString save(const QString &dir, Slot slot, const QString &text, const QDateTime &now)
{
    const QString folder = QDir::cleanPath(dir + u'/' + now.toString(kFolderFormat));
    if (!QDir().mkpath(folder))
    {
        return {};
    }

    const QString path = folder + u'/' + fileName(slot);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return {};
    }

    const QByteArray bytes = text.toUtf8();
    const bool ok = file.write(bytes) == bytes.size();
    file.close();
    return ok ? path : QString();
}

LineEnding lineEndingFor(const QString &original)
{
    if (original.contains(QLatin1String("\r\n")))
    {
        return LineEnding::Crlf;
    }

    if (original.contains(u'\n'))
    {
        return LineEnding::Lf;
    }

#ifdef Q_OS_WIN
    return LineEnding::Crlf;
#else
    return LineEnding::Lf;
#endif  // Q_OS_WIN
}

QString withLineEnding(const QString &text, LineEnding lineEnding)
{
    QString result = text;
    result.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (lineEnding == LineEnding::Crlf)
    {
        result.replace(QLatin1String("\n"), QLatin1String("\r\n"));
    }

    return result;
}

int removeOlderThan(const QString &dir, const QDateTime &cutoff)
{
    int removed = 0;
    const QDir base(dir);
    const QStringList names = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : names)
    {
        const QDateTime time = folderTime(name);
        if (time.isValid() && time < cutoff && QDir(base.filePath(name)).removeRecursively())
        {
            ++removed;
        }
    }

    return removed;
}

}  // namespace PastedText
