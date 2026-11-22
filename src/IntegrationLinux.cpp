#include "Integration.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <cerrno>
#include <unistd.h>

namespace Integration {

QString executablePath()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty() && QFileInfo(appImage).isFile())
    {
        return QFileInfo(appImage).absoluteFilePath();
    }

    return QCoreApplication::applicationFilePath();
}

namespace {

const QStringList kScriptNames{
    QStringLiteral("Mark for compare"),
    QStringLiteral("Compare with marked"),
    QStringLiteral("Open in diffpick"),
    QStringLiteral("Compare selected"),
};

QString shellQuoted(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString desktopQuoted(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool writeExecutable(const QString &path, const QByteArray &content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit())
    {
        return false;
    }

    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                           QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                           QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                           QFileDevice::ExeOther);
}

QByteArray buildFileManagerScript(const QString &executable, const QString &arguments)
{
    const QString content =
        QStringLiteral("#!/bin/sh\n"
                       "set -f\n"
                       "items=${NAUTILUS_SCRIPT_SELECTED_FILE_PATHS:-${NEMO_SCRIPT_SELECTED_FILE_"
                       "PATHS:-${CAJA_SCRIPT_SELECTED_FILE_PATHS:-}}}\n"
                       "old_ifs=$IFS\n"
                       "IFS='\n'\n"
                       "set -- $items\n"
                       "IFS=$old_ifs\n"
                       "exec %1 %2\n")
            .arg(shellQuoted(executable), arguments);
    return content.toUtf8();
}

QStringList scriptRoots()
{
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QStringList roots;
    if (!QStandardPaths::findExecutable(QStringLiteral("nautilus")).isEmpty())
    {
        roots << data + QStringLiteral("/nautilus/scripts/Diffpick");
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("nemo")).isEmpty())
    {
        roots << data + QStringLiteral("/nemo/scripts/Diffpick");
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("caja")).isEmpty())
    {
        roots << config + QStringLiteral("/caja/scripts/Diffpick");
    }

    return roots;
}

QString dolphinFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/kio/servicemenus/diffpick.desktop");
}

QStringList allScriptRoots()
{
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return {data + QStringLiteral("/nautilus/scripts/Diffpick"),
            data + QStringLiteral("/nemo/scripts/Diffpick"),
            config + QStringLiteral("/caja/scripts/Diffpick"),
            data + QStringLiteral("/nautilus/scripts/Meld"),
            data + QStringLiteral("/nemo/scripts/Meld"),
            config + QStringLiteral("/caja/scripts/Meld")};
}

bool removeFileIfPresent(const QString &path)
{
    // unlink also removes dangling symlinks and reports inaccessible paths.
    return ::unlink(QFile::encodeName(path).constData()) == 0 || errno == ENOENT ||
           errno == ENOTDIR;
}

}  // namespace

Result remove()
{
    QStringList failedPaths;
    const auto removeEntry = [&failedPaths](const QString &path) {
        if (!removeFileIfPresent(path))
        {
            failedPaths << path;
        }
    };
    QStringList names = kScriptNames;
    names << QStringLiteral("Open in meldpick");
    for (const QString &root : allScriptRoots())
    {
        for (const QString &name : names)
        {
            removeEntry(QDir(root).filePath(name));
        }

        // Leave the directory in place if it contains unrelated files.
        QDir().rmdir(root);
    }

    removeEntry(dolphinFile());
    const QString oldDolphin =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/kio/servicemenus/meldpick.desktop");
    removeEntry(oldDolphin);
    if (!failedPaths.isEmpty())
    {
        return {false, QCoreApplication::translate(
                           "Integration", "System integration could not be fully removed:\n%1")
                           .arg(failedPaths.join(QLatin1Char('\n')))};
    }

    return {true, QCoreApplication::translate("Integration", "System integration was removed.")};
}

Result install(const QString &executable, const QString &)
{
    if (!QFileInfo(executable).isFile())
    {
        return {false, QCoreApplication::translate("Integration",
                                                   "The diffpick executable does not exist.")};
    }

    const Result cleanup = remove();
    if (!cleanup.ok)
    {
        return cleanup;
    }

    QStringList installedLocations;
    const auto failedInstall = [](const QString &message) -> Result {
        const Result rollback = remove();
        return {false, rollback.ok ? message : message + QStringLiteral("\n\n") + rollback.message};
    };
    for (const QString &root : scriptRoots())
    {
        const QList<QByteArray> scripts{
            buildFileManagerScript(executable, QStringLiteral("--mark \"${1:-}\"")),
            buildFileManagerScript(executable, QStringLiteral("--compare-with-marked \"${1:-}\"")),
            buildFileManagerScript(executable, QStringLiteral("--open \"$@\"")),
            buildFileManagerScript(executable, QStringLiteral("--compare --sort \"$@\"")),
        };
        for (int i = 0; i < kScriptNames.size(); ++i)
        {
            if (!writeExecutable(root + QLatin1Char('/') + kScriptNames[i], scripts[i]))
            {
                return failedInstall(QCoreApplication::translate(
                    "Integration", "A file-manager script could not be written."));
            }
        }

        installedLocations << root;
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("dolphin")).isEmpty())
    {
        const QString exe = desktopQuoted(executable);
        const QString desktop =
            QStringLiteral(
                "[Desktop Entry]\n"
                "Type=Service\n"
                "Icon=diffpick\n"
                "MimeType=all/all;\n"
                "X-KDE-ServiceTypes=KonqPopupMenu/Plugin\n"
                "X-KDE-Submenu=Diffpick\n"
                "Actions=mark;compareMarked;open;compare;\n\n"
                "[Desktop Action mark]\nName=Mark for compare\nExec=%1 --mark %f\n\n"
                "[Desktop Action compareMarked]\nName=Compare with marked\nExec=%1 "
                "--compare-with-marked %f\n\n"
                "[Desktop Action open]\nName=Open in diffpick\nExec=%1 --open %F\n\n"
                "[Desktop Action compare]\nName=Compare selected\nExec=%1 --compare --sort %F\n")
                .arg(exe);
        if (!writeExecutable(dolphinFile(), desktop.toUtf8()))
        {
            return failedInstall(QCoreApplication::translate(
                "Integration", "The Dolphin service menu could not be written."));
        }

        installedLocations << dolphinFile();
    }

    if (installedLocations.isEmpty())
    {
        return {false,
                QCoreApplication::translate("Integration", "No supported file manager was found.")};
    }

    return {
        true,
        QCoreApplication::translate("Integration",
                                    "System integration was installed for the current user.\n\n"
                                    "Installed locations:\n%1")
            .arg(installedLocations.join(QLatin1Char('\n'))),
    };
}

}  // namespace Integration
