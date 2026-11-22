#include "Cli.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TstCli : public QObject
{
    Q_OBJECT

    private slots:
    void openAndNormalize();
    void naturalSort();
    void compareModes();
    void markModes();
    void rejectsInvalidRequests();
};

namespace {

QString createFile(const QString &dir, const QString &name)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) ? path : QString();
}

}  // namespace

void TstCli::openAndNormalize()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = createFile(tmp.path(), QStringLiteral("a.txt"));
    QVERIFY(!file.isEmpty());

    const Cli::Request request = Cli::prepare({}, {QStringLiteral("a.txt")}, tmp.path());
    QVERIFY(request.isValid());
    QCOMPARE(request.mode, Cli::Mode::Open);
    QCOMPARE(request.items, QStringList{file});
    QVERIFY(!request.shouldCompareImmediately());
}

void TstCli::naturalSort()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a10 = createFile(tmp.path(), QStringLiteral("a10.txt"));
    const QString a2 = createFile(tmp.path(), QStringLiteral("a2.txt"));
    QVERIFY(!a10.isEmpty());
    QVERIFY(!a2.isEmpty());

    Cli::Options options;
    options.open = true;
    options.sort = true;
    const Cli::Request request = Cli::prepare(options, {a10, a2}, tmp.path());
    QVERIFY(request.isValid());
    QCOMPARE(request.items, (QStringList{a2, a10}));
}

void TstCli::compareModes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = createFile(tmp.path(), QStringLiteral("a.txt"));
    const QString b = createFile(tmp.path(), QStringLiteral("b.txt"));
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());

    Cli::Options compare;
    compare.compare = true;
    Cli::Request request = Cli::prepare(compare, {a}, tmp.path());
    QVERIFY(request.isValid());
    QVERIFY(!request.shouldCompareImmediately());

    request = Cli::prepare(compare, {a, b}, tmp.path());
    QVERIFY(request.isValid());
    QVERIFY(request.shouldCompareImmediately());
}

void TstCli::markModes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString marked = createFile(tmp.path(), QStringLiteral("marked.txt"));
    const QString other = createFile(tmp.path(), QStringLiteral("other.txt"));
    QVERIFY(!marked.isEmpty());
    QVERIFY(!other.isEmpty());

    Cli::Options mark;
    mark.mark = true;
    Cli::Request request = Cli::prepare(mark, {marked}, tmp.path());
    QVERIFY(request.isValid());
    QCOMPARE(request.mode, Cli::Mode::Mark);

    Cli::Options compareMarked;
    compareMarked.compareWithMarked = true;
    request = Cli::prepare(compareMarked, {other}, tmp.path(), marked);
    QVERIFY(request.isValid());
    QCOMPARE(request.items, (QStringList{marked, other}));
    QVERIFY(request.shouldCompareImmediately());
}

void TstCli::rejectsInvalidRequests()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = createFile(tmp.path(), QStringLiteral("a.txt"));
    const QString b = createFile(tmp.path(), QStringLiteral("b.txt"));
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());

    Cli::Options conflict;
    conflict.open = true;
    conflict.compare = true;
    QCOMPARE(Cli::prepare(conflict, {a}, tmp.path()).problem, Cli::Problem::ConflictingModes);

    Cli::Options install;
    install.install = true;
    QCOMPARE(Cli::prepare(install, {a}, tmp.path()).problem, Cli::Problem::UnexpectedItems);

    Cli::Options open;
    open.open = true;
    QCOMPARE(Cli::prepare(open, {a, b, a, b}, tmp.path()).problem, Cli::Problem::TooManyItems);

    Cli::Options compare;
    compare.compare = true;
    QCOMPARE(Cli::prepare(compare, {}, tmp.path()).problem, Cli::Problem::WrongCount);
    QCOMPARE(
        Cli::prepare(compare, {a, tmp.path() + QStringLiteral("/missing")}, tmp.path()).problem,
        Cli::Problem::Missing);
    QCOMPARE(Cli::prepare(compare, {a, a}, tmp.path()).problem, Cli::Problem::Duplicate);

    Cli::Options compareMarked;
    compareMarked.compareWithMarked = true;
    QCOMPARE(Cli::prepare(compareMarked, {b}, tmp.path(), QString()).problem,
             Cli::Problem::MarkedMissing);
    QCOMPARE(Cli::prepare(compareMarked, {a}, tmp.path(), a).problem, Cli::Problem::Duplicate);
}

QTEST_GUILESS_MAIN(TstCli)
#include "tst_cli.moc"
