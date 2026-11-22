#include "Integration.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QStringList>

#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>

#include <string>

namespace Integration {

QString executablePath()
{
    return QCoreApplication::applicationFilePath();
}

namespace {

QString quoted(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool writeSubmenu(const QString &className, const QString &argument, const QString &executable,
                  const QString &icon)
{
    QSettings menu(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\") + className +
                       QStringLiteral("\\shell\\diffpick"),
                   QSettings::NativeFormat);
    menu.clear();
    menu.setValue(QStringLiteral("MUIVerb"), QStringLiteral("Diffpick"));
    menu.setValue(QStringLiteral("SubCommands"), QString());
    if (!icon.isEmpty())
    {
        menu.setValue(QStringLiteral("Icon"), QDir::toNativeSeparators(icon));
    }

    const QString command = quoted(QDir::toNativeSeparators(executable));
    const QString item = QLatin1Char('"') + argument + QLatin1Char('"');
    menu.setValue(QStringLiteral("shell/mark/MUIVerb"),
                  QCoreApplication::translate("Integration", "Mark for compare"));
    menu.setValue(QStringLiteral("shell/mark/command/."),
                  command + QStringLiteral(" --mark ") + item);
    menu.setValue(QStringLiteral("shell/compare/MUIVerb"),
                  QCoreApplication::translate("Integration", "Compare with marked"));
    menu.setValue(QStringLiteral("shell/compare/command/."),
                  command + QStringLiteral(" --compare-with-marked ") + item);
    menu.setValue(QStringLiteral("shell/open/MUIVerb"),
                  QCoreApplication::translate("Integration", "Open in diffpick"));
    menu.setValue(QStringLiteral("shell/open/command/."),
                  command + QStringLiteral(" --open ") + item);
    menu.sync();
    return menu.status() == QSettings::NoError;
}

QString sendToShortcut(const QString &name)
{
    const QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty())
    {
        return {};
    }

    return QDir::fromNativeSeparators(appData) + QStringLiteral("/Microsoft/Windows/SendTo/") +
           name + QStringLiteral(".lnk");
}

bool createShortcut(const QString &path, const QString &executable)
{
    const HRESULT initializationResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto uninitialize = qScopeGuard([initializationResult] {
        if (SUCCEEDED(initializationResult))
        {
            CoUninitialize();
        }
    });

    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void **>(&link))))
    {
        return false;
    }

    const auto releaseLink = qScopeGuard([link] { link->Release(); });
    const std::wstring nativeExe = QDir::toNativeSeparators(executable).toStdWString();
    if (FAILED(link->SetPath(nativeExe.c_str())) || FAILED(link->SetArguments(L"--open --sort")) ||
        FAILED(link->SetDescription(L"Open selected items in diffpick")))
    {
        return false;
    }

    IPersistFile *shortcutFile = nullptr;
    if (FAILED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&shortcutFile))))
    {
        return false;
    }

    const auto releaseFile = qScopeGuard([shortcutFile] { shortcutFile->Release(); });
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    return SUCCEEDED(shortcutFile->Save(nativePath.c_str(), TRUE));
}

bool removeRegistryKey(const QString &key)
{
    const LSTATUS result = RegDeleteTreeW(HKEY_CURRENT_USER, key.toStdWString().c_str());
    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
           result == ERROR_PATH_NOT_FOUND;
}

bool removeShortcutIfPresent(const QString &path)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    if (DeleteFileW(nativePath.c_str()))
    {
        return true;
    }

    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

}  // namespace

Result remove()
{
    QStringList failedEntries;
    for (const QString &entry : {QStringLiteral("diffpick"), QStringLiteral("meldpick")})
    {
        for (const QString &className : {QStringLiteral("*"), QStringLiteral("Directory"),
                                         QStringLiteral("Directory\\Background")})
        {
            const QString key =
                QStringLiteral("Software\\Classes\\%1\\shell\\%2").arg(className, entry);
            if (!removeRegistryKey(key))
            {
                failedEntries << QStringLiteral("HKCU\\") + key;
            }
        }

        const QString shortcut = sendToShortcut(entry);
        if (shortcut.isEmpty())
        {
            const QString unavailable = QCoreApplication::translate(
                "Integration", "The Send To folder could not be located.");
            if (!failedEntries.contains(unavailable))
            {
                failedEntries << unavailable;
            }

            continue;
        }

        if (!removeShortcutIfPresent(shortcut))
        {
            failedEntries << QDir::toNativeSeparators(shortcut);
        }
    }

    if (!failedEntries.isEmpty())
    {
        return {false, QCoreApplication::translate(
                           "Integration", "System integration could not be fully removed:\n%1")
                           .arg(failedEntries.join(QLatin1Char('\n')))};
    }

    return {true, QCoreApplication::translate("Integration", "System integration was removed.")};
}

Result install(const QString &executable, const QString &icon)
{
    if (!QFile::exists(executable))
    {
        return {false, QCoreApplication::translate("Integration",
                                                   "The diffpick executable does not exist.")};
    }

    const Result cleanup = remove();
    if (!cleanup.ok)
    {
        return cleanup;
    }

    const auto failedInstall = [](const QString &message) -> Result {
        const Result rollback = remove();
        return {false, rollback.ok ? message : message + QStringLiteral("\n\n") + rollback.message};
    };

    if (!writeSubmenu(QStringLiteral("*"), QStringLiteral("%1"), executable, icon) ||
        !writeSubmenu(QStringLiteral("Directory"), QStringLiteral("%1"), executable, icon) ||
        !writeSubmenu(QStringLiteral("Directory\\Background"), QStringLiteral("%V"), executable,
                      icon))
    {
        return failedInstall(QCoreApplication::translate(
            "Integration", "The Explorer context menu could not be installed."));
    }

    const QString shortcut = sendToShortcut(QStringLiteral("diffpick"));
    if (shortcut.isEmpty() || !QDir().mkpath(QFileInfo(shortcut).absolutePath()) ||
        !createShortcut(shortcut, executable))
    {
        return failedInstall(QCoreApplication::translate(
            "Integration", "The Send To shortcut could not be created."));
    }

    return {
        true,
        QCoreApplication::translate(
            "Integration", "System integration was installed for the current user.\n\n"
                           "Registry keys:\n"
                           "HKCU\\Software\\Classes\\*\\shell\\diffpick\n"
                           "HKCU\\Software\\Classes\\Directory\\shell\\diffpick\n"
                           "HKCU\\Software\\Classes\\Directory\\Background\\shell\\diffpick\n\n"
                           "Send To shortcut:\n%1")
            .arg(QDir::toNativeSeparators(shortcut)),
    };
}

}  // namespace Integration
