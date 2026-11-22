#include "Settings.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace {

const QString kMeldCommand = QStringLiteral("meld/command");
const QString kCloseAfterLaunch = QStringLiteral("ui/closeAfterLaunch");
const QString kLastDir = QStringLiteral("ui/lastDir");
const QString kBrowseLocations = QStringLiteral("ui/browseLocations");
const QString kActiveBrowseLocation = QStringLiteral("ui/activeBrowseLocation");
const QString kGeometry = QStringLiteral("ui/geometry");
const QString kDiagnosticLogging = QStringLiteral("diagnostics/logEnabled");
const QString kHistory = QStringLiteral("history");
const QString kMarked = QStringLiteral("marked");
const QString kIntegrationExePath = QStringLiteral("integration/exePath");

}  // namespace

QString BrowseLocations::activePath() const
{
    if (activeIndex < 0 || activeIndex >= count)
    {
        return {};
    }

    return paths[activeIndex];
}

Settings::Settings()
    : m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("diffpick"),
                 QStringLiteral("diffpick"))
{
    if (QFileInfo::exists(m_settings.fileName()))
    {
        return;
    }

    QSettings old(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("meldpick"),
                  QStringLiteral("meldpick"));
    if (!QFileInfo::exists(old.fileName()))
    {
        return;
    }

    for (const QString &key : old.allKeys())
    {
        m_settings.setValue(key, old.value(key));
    }

    m_settings.sync();
}

Settings::Settings(const QString &fileName)
    : m_settings(fileName, QSettings::IniFormat)
{}

QString Settings::fileName() const
{
    return m_settings.fileName();
}

QString Settings::meldCommand() const
{
    return m_settings.value(kMeldCommand).toString();
}

void Settings::setMeldCommand(const QString &command)
{
    m_settings.setValue(kMeldCommand, command);
}

bool Settings::closeAfterLaunch() const
{
    return m_settings.value(kCloseAfterLaunch, true).toBool();
}

void Settings::setCloseAfterLaunch(bool close)
{
    m_settings.setValue(kCloseAfterLaunch, close);
}

QString Settings::lastDir() const
{
    return m_settings.value(kLastDir, QDir::homePath()).toString();
}

void Settings::setLastDir(const QString &dir)
{
    m_settings.setValue(kLastDir, dir);
}

BrowseLocations Settings::browseLocations() const
{
    BrowseLocations locations;
    const QStringList savedPaths = m_settings.value(kBrowseLocations).toStringList();
    for (int index = 0; index < BrowseLocations::count && index < savedPaths.size(); ++index)
    {
        locations.paths[index] = savedPaths[index];
    }

    locations.activeIndex = m_settings.value(kActiveBrowseLocation, -1).toInt();
    if (locations.activePath().isEmpty())
    {
        locations.activeIndex = -1;
    }

    return locations;
}

void Settings::setBrowseLocations(const BrowseLocations &locations)
{
    QStringList savedPaths;
    for (const QString &path : locations.paths)
    {
        savedPaths.append(path);
    }

    m_settings.setValue(kBrowseLocations, savedPaths);
    m_settings.setValue(kActiveBrowseLocation,
                        locations.activePath().isEmpty() ? -1 : locations.activeIndex);
}

QByteArray Settings::geometry() const
{
    return m_settings.value(kGeometry).toByteArray();
}

void Settings::setGeometry(const QByteArray &geometry)
{
    m_settings.setValue(kGeometry, geometry);
}

bool Settings::diagnosticLoggingEnabled() const
{
    return m_settings.value(kDiagnosticLogging, false).toBool();
}

void Settings::setDiagnosticLoggingEnabled(bool enabled)
{
    m_settings.setValue(kDiagnosticLogging, enabled);
}

QList<Comparison> Settings::history() const
{
    QList<Comparison> list;
    const int size = m_settings.beginReadArray(kHistory);
    for (int i = 0; i < size; ++i)
    {
        m_settings.setArrayIndex(i);
        list.append({m_settings.value(QStringLiteral("left")).toString(),
                     m_settings.value(QStringLiteral("middle")).toString(),
                     m_settings.value(QStringLiteral("right")).toString()});
    }

    m_settings.endArray();
    return list;
}

void Settings::addToHistory(const Comparison &comparison)
{
    QList<Comparison> list = history();
    list.removeAll(comparison);
    list.prepend(comparison);
    while (list.size() > kMaxHistory)
    {
        list.removeLast();
    }

    m_settings.remove(kHistory);
    m_settings.beginWriteArray(kHistory, list.size());
    for (int i = 0; i < list.size(); ++i)
    {
        m_settings.setArrayIndex(i);
        m_settings.setValue(QStringLiteral("left"), list[i].left);
        m_settings.setValue(QStringLiteral("middle"), list[i].middle);
        m_settings.setValue(QStringLiteral("right"), list[i].right);
    }

    m_settings.endArray();
}

QString Settings::marked() const
{
    return m_settings.value(kMarked).toString();
}

void Settings::setMarked(const QString &path)
{
    m_settings.setValue(kMarked, path);
}

QString Settings::integrationExePath() const
{
    return m_settings.value(kIntegrationExePath).toString();
}

void Settings::setIntegrationExePath(const QString &path)
{
    m_settings.setValue(kIntegrationExePath, path);
}
