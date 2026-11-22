#include "Log.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

}  // namespace

class TstLog : public QObject
{
    Q_OBJECT

    private slots:
    void writesMessagesAndCommands();
    void truncatesToLast256KiB();
    void disableStopsWriting();
};

void TstLog::writesMessagesAndCommands()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(Log::initialize(tmp.path()));
    Log::info(QStringLiteral("Meld detected"));
    Log::error(QStringLiteral("Meld missing"));
    Log::command(QStringLiteral("C:/Program Files/Meld/Meld.exe"),
                 {QStringLiteral("C:/a file.txt"), QStringLiteral("C:/b.txt")});

    const QByteArray contents = readAll(Log::filePath());
    QVERIFY(contents.contains("[INFO] Meld detected"));
    QVERIFY(contents.contains("[ERROR] Meld missing"));
    QVERIFY(contents.contains("[COMMAND] \"C:/Program Files/Meld/Meld.exe\""));
    QVERIFY(contents.contains("\"C:/a file.txt\" C:/b.txt"));
}

void TstLog::truncatesToLast256KiB()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/diffpick.log");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArray(300 * 1024, 'x')) > Log::kMaxBytes);
    QVERIFY(file.write("\nTAIL\n") > 0);
    file.close();

    QVERIFY(Log::initialize(tmp.path()));
    const QByteArray contents = readAll(path);
    QVERIFY(contents.size() <= Log::kMaxBytes);
    QVERIFY(contents.endsWith("TAIL\n"));
}

void TstLog::disableStopsWriting()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(Log::initialize(tmp.path()));
    Log::info(QStringLiteral("before disable"));
    const QString path = Log::filePath();
    const qint64 size = QFile(path).size();
    QVERIFY(size > 0);

    Log::disable();
    QVERIFY(Log::filePath().isEmpty());
    Log::info(QStringLiteral("after disable"));
    QCOMPARE(QFile(path).size(), size);
}

QTEST_GUILESS_MAIN(TstLog)
#include "tst_log.moc"
