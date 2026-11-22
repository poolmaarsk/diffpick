#include "PathUtil.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace PathUtil {

namespace {

// Strips one pair of surrounding quotes, or a single unpaired trailing '"':
// Windows turns "D:\" into D:" when it splits a command line.
QString stripQuotes(QString s)
{
    if (s.size() >= 2)
    {
        const QChar first = s.front();
        if ((first == u'"' || first == u'\'') && s.back() == first)
        {
            return s.mid(1, s.size() - 2).trimmed();
        }
    }

    if (s.endsWith(u'"') && s.count(u'"') == 1)
    {
        s.chop(1);
    }

    return s.trimmed();
}

#ifdef Q_OS_WIN
// Expands %VAR% for defined variables; anything else stays literal.
QString expandEnvVars(const QString &s)
{
    static const QRegularExpression var(QStringLiteral("%([^%/]+)%"));
    QString out;
    qsizetype last = 0;
    auto it = var.globalMatch(s);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const QByteArray name = m.captured(1).toLocal8Bit();
        if (!qEnvironmentVariableIsSet(name.constData()))
        {
            continue;
        }

        out += s.mid(last, m.capturedStart() - last);
        out += QDir::fromNativeSeparators(qEnvironmentVariable(name.constData()));
        last = m.capturedEnd();
    }

    return out + s.mid(last);
}
#endif  // Q_OS_WIN

}  // namespace

QString normalizePath(const QString &input, const QString &baseDir)
{
    // 1. + 2. Whitespace and quotes.
    QString s = stripQuotes(input.trimmed());
    if (s.isEmpty())
    {
        return {};
    }

#ifdef Q_OS_WIN
    if (s.size() == 2 && s[0].isLetter() && s[1] == u':')
    {
        s += u'/';
    }
#endif  // Q_OS_WIN

    // 3. URIs.
    if (s.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
    {
        s = QUrl(s).toLocalFile();
        if (s.isEmpty())
        {
            return {};
        }
    }
    else
    {
        static const QRegularExpression otherScheme(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]+://"));
        if (otherScheme.match(s).hasMatch())
        {
            return {};
        }
    }

    // 4. Separators.
#ifdef Q_OS_WIN
    if (s.startsWith(QLatin1String("\\\\?\\UNC\\"), Qt::CaseInsensitive))
    {
        s = QStringLiteral("\\\\") + s.mid(8);
    }
    else if (s.startsWith(QLatin1String("\\\\?\\")))
    {
        s = s.mid(4);
    }
#endif  // Q_OS_WIN

    s.replace(u'\\', u'/');

    // 5. Home folder and environment variables.
    if (s == u'~' || s.startsWith(QLatin1String("~/")))
    {
        s = QDir::homePath() + s.mid(1);
    }

#ifdef Q_OS_WIN
    s = expandEnvVars(s);
#endif  // Q_OS_WIN

    // 6. Relative paths.
    if (QDir::isRelativePath(s) && !baseDir.isEmpty())
    {
        s = QDir::fromNativeSeparators(baseDir) + u'/' + s;
    }

    // 7. Clean up; this also drops a trailing separator except on a root.
    return QDir::cleanPath(s);
}

Kind classify(const QString &path)
{
    if (path.isEmpty() || QDir::isRelativePath(path))
    {
        return Kind::Missing;
    }

    const QFileInfo info(path);
    if (info.isDir())
    {
        return Kind::Directory;
    }

    if (info.exists())
    {
        return Kind::File;
    }

    return Kind::Missing;
}

bool sameItem(const QString &a, const QString &b)
{
    const auto key = [](const QString &path) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
    };
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif  // Q_OS_WIN

    return key(a).compare(key(b), cs) == 0;
}

}  // namespace PathUtil
