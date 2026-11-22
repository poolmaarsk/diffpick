#include "MainWindow.hpp"
#include "BuildConfig.hpp"
#include "GitInfo.hpp"
#include "Cli.hpp"
#include "Integration.hpp"
#include "Log.hpp"
#include "PastedText.hpp"
#include "Settings.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

namespace {

int showError(const QString &message)
{
    Log::error(message);
#ifdef Q_OS_WIN
    QMessageBox::critical(nullptr, QObject::tr("diffpick"), message);
#else
    qCritical().noquote() << message;
#endif  // Q_OS_WIN

    return 1;
}

int showSuccess(const QString &message)
{
    Log::info(message);
#ifdef Q_OS_WIN
    QMessageBox::information(nullptr, QObject::tr("diffpick"), message);
#else
    qInfo().noquote() << message;
#endif  // Q_OS_WIN

    return 0;
}

QString problemMessage(const Cli::Request &request)
{
    switch (request.problem)
    {
    case Cli::Problem::ConflictingModes:
        return QObject::tr("Choose only one operation mode.");
    case Cli::Problem::UnexpectedItems:
        return QObject::tr("This operation does not accept file or folder arguments.");
    case Cli::Problem::TooManyItems:
        return QObject::tr("Choose at most three items.");
    case Cli::Problem::WrongCount:
        return QObject::tr("Choose the required number of items for this operation.");
    case Cli::Problem::Missing:
        return request.index >= 0 ? QObject::tr("Item %1 does not exist.").arg(request.index + 1)
                                  : QObject::tr("The selected item does not exist.");
    case Cli::Problem::MarkedMissing:
        return QObject::tr("The marked item does not exist.");
    case Cli::Problem::Duplicate:
        return QObject::tr("Item %1 is already used in another slot.").arg(request.index + 1);
    case Cli::Problem::None:
        return {};
    }

    return QObject::tr("Invalid command-line request.");
}

}  // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("diffpick"));
    QCoreApplication::setApplicationVersion(QLatin1String(BuildInfo::applicationVersion));
    QCoreApplication::setOrganizationName(QStringLiteral("diffpick"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/diffpick.png")));
    Settings settings;
    if (settings.diagnosticLoggingEnabled())
    {
        Log::initialize(PastedText::defaultDir());
        Log::info(QStringLiteral("diffpick started: version=%1 cmake=%2 gitTag=%3 gitHash=%4")
                      .arg(QLatin1String(BuildInfo::applicationVersion),
                           QLatin1String(BuildInfo::cmakeVersion), QLatin1String(BuildInfo::gitTag),
                           QLatin1String(BuildInfo::gitHash)));
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QObject::tr("Choose two or three items and compare them in Meld."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption openOption(QStringLiteral("open"),
                                        QObject::tr("Open the window with the supplied items."));
    const QCommandLineOption compareOption(QStringLiteral("compare"),
                                           QObject::tr("Compare the supplied items immediately."));
    const QCommandLineOption sortOption(
        QStringLiteral("sort"), QObject::tr("Sort items by file name before assigning slots."));
    const QCommandLineOption markOption(QStringLiteral("mark"),
                                        QObject::tr("Remember one item for a later comparison."));
    const QCommandLineOption compareMarkedOption(
        QStringLiteral("compare-with-marked"),
        QObject::tr("Compare one item with the previously marked item."));
    const QCommandLineOption installOption(QStringLiteral("install"),
                                           QObject::tr("Install per-user system integration."));
    const QCommandLineOption uninstallOption(QStringLiteral("uninstall"),
                                             QObject::tr("Remove per-user system integration."));
    parser.addOptions({openOption, compareOption, sortOption, markOption, compareMarkedOption,
                       installOption, uninstallOption});
    parser.addPositionalArgument(QStringLiteral("items"),
                                 QObject::tr("Files or folders to prefill."),
                                 QStringLiteral("[items...]"));
    parser.process(app);

    Cli::Options options;
    options.open = parser.isSet(openOption);
    options.compare = parser.isSet(compareOption);
    options.sort = parser.isSet(sortOption);
    options.mark = parser.isSet(markOption);
    options.compareWithMarked = parser.isSet(compareMarkedOption);
    options.install = parser.isSet(installOption);
    options.uninstall = parser.isSet(uninstallOption);
    const Cli::Request request =
        Cli::prepare(options, parser.positionalArguments(), QDir::currentPath(), settings.marked());
    if (!request.isValid())
    {
        return showError(problemMessage(request));
    }

    if (request.mode == Cli::Mode::Install)
    {
        const QString executable = Integration::executablePath();
        const Integration::Result result = Integration::install(executable, executable);
        if (!result.ok)
        {
            return showError(result.message);
        }

        settings.setIntegrationExePath(executable);
        return showSuccess(result.message);
    }

    if (request.mode == Cli::Mode::Uninstall)
    {
        const Integration::Result result = Integration::remove();
        if (!result.ok)
        {
            return showError(result.message);
        }

        settings.setIntegrationExePath(QString());
        return showSuccess(result.message);
    }

    if (request.mode == Cli::Mode::Mark)
    {
        settings.setMarked(request.items.first());
        return 0;
    }

    MainWindow window;
    window.setItems(request.items);
    window.show();
    if (request.shouldCompareImmediately())
    {
        QTimer::singleShot(0, &window, &MainWindow::compareNow);
    }

    return app.exec();
}
