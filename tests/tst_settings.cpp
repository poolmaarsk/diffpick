#include "Settings.hpp"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

class TstSettings : public QObject
{
    Q_OBJECT

    private slots:
    void defaultFileName();
    void defaults();
    void persists();
    void browseLocationsRoundTrip();
    void historyOrderAndDuplicates();
    void historyLimit();
    void historyRoundTrip();
    void historyReplacesOldEntries();
};

void TstSettings::defaultFileName()
{
    // Constructing does not write anything.
    const QString name = Settings().fileName();
    QVERIFY2(name.endsWith("/diffpick/diffpick.ini"), qPrintable(name));
}

void TstSettings::defaults()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const Settings s(tmp.path() + "/diffpick.ini");

    QVERIFY(s.meldCommand().isEmpty());
    QVERIFY(s.closeAfterLaunch());
    QCOMPARE(s.lastDir(), QDir::homePath());
    QCOMPARE(s.browseLocations().activeIndex, -1);
    QVERIFY(s.geometry().isEmpty());
    QVERIFY(!s.diagnosticLoggingEnabled());
    QVERIFY(s.history().isEmpty());
    QVERIFY(s.marked().isEmpty());
    QVERIFY(s.integrationExePath().isEmpty());
}

void TstSettings::browseLocationsRoundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString settingsPath = tmp.filePath(QStringLiteral("settings.ini"));
    BrowseLocations locations;
    locations.paths[0] = tmp.filePath(QStringLiteral("first, with spaces"));
    locations.paths[4] = tmp.filePath(QStringLiteral("last"));
    locations.activeIndex = 4;
    {
        Settings settings(settingsPath);
        settings.setBrowseLocations(locations);
    }
    const BrowseLocations restored = Settings(settingsPath).browseLocations();
    QVERIFY(restored.paths == locations.paths);
    QCOMPARE(restored.activeIndex, 4);
    QCOMPARE(restored.activePath(), locations.paths[4]);

    locations.paths[4].clear();
    Settings(settingsPath).setBrowseLocations(locations);
    const BrowseLocations afterRemoval = Settings(settingsPath).browseLocations();
    QCOMPARE(afterRemoval.activeIndex, -1);
    QCOMPARE(afterRemoval.paths[0], locations.paths[0]);
}

void TstSettings::persists()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + "/diffpick.ini";
    const QByteArray geometry("\x01\x02\x03\x00\xFF", 5);

    {
        Settings s(file);
        s.setMeldCommand("flatpak run org.gnome.meld");
        s.setCloseAfterLaunch(false);
        s.setLastDir("C:/a, b/\xC4\x8D");
        s.setGeometry(geometry);
        s.setDiagnosticLoggingEnabled(true);
        s.setMarked("D:/x/\"quoted\".txt");
        s.setIntegrationExePath("C:/Program Files/diffpick/diffpick.exe");
    }

    const Settings s(file);
    QCOMPARE(s.meldCommand(), QString("flatpak run org.gnome.meld"));
    QVERIFY(!s.closeAfterLaunch());
    QCOMPARE(s.lastDir(), QString("C:/a, b/\xC4\x8D"));
    QCOMPARE(s.geometry(), geometry);
    QVERIFY(s.diagnosticLoggingEnabled());
    QCOMPARE(s.marked(), QString("D:/x/\"quoted\".txt"));
    QCOMPARE(s.integrationExePath(), QString("C:/Program Files/diffpick/diffpick.exe"));
}

void TstSettings::historyOrderAndDuplicates()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings s(tmp.path() + "/diffpick.ini");

    const Comparison a{"a1", "", "a2"};
    const Comparison b{"b1", "b2", "b3"};
    const Comparison c{"c1", "", "c2"};
    s.addToHistory(a);
    s.addToHistory(b);
    s.addToHistory(c);
    QCOMPARE(s.history(), (QList<Comparison>{c, b, a}));

    // A duplicate moves to the front and does not appear twice.
    s.addToHistory(a);
    QCOMPARE(s.history(), (QList<Comparison>{a, c, b}));

    // Same left and right but a different middle is a different comparison.
    const Comparison a3{"a1", "mid", "a2"};
    s.addToHistory(a3);
    QCOMPARE(s.history(), (QList<Comparison>{a3, a, c, b}));
}

void TstSettings::historyLimit()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings s(tmp.path() + "/diffpick.ini");

    for (int i = 0; i < 20; ++i)
    {
        s.addToHistory({QString("l%1").arg(i), "", QString("r%1").arg(i)});
    }

    const QList<Comparison> history = s.history();
    QCOMPARE(history.size(), Settings::kMaxHistory);
    QCOMPARE(history.first().left, QString("l19"));
    QCOMPARE(history.last().left, QString("l5"));
}

void TstSettings::historyRoundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + "/diffpick.ini";
    const Comparison odd{QString::fromUtf8("C:\\a, b\\\xC4\x8D\xC5\xA5.txt"), "",
                         "'single' \"double\" ; # = [x]"};
    {
        Settings s(file);
        s.addToHistory(odd);
    }
    QCOMPARE(Settings(file).history(), (QList<Comparison>{odd}));
}

void TstSettings::historyReplacesOldEntries()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + "/diffpick.ini";
    {
        Settings s(file);
        for (int i = 0; i < 5; ++i)
        {
            s.addToHistory({QString("l%1").arg(i), "", "r"});
        }
    }
    // Moving an entry must not leave stale array entries behind after reopening.
    Settings s(file);
    s.addToHistory({"l0", "", "r"});
    const QList<Comparison> history = s.history();
    QCOMPARE(history.size(), 5);
    QCOMPARE(history.first().left, QString("l0"));
    QCOMPARE(history.last().left, QString("l1"));
}

QTEST_GUILESS_MAIN(TstSettings)
#include "tst_settings.moc"
