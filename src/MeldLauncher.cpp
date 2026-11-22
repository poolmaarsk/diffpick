#include "MeldLauncher.hpp"

#include "Log.hpp"
#include "PathUtil.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace MeldLauncher {

namespace {

QString resolveProgram(const QString &program)
{
    const QFileInfo info(program);
    if (info.isAbsolute())
    {
        return info.isFile() ? program : QString();
    }

    return QStandardPaths::findExecutable(program);
}

}  // namespace

Command findMeld(const QString &customCommand)
{
    QStringList customArguments = QProcess::splitCommand(customCommand);
    if (!customArguments.isEmpty())
    {
        const QString program = resolveProgram(customArguments.takeFirst());
        if (!program.isEmpty())
        {
            Log::info(QStringLiteral("Meld detected from custom command: %1").arg(program));
            return {program, customArguments};
        }

        Log::error(QStringLiteral("Configured Meld command was not found"));
    }

    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("meld"));
    if (!inPath.isEmpty())
    {
        Log::info(QStringLiteral("Meld detected in PATH: %1").arg(inPath));
        return {inPath, {}};
    }

#if defined(Q_OS_WIN)
    const struct
    {
        const char *var;
        const char *suffix;
    } locations[] = {
        {"ProgramFiles", "/Meld/Meld.exe"},
        {"ProgramFiles(x86)", "/Meld/Meld.exe"},
        {"LOCALAPPDATA", "/Programs/Meld/Meld.exe"},
    };
    for (const auto &location : locations)
    {
        const QString base = qEnvironmentVariable(location.var);
        if (base.isEmpty())
        {
            continue;
        }

        const QString candidate = QDir::fromNativeSeparators(base) + QLatin1String(location.suffix);
        if (QFileInfo(candidate).isFile())
        {
            Log::info(QStringLiteral("Meld detected at: %1").arg(candidate));
            return {candidate, {}};
        }
    }
#elif defined(Q_OS_LINUX)
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    if (!flatpak.isEmpty())
    {
        QProcess flatpakInfo;
        flatpakInfo.start(flatpak, {QStringLiteral("info"), QStringLiteral("org.gnome.meld")});
        if (flatpakInfo.waitForFinished(5000) && flatpakInfo.exitStatus() == QProcess::NormalExit &&
            flatpakInfo.exitCode() == 0)
        {
            Log::info(QStringLiteral("Meld detected as Flatpak"));
            return {flatpak, {QStringLiteral("run"), QStringLiteral("org.gnome.meld")}};
        }
    }
#endif  // Q_OS_WIN

    Log::error(QStringLiteral("Meld was not found"));
    return {};
}

Validation validate(const QStringList &items)
{
    if (items.size() < 2 || items.size() > 3)
    {
        return {Problem::WrongCount, -1};
    }

    for (int i = 0; i < items.size(); ++i)
    {
        if (PathUtil::classify(items[i]) == PathUtil::Kind::Missing)
        {
            return {Problem::Missing, i};
        }

        for (int j = 0; j < i; ++j)
        {
            if (PathUtil::sameItem(items[i], items[j]))
            {
                return {Problem::Duplicate, i};
            }
        }
    }

    return {};
}

QStringList compareArgs(const QStringList &items)
{
    QStringList args;
    for (const QString &item : items)
    {
        args << QDir::toNativeSeparators(item);
    }

    return args;
}

QStringList multiTabArgs(const QString &fixed, const QStringList &picked, bool fixedOnLeft)
{
    const QString fixedArg = QDir::toNativeSeparators(fixed);
    QStringList args;
    for (const QString &item : picked)
    {
        if (PathUtil::sameItem(item, fixed))
        {
            continue;
        }

        const QString pickedArg = QDir::toNativeSeparators(item);
        args << QStringLiteral("--diff");
        if (fixedOnLeft)
        {
            args << fixedArg << pickedArg;
        }
        else
        {
            args << pickedArg << fixedArg;
        }
    }

    return args;
}

bool launch(const Command &meld, const QStringList &args)
{
    if (!meld.isValid())
    {
        return false;
    }

    const QStringList allArgs = meld.args + args;
    Log::command(meld.program, allArgs);
    const bool started = QProcess::startDetached(meld.program, allArgs);
    if (!started)
    {
        Log::error(QStringLiteral("Meld could not be started"));
    }

    return started;
}

}  // namespace MeldLauncher
