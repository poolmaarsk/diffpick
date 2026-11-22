#include "PathUtil.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using PathUtil::Kind;

class TstPathUtil : public QObject
{
    Q_OBJECT

    private slots:
    void initTestCase();
    void normalize_data();
    void normalize();
    void classify();
    void sameItem();
};

#ifdef Q_OS_WIN
static const QString kBase = QStringLiteral("C:/base");
#else
static const QString kBase = QStringLiteral("/base");
#endif  // Q_OS_WIN

void TstPathUtil::initTestCase()
{
#ifdef Q_OS_WIN
    qputenv("DIFFPICK_TEST_DIR", "C:\\envdir");
    qunsetenv("DIFFPICK_NOT_SET");
#endif  // Q_OS_WIN
}

void TstPathUtil::normalize_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("baseDir");
    QTest::addColumn<QString>("expected");

    const auto row = [](const char *name, const QString &input, const QString &baseDir,
                        const QString &expected) {
        QTest::newRow(name) << input << baseDir << expected;
    };
    const QString home = QDir::homePath();

    row("empty", "", kBase, "");
    row("whitespace only", " \r\n ", kBase, "");
    row("trim", "  \ta.txt\r\n", kBase, kBase + "/a.txt");
    row("double quotes", "\"a b.txt\"", kBase, kBase + "/a b.txt");
    row("single quotes", "'a b.txt'", kBase, kBase + "/a b.txt");
    row("spaces inside quotes", "\" a.txt \"", kBase, kBase + "/a.txt");
    row("relative with backslashes", "dir\\sub\\a.txt", kBase, kBase + "/dir/sub/a.txt");
    row("relative without baseDir", "dir\\a.txt", "", "dir/a.txt");
    row("dot segments", "x/../y/./a.txt", kBase, kBase + "/y/a.txt");
    row("trailing separator", "dir/", kBase, kBase + "/dir");
    row("tilde", "~", kBase, home);
    row("tilde slash", "~/a.txt", kBase, home + "/a.txt");
    row("tilde backslash", "~\\a.txt", kBase, home + "/a.txt");
    row("tilde inside name", "a~/b", kBase, kBase + "/a~/b");
    row("sftp rejected", "sftp://host/x", kBase, "");
    row("smb rejected", "smb://host/share", kBase, "");

#ifdef Q_OS_WIN
    row("copy as path", "\"C:\\Users\\x\\a.txt\"", "", "C:/Users/x/a.txt");
    row("quoted forward slashes", "\"C:/Users/x/a.txt\"", "", "C:/Users/x/a.txt");
    row("file uri", "file:///C:/Users/x/a.txt", "", "C:/Users/x/a.txt");
    row("file uri percent-encoded", "file:///C:/a%20b/%C4%8D.txt", "",
        QString::fromUtf8("C:/a b/\xC4\x8D.txt"));
    row("drive root quoted", "\"D:\\\"", "", "D:/");
    row("drive root from argv", "D:\"", "", "D:/");
    row("folder from argv", "C:\\dir\"", "", "C:/dir");
    row("bare drive", "D:", "", "D:/");
    row("unc backslashes", "\\\\server\\share\\x", "", "//server/share/x");
    row("unc slashes", "//server/share/x", "", "//server/share/x");
    row("unc file uri", "file://server/share/x", "", "//server/share/x");
    row("unc trailing separator", "\\\\server\\share\\", "", "//server/share");
    row("long path prefix", "\\\\?\\C:\\x\\a.txt", "", "C:/x/a.txt");
    row("long unc prefix", "\\\\?\\UNC\\server\\share\\x", "", "//server/share/x");
    row("env var", "%DIFFPICK_TEST_DIR%\\a.txt", "", "C:/envdir/a.txt");
    row("undefined env var", "%DIFFPICK_NOT_SET%\\a.txt", "", "%DIFFPICK_NOT_SET%/a.txt");
    row("percent in name", "C:\\100% sure.txt", "", "C:/100% sure.txt");
    row("percents around undefined name", "C:\\a%DIFFPICK_NOT_SET%b.txt", "",
        "C:/a%DIFFPICK_NOT_SET%b.txt");
#else
    row("absolute", "/home/x/a.txt", kBase, "/home/x/a.txt");
    row("root", "/", kBase, "/");
    row("file uri percent-encoded", "file:///home/x/a%20b.txt", kBase, "/home/x/a b.txt");
    row("backslash is a separator", "/home/x\\a.txt", kBase, "/home/x/a.txt");
    row("tilde mixed separators", "~/proj\\src/", kBase, home + "/proj/src");
    row("percent stays literal", "/tmp/%HOME%", kBase, "/tmp/%HOME%");
    row("drive-like name is relative", "D:", kBase, kBase + "/D:");
#endif  // Q_OS_WIN
}

void TstPathUtil::normalize()
{
    QFETCH(QString, input);
    QFETCH(QString, baseDir);
    QFETCH(QString, expected);
    QCOMPARE(PathUtil::normalizePath(input, baseDir), expected);
}

void TstPathUtil::classify()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + "/sub";
    QVERIFY(QDir().mkdir(dir));
    const QString file = tmp.path() + "/a.txt";
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }

    QCOMPARE(PathUtil::classify(file), Kind::File);
    QCOMPARE(PathUtil::classify(dir), Kind::Directory);
    QCOMPARE(PathUtil::classify(tmp.path() + "/missing.txt"), Kind::Missing);
    QCOMPARE(PathUtil::classify(""), Kind::Missing);
    QCOMPARE(PathUtil::classify("a.txt"), Kind::Missing);

    // A normalized path with a trailing separator is classified by the file system.
    const QString withSeparator = QDir::toNativeSeparators(dir) + "\\";
    QCOMPARE(PathUtil::classify(PathUtil::normalizePath(withSeparator)), Kind::Directory);
}

void TstPathUtil::sameItem()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + "/a.txt";
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }

    QVERIFY(PathUtil::sameItem(file, file));
    QVERIFY(PathUtil::sameItem(file, tmp.path() + "/sub/../a.txt"));
    QVERIFY(!PathUtil::sameItem(file, tmp.path() + "/b.txt"));
    QVERIFY(PathUtil::sameItem(tmp.path() + "/missing.txt", tmp.path() + "/./missing.txt"));
#ifdef Q_OS_WIN
    QVERIFY(PathUtil::sameItem(file, tmp.path() + "/A.TXT"));
    QVERIFY(PathUtil::sameItem(file, QDir::toNativeSeparators(file)));
    QVERIFY(PathUtil::sameItem(tmp.path() + "/missing.txt", tmp.path() + "/MISSING.txt"));
#else
    QVERIFY(!PathUtil::sameItem(tmp.path() + "/missing.txt", tmp.path() + "/MISSING.txt"));
#endif  // Q_OS_WIN
}

QTEST_GUILESS_MAIN(TstPathUtil)
#include "tst_pathutil.moc"
