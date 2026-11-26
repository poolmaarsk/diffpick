#pragma once

#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>

#include <array>

struct BrowseLocations
{
    static constexpr int count = 5;
    std::array<QString, count> paths;
    int activeIndex = -1;

    QString activePath() const;
};

// One comparison as entered in the three slots; middle is empty in a 2-way compare.
struct Comparison
{
    QString left;
    QString middle;
    QString right;

    bool operator==(const Comparison &other) const
    {
        return left == other.left && middle == other.middle && right == other.right;
    }
};

// Persistent settings in an INI file: %APPDATA%/diffpick/diffpick.ini on
// Windows, ~/.config/diffpick/diffpick.ini on Linux.
class Settings
{
    public:
    static constexpr int kMaxHistory = 15;

    Settings();
    explicit Settings(const QString &fileName);  // for tests

    QString fileName() const;

    QString meldCommand() const;  // empty = auto-detect
    void setMeldCommand(const QString &command);

    bool closeAfterLaunch() const;  // default true
    void setCloseAfterLaunch(bool close);

    QString lastDir() const;  // default: home folder
    void setLastDir(const QString &dir);
    BrowseLocations browseLocations() const;
    void setBrowseLocations(const BrowseLocations &locations);

    QByteArray geometry() const;
    void setGeometry(const QByteArray &geometry);

    bool diagnosticLoggingEnabled() const;  // default false
    void setDiagnosticLoggingEnabled(bool enabled);

    QList<Comparison> history() const;                // newest first
    void addToHistory(const Comparison &comparison);  // moves duplicates to the front
    void removeFromHistory(const Comparison &comparison);

    QString marked() const;
    void setMarked(const QString &path);

    QString integrationExePath() const;  // empty = system integration not installed
    void setIntegrationExePath(const QString &path);

    private:
    void writeHistory(const QList<Comparison> &list);
    mutable QSettings m_settings;
};
