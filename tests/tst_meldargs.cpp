#include "MeldLauncher.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <iterator>

using MeldLauncher::Problem;

namespace {

const char *const kEnvVars[] = {"PATH", "ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA"};

#ifdef Q_OS_WIN
const QString kExeSuffix = QStringLiteral(".exe");
#else
const QString kExeSuffix;
#endif  // Q_OS_WIN

bool createFile(const QString &path, bool executable = false)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        return false;
    }

    f.close();
    return !executable ||
           f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser);
}

QString canonical(const QString &path)
{
    return QFileInfo(path).canonicalFilePath();
}

}  // namespace

class TstMeldArgs : public QObject
{
    Q_OBJECT

    private slots:
    void init();
    void cleanup();

    void findMeldCustomCommand();
    void findMeldCustomMissingFallsBack();
    void findMeldInPath();
#ifdef Q_OS_WIN
    void findMeldWindowsLocations();
#elif defined(Q_OS_LINUX)
    void findMeldFlatpak();
#endif  // Q_OS_WIN

    void findMeldNotFound();

    void validate();
    void compareArgs();
    void multiTabArgs();

    private:
    QTemporaryDir m_tmp;
    QByteArray m_savedEnv[std::size(kEnvVars)];
    QString m_emptyDir;
};

void TstMeldArgs::init()
{
    QVERIFY(m_tmp.isValid());
    for (size_t i = 0; i < std::size(kEnvVars); ++i)
    {
        m_savedEnv[i] = qgetenv(kEnvVars[i]);
    }

    // Nothing is found unless a test puts it there.
    m_emptyDir = m_tmp.path() + "/empty";
    QVERIFY(QDir().mkpath(m_emptyDir));
    const QByteArray empty = QDir::toNativeSeparators(m_emptyDir).toLocal8Bit();
    for (const char *var : kEnvVars)
    {
        qputenv(var, empty);
    }
}

void TstMeldArgs::cleanup()
{
    for (size_t i = 0; i < std::size(kEnvVars); ++i)
    {
        qputenv(kEnvVars[i], m_savedEnv[i]);
    }
}

void TstMeldArgs::findMeldCustomCommand()
{
    const QString program = m_tmp.path() + "/my meld/meld-custom" + kExeSuffix;
    QVERIFY(createFile(program, true));

    const auto meld =
        MeldLauncher::findMeld("\"" + QDir::toNativeSeparators(program) + "\" --newtab");
    QCOMPARE(canonical(meld.program), canonical(program));
    QCOMPARE(meld.args, QStringList{"--newtab"});
}

void TstMeldArgs::findMeldCustomMissingFallsBack()
{
    const QString inPath = m_tmp.path() + "/bin/meld" + kExeSuffix;
    QVERIFY(createFile(inPath, true));
    qputenv("PATH", QDir::toNativeSeparators(m_tmp.path() + "/bin").toLocal8Bit());

    const auto meld = MeldLauncher::findMeld(m_tmp.path() + "/missing/meld" + kExeSuffix);
    QCOMPARE(canonical(meld.program), canonical(inPath));
    QVERIFY(meld.args.isEmpty());
}

void TstMeldArgs::findMeldInPath()
{
    const QString inPath = m_tmp.path() + "/bin/meld" + kExeSuffix;
    QVERIFY(createFile(inPath, true));
    qputenv("PATH", QDir::toNativeSeparators(m_tmp.path() + "/bin").toLocal8Bit());

    const auto meld = MeldLauncher::findMeld(QString());
    QCOMPARE(canonical(meld.program), canonical(inPath));
    QVERIFY(meld.args.isEmpty());
}

#ifdef Q_OS_WIN
void TstMeldArgs::findMeldWindowsLocations()
{
    const QString local = m_tmp.path() + "/local";
    const QString program = local + "/Programs/Meld/Meld.exe";
    QVERIFY(createFile(program));
    qputenv("LOCALAPPDATA", QDir::toNativeSeparators(local).toLocal8Bit());

    QCOMPARE(canonical(MeldLauncher::findMeld(QString()).program), canonical(program));

    // ProgramFiles wins over LOCALAPPDATA.
    const QString programFiles = m_tmp.path() + "/pf";
    const QString preferred = programFiles + "/Meld/Meld.exe";
    QVERIFY(createFile(preferred));
    qputenv("ProgramFiles", QDir::toNativeSeparators(programFiles).toLocal8Bit());

    QCOMPARE(canonical(MeldLauncher::findMeld(QString()).program), canonical(preferred));
}
#elif defined(Q_OS_LINUX)
void TstMeldArgs::findMeldFlatpak()
{
    QTemporaryDir flatpakDirectory;
    QVERIFY(flatpakDirectory.isValid());
    qputenv("PATH", flatpakDirectory.path().toLocal8Bit());
    const QString flatpakPath = flatpakDirectory.filePath(QStringLiteral("flatpak"));
    QFile flatpakStub(flatpakPath);
    QVERIFY(flatpakStub.open(QIODevice::WriteOnly));
    QVERIFY(flatpakStub.write("#!/bin/sh\n"
                              "test \"$1\" = info && test \"$2\" = org.gnome.meld && "
                              "test -f \"$0.installed\"\n") > 0);
    flatpakStub.close();
    QVERIFY(flatpakStub.setPermissions(flatpakStub.permissions() | QFileDevice::ExeOwner));

    QVERIFY(!MeldLauncher::findMeld(QString()).isValid());

    QVERIFY(createFile(flatpakPath + QStringLiteral(".installed")));
    const auto meld = MeldLauncher::findMeld(QString());
    QCOMPARE(meld.program, flatpakPath);
    QCOMPARE(meld.args, (QStringList{"run", "org.gnome.meld"}));
}
#endif  // Q_OS_WIN

void TstMeldArgs::findMeldNotFound()
{
    QVERIFY(!MeldLauncher::findMeld(QString()).isValid());
    QVERIFY(!MeldLauncher::findMeld(m_tmp.path() + "/missing/meld").isValid());
}

void TstMeldArgs::validate()
{
    const QString a = m_tmp.path() + "/a.txt";
    const QString b = m_tmp.path() + "/b.txt";
    const QString c = m_tmp.path() + "/c.txt";
    const QString dir = m_tmp.path() + "/dir";
    QVERIFY(createFile(a));
    QVERIFY(createFile(b));
    QVERIFY(createFile(c));
    QVERIFY(QDir().mkpath(dir));
    const QString missing = m_tmp.path() + "/missing.txt";

    const auto check = [](const QStringList &items, Problem problem, int index,
                          bool hasDuplicate = false) {
        const auto result = MeldLauncher::validate(items);
        return result.problem == problem && result.index == index &&
               result.hasDuplicate == hasDuplicate;
    };

    QVERIFY(check({a, b}, Problem::None, -1));
    QVERIFY(check({a, b, c}, Problem::None, -1));
    QVERIFY(check({a, dir}, Problem::None, -1));
    QVERIFY(check({a}, Problem::WrongCount, -1));
    QVERIFY(check({a, b, c, dir}, Problem::WrongCount, -1));
    QVERIFY(check({a, missing}, Problem::Missing, 1));
    QVERIFY(check({a, b, a}, Problem::None, -1, true));
    QVERIFY(check({a, a, missing}, Problem::Missing, 2));
#ifdef Q_OS_WIN
    QVERIFY(check({a, m_tmp.path() + "/A.TXT"}, Problem::None, -1, true));
#endif  // Q_OS_WIN
}

void TstMeldArgs::compareArgs()
{
#ifdef Q_OS_WIN
    QCOMPARE(MeldLauncher::compareArgs({"C:/x/a.txt", "C:/x/b.txt"}),
             (QStringList{"C:\\x\\a.txt", "C:\\x\\b.txt"}));
    QCOMPARE(MeldLauncher::compareArgs({"C:/l", "C:/m", "C:/r"}),
             (QStringList{"C:\\l", "C:\\m", "C:\\r"}));
#else
    QCOMPARE(MeldLauncher::compareArgs({"/x/a.txt", "/x/b.txt"}),
             (QStringList{"/x/a.txt", "/x/b.txt"}));
    QCOMPARE(MeldLauncher::compareArgs({"/l", "/m", "/r"}), (QStringList{"/l", "/m", "/r"}));
#endif  // Q_OS_WIN
}

void TstMeldArgs::multiTabArgs()
{
    const QString fixed = m_tmp.path() + "/a.txt";
    const QString x = m_tmp.path() + "/dir/x.txt";
    const QString y = m_tmp.path() + "/dir/y.txt";
    QVERIFY(createFile(fixed));
    QVERIFY(createFile(x));
    QVERIFY(createFile(y));
    const QString F = QDir::toNativeSeparators(fixed);
    const QString X = QDir::toNativeSeparators(x);
    const QString Y = QDir::toNativeSeparators(y);

    QCOMPARE(MeldLauncher::multiTabArgs(fixed, {x, y}, true),
             (QStringList{"--diff", F, X, "--diff", F, Y}));
    QCOMPARE(MeldLauncher::multiTabArgs(fixed, {x, y}, false),
             (QStringList{"--diff", X, F, "--diff", Y, F}));
    QCOMPARE(MeldLauncher::multiTabArgs(fixed, {x, fixed}, true),
             (QStringList{"--diff", F, X, "--diff", F, F}));
    QCOMPARE(MeldLauncher::multiTabArgs(fixed, {fixed}, false), (QStringList{"--diff", F, F}));
}

QTEST_GUILESS_MAIN(TstMeldArgs)
#include "tst_meldargs.moc"
