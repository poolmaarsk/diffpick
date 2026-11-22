#include "PastedText.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using PastedText::LineEnding;
using PastedText::Slot;

namespace {

const QDateTime kNow(QDate(2026, 11, 17), QTime(9, 30, 45, 123));

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<unreadable>");
}

QStringList subfolders(const QString &dir)
{
    return QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

}  // namespace

class TstPastedText : public QObject
{
    Q_OBJECT

    private slots:
    void defaultDir();
    void saveWritesFile();
    void saveSlotNames();
    void saveKeepsBytesExactly();
    void saveFailsWhenDirIsAFile();
    void lineEndingFor_data();
    void lineEndingFor();
    void withLineEnding();
    void removeOlderThan();
    void removeOlderThanMissingDir();
};

void TstPastedText::defaultDir()
{
    const QString dir = PastedText::defaultDir();
    QVERIFY(dir.endsWith("/diffpick"));
    QVERIFY(!QDir::isRelativePath(dir));
    QVERIFY(!dir.startsWith("/tmp/"));
}

void TstPastedText::saveWritesFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + "/nested/diffpick";

    const QString path = PastedText::save(dir, Slot::Left, "hello", kNow);
    QCOMPARE(path, dir + "/20261117-093045-123/Text (left).txt");
    QCOMPARE(readAll(path), QByteArray("hello"));
}

void TstPastedText::saveSlotNames()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString left = PastedText::save(tmp.path(), Slot::Left, "l", kNow);
    const QString middle = PastedText::save(tmp.path(), Slot::Middle, "m", kNow);
    const QString right = PastedText::save(tmp.path(), Slot::Right, "r", kNow);
    const QString folder = tmp.path() + "/20261117-093045-123";

    QCOMPARE(left, folder + "/Text (left).txt");
    QCOMPARE(middle, folder + "/Text (middle).txt");
    QCOMPARE(right, folder + "/Text (right).txt");
    // Saved in the same millisecond, so they share a folder and keep their own content.
    QCOMPARE(readAll(left), QByteArray("l"));
    QCOMPARE(readAll(middle), QByteArray("m"));
    QCOMPARE(readAll(right), QByteArray("r"));
}

void TstPastedText::saveKeepsBytesExactly()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString text = QString::fromUtf8("\xC4\x8D\xC5\xA5\xC5\xBE x\r\ny\nz\r\n");
    const QString path = PastedText::save(tmp.path(), Slot::Right, text, kNow);

    const QByteArray bytes = readAll(path);
    QCOMPARE(bytes, QByteArray("\xC4\x8D\xC5\xA5\xC5\xBE x\r\ny\nz\r\n"));
    QVERIFY(!bytes.startsWith("\xEF\xBB\xBF"));  // no BOM

    const QString empty = PastedText::save(tmp.path(), Slot::Left, "", kNow.addSecs(1));
    QVERIFY(!empty.isEmpty());
    QCOMPARE(readAll(empty), QByteArray());
}

void TstPastedText::saveFailsWhenDirIsAFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString blocker = tmp.path() + "/blocker";
    {
        QFile f(blocker);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }
    QVERIFY(PastedText::save(blocker, Slot::Left, "x", kNow).isEmpty());
}

void TstPastedText::lineEndingFor_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<bool>("crlf");

#ifdef Q_OS_WIN
    const bool native = true;
#else
    const bool native = false;
#endif  // Q_OS_WIN

    QTest::newRow("crlf") << QString("a\r\nb") << true;
    QTest::newRow("lf") << QString("a\nb") << false;
    QTest::newRow("mixed, crlf wins") << QString("a\nb\r\nc") << true;
    QTest::newRow("empty uses OS") << QString() << native;
    QTest::newRow("no line break uses OS") << QString("abc") << native;
    QTest::newRow("lone cr uses OS") << QString("a\rb") << native;
}

void TstPastedText::lineEndingFor()
{
    QFETCH(QString, original);
    QFETCH(bool, crlf);
    QCOMPARE(PastedText::lineEndingFor(original), crlf ? LineEnding::Crlf : LineEnding::Lf);
}

void TstPastedText::withLineEnding()
{
    const QString mixed = "a\r\nb\nc\r\n\nd";
    QCOMPARE(PastedText::withLineEnding(mixed, LineEnding::Lf), QString("a\nb\nc\n\nd"));
    QCOMPARE(PastedText::withLineEnding(mixed, LineEnding::Crlf), QString("a\r\nb\r\nc\r\n\r\nd"));
    QCOMPARE(PastedText::withLineEnding("no breaks", LineEnding::Crlf), QString("no breaks"));
    QCOMPARE(PastedText::withLineEnding("a\rb", LineEnding::Crlf), QString("a\rb"));
}

void TstPastedText::removeOlderThan()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();

    QVERIFY(!PastedText::save(dir, Slot::Left, "x", kNow.addDays(-8)).isEmpty());
    QVERIFY(!PastedText::save(dir, Slot::Left, "x", kNow.addDays(-7)).isEmpty());
    QVERIFY(!PastedText::save(dir, Slot::Left, "x", kNow.addDays(-6)).isEmpty());
    QVERIFY(QDir().mkpath(dir + "/keep-me"));
    QVERIFY(QDir().mkpath(dir + "/20261117-093045"));  // not a pasted-text folder
    {
        QFile log(dir + "/diffpick.log");
        QVERIFY(log.open(QIODevice::WriteOnly));
    }

    // Exactly at the cutoff is kept.
    QCOMPARE(PastedText::removeOlderThan(dir, kNow.addDays(-7)), 1);
    QCOMPARE(subfolders(dir), (QStringList{"20261110-093045-123", "20261111-093045-123",
                                           "20261117-093045", "keep-me"}));
    QVERIFY(QFile::exists(dir + "/diffpick.log"));

    // A date in the future deletes all pasted texts and nothing else.
    QCOMPARE(PastedText::removeOlderThan(dir, kNow.addYears(1)), 2);
    QCOMPARE(subfolders(dir), (QStringList{"20261117-093045", "keep-me"}));
    QVERIFY(QFile::exists(dir + "/diffpick.log"));
}

void TstPastedText::removeOlderThanMissingDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QCOMPARE(PastedText::removeOlderThan(tmp.path() + "/missing", kNow), 0);
}

QTEST_GUILESS_MAIN(TstPastedText)
#include "tst_pastedtext.moc"
