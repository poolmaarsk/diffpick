#include "Integration.hpp"
#include "MainWindow.hpp"
#include "SettingsDialog.hpp"
#include "SlotWidget.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>

class FolderOpenRecorder : public QObject
{
    Q_OBJECT

    public slots:
    void recordFolder(const QUrl &url) { openedFolder = url; }

    public:
    QUrl openedFolder;
};

class TstMainWindow : public QObject
{
    Q_OBJECT

    private slots:
    void itemMapping();
    void browseStartDir();
    void exchangeInputs();
    void actionButtonDesign();
    void clipboardPaths();
    void browseKeyboardShortcut();
    void emptyInputResetsStatus();
    void recentComparisons();
    void menuOrganization();
    void compareKeyboardShortcut();
    void swapKeyboardShortcut();
    void settingsDialogCommand();
    void browseLocationControls();
    void integrationExecutablePath();
    void integrationRepairNotice();
#ifdef Q_OS_LINUX
    void fileManagerIntegration();
    void compareDetectsMeldInBackground();
    void duplicateInputsRequireConfirmation();
#endif  // Q_OS_LINUX
};

#ifdef Q_OS_LINUX
namespace {

bool writeTestExecutable(const QString &path, const QByteArray &script)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(script) != script.size())
    {
        return false;
    }

    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                               QFileDevice::ExeOwner);
}

}  // namespace

void TstMainWindow::fileManagerIntegration()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QByteArray originalPath = qgetenv("PATH");
    const QByteArray originalData = qgetenv("XDG_DATA_HOME");
    const QByteArray originalConfig = qgetenv("XDG_CONFIG_HOME");
    const auto restoreEnvironment = qScopeGuard([&] {
        qputenv("PATH", originalPath);
        qputenv("XDG_DATA_HOME", originalData);
        qputenv("XDG_CONFIG_HOME", originalConfig);
    });
    qputenv("PATH", QFile::encodeName(sandbox.path()));
    qputenv("XDG_DATA_HOME", QFile::encodeName(sandbox.filePath(QStringLiteral("data"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(sandbox.filePath(QStringLiteral("config"))));

    QVERIFY(writeTestExecutable(sandbox.filePath(QStringLiteral("nautilus")), "#!/bin/sh\n"));
    const QString executable = sandbox.filePath(QStringLiteral("diffpick"));
    QVERIFY(writeTestExecutable(executable, "#!/bin/sh\nprintf '%s\\n' \"$@\"\n"));
    const auto installed = Integration::install(executable);
    QVERIFY2(installed.ok, qPrintable(installed.message));

    const QString scriptPath =
        sandbox.filePath(QStringLiteral("data/nautilus/scripts/Diffpick/Open in diffpick"));
    QProcess launcher;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS"),
                       sandbox.filePath(QStringLiteral("*.txt")));
    launcher.setProcessEnvironment(environment);
    QFile matchingFile(sandbox.filePath(QStringLiteral("other.txt")));
    QVERIFY(matchingFile.open(QIODevice::WriteOnly));
    matchingFile.close();
    launcher.start(scriptPath, {});
    QVERIFY(launcher.waitForFinished(2000));
    QCOMPARE(launcher.exitCode(), 0);
    QCOMPARE(launcher.readAllStandardOutput(),
             "--open\n" + QFile::encodeName(sandbox.filePath(QStringLiteral("*.txt"))) + '\n');

    QVERIFY(QFile::remove(scriptPath));
    QVERIFY(QDir().mkdir(scriptPath));
    const auto incompleteRemoval = Integration::remove();
    QVERIFY(!incompleteRemoval.ok);
    QVERIFY(incompleteRemoval.message.contains(scriptPath));
    QVERIFY(QDir().rmdir(scriptPath));
    QVERIFY(Integration::remove().ok);
    QVERIFY(Integration::remove().ok);
}

void TstMainWindow::compareDetectsMeldInBackground()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QByteArray originalPath = qgetenv("PATH");
    const auto restoreEnvironment = qScopeGuard([&] { qputenv("PATH", originalPath); });
    qputenv("PATH", QFile::encodeName(sandbox.path()));
    QVERIFY(writeTestExecutable(sandbox.filePath(QStringLiteral("flatpak")),
                                "#!/bin/sh\n"
                                "if [ \"$1\" = info ]; then\n"
                                "    attempts=0\n"
                                "    while [ ! -f \"$0.ready\" ] && [ \"$attempts\" -lt 75 ]; do\n"
                                "        attempts=$((attempts + 1))\n"
                                "        /bin/sleep 0.02\n"
                                "    done\n"
                                "else\n"
                                "    printf '%s\\n' \"$@\" > \"$0.args\"\n"
                                "fi\n"));

    const QString leftPath = sandbox.filePath(QStringLiteral("left.txt"));
    const QString rightPath = sandbox.filePath(QStringLiteral("right.txt"));
    for (const QString &path : {leftPath, rightPath})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    const QString settingsPath = sandbox.filePath(QStringLiteral("settings.ini"));
    {
        Settings settings(settingsPath);
        settings.setCloseAfterLaunch(false);
    }

    MainWindow window(settingsPath, nullptr);
    window.setItems({leftPath, rightPath});
    window.show();
    window.compareNow();
    auto *compareButton = window.findChild<QPushButton *>(QStringLiteral("compareButton"));
    QVERIFY(compareButton);
    QVERIFY(!compareButton->isEnabled());

    bool eventLoopResponded = false;
    QTimer::singleShot(0, &window, [&] { eventLoopResponded = true; });
    QTRY_VERIFY(eventLoopResponded);
    QVERIFY(!compareButton->isEnabled());
    QFile readyFile(sandbox.filePath(QStringLiteral("flatpak.ready")));
    QVERIFY(readyFile.open(QIODevice::WriteOnly));
    readyFile.close();

    QFile argumentsFile(sandbox.filePath(QStringLiteral("flatpak.args")));
    QTRY_VERIFY(argumentsFile.size() > 0);
    QVERIFY(argumentsFile.open(QIODevice::ReadOnly));
    QCOMPARE(argumentsFile.readAll(),
             QStringLiteral("run\norg.gnome.meld\n%1\n%2\n").arg(leftPath, rightPath).toUtf8());
    QTRY_VERIFY(compareButton->isEnabled());
}
void TstMainWindow::duplicateInputsRequireConfirmation()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QString meldPath = sandbox.filePath(QStringLiteral("meld"));
    QVERIFY(writeTestExecutable(meldPath, "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$0.args\"\n"));
    QFile input(sandbox.filePath(QStringLiteral("input.txt")));
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.close();

    const QString settingsPath = sandbox.filePath(QStringLiteral("settings.ini"));
    {
        Settings settings(settingsPath);
        settings.setMeldCommand(meldPath);
        settings.setCloseAfterLaunch(false);
    }

    MainWindow window(settingsPath, nullptr);
    window.setItems({input.fileName(), input.fileName()});
    window.show();

    auto *compare = window.findChild<QPushButton *>(QStringLiteral("compareButton"));
    QVERIFY(compare);
    for (const bool proceed : {false, true})
    {
        bool warningShown = false;
        QTimer::singleShot(0, &window, [&] {
            auto *warning = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!warning)
            {
                return;
            }

            warningShown = warning->windowTitle() == QStringLiteral("Duplicate inputs") &&
                           warning->defaultButton() == warning->button(QMessageBox::Cancel);
            for (auto *button : warning->buttons())
            {
                if (proceed && warning->buttonRole(button) == QMessageBox::AcceptRole)
                {
                    button->click();
                    return;
                }
            }

            warning->reject();
        });
        window.compareNow();
        QVERIFY(warningShown);
        QCOMPARE(compare->isEnabled(), !proceed);
    }

    QFile args(meldPath + QStringLiteral(".args"));
    QTRY_VERIFY(args.size() > 0);
    QVERIFY(args.open(QIODevice::ReadOnly));
    QCOMPARE(args.readAll(), QStringLiteral("%1\n%1\n").arg(input.fileName()).toUtf8());
}
#endif  // Q_OS_LINUX

void TstMainWindow::integrationRepairNotice()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    const QString settingsPath = settingsDir.filePath(QStringLiteral("settings.ini"));

    for (const QString &installedPath : {QString(), Integration::executablePath(),
                                         settingsDir.filePath(QStringLiteral("old/diffpick"))})
    {
        {
            Settings settings(settingsPath);
            settings.setIntegrationExePath(installedPath);
        }
        MainWindow window(settingsPath, nullptr);
        auto *notice = window.findChild<QWidget *>(QStringLiteral("integrationNotice"));
        QVERIFY(notice);
        const bool needsRepair =
            !installedPath.isEmpty() && installedPath != Integration::executablePath();
        QCOMPARE(!notice->isHidden(), needsRepair);
    }
}

void TstMainWindow::integrationExecutablePath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString appImage = tmp.path() + QStringLiteral("/diffpick.AppImage");
    QFile file(appImage);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    const QByteArray previous = qgetenv("APPIMAGE");
    qputenv("APPIMAGE", appImage.toLocal8Bit());
#ifdef Q_OS_LINUX
    QCOMPARE(Integration::executablePath(), appImage);
#else
    QCOMPARE(Integration::executablePath(), QApplication::applicationFilePath());
#endif  // Q_OS_LINUX

    if (previous.isNull())
    {
        qunsetenv("APPIMAGE");
    }
    else
    {
        qputenv("APPIMAGE", previous);
    }
}

void TstMainWindow::itemMapping()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QStringList paths;
    for (int i = 0; i < 4; ++i)
    {
        const QString path = tmp.path() + QStringLiteral("/%1.txt").arg(i);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        paths << path;
    }

    MainWindow window;
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *middle = window.findChild<SlotWidget *>(QStringLiteral("middleSlot"));
    auto *right = window.findChild<SlotWidget *>(QStringLiteral("rightSlot"));
    QVERIFY(left);
    QVERIFY(middle);
    QVERIFY(right);

    window.setItems({paths[0], paths[1]});
    QCOMPARE(left->path(), paths[0]);
    QVERIFY(middle->path().isEmpty());
    QCOMPARE(right->path(), paths[1]);

    window.setItems({paths[0], paths[1], paths[2]});
    QCOMPARE(left->path(), paths[0]);
    QCOMPARE(middle->path(), paths[1]);
    QCOMPARE(right->path(), paths[2]);

    window.setItems({paths[3]}, middle);
    QCOMPARE(left->path(), paths[0]);
    QCOMPARE(middle->path(), paths[3]);
    QCOMPARE(right->path(), paths[2]);

    window.setItems(paths);  // More than three changes nothing.
    QCOMPARE(left->path(), paths[0]);
    QCOMPARE(middle->path(), paths[3]);
    QCOMPARE(right->path(), paths[2]);
}

void TstMainWindow::browseStartDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString firstDir = tmp.path() + QStringLiteral("/first");
    const QString lastDir = tmp.path() + QStringLiteral("/last");
    const QString preferredDir = tmp.path() + QStringLiteral("/preferred");
    QVERIFY(QDir().mkpath(firstDir));
    QVERIFY(QDir().mkpath(lastDir));
    QVERIFY(QDir().mkpath(preferredDir));
    const QString filePath = firstDir + QStringLiteral("/a.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QCOMPARE(SlotWidget::startDirForPath(filePath, lastDir), firstDir);
    QCOMPARE(SlotWidget::startDirForPath(firstDir, lastDir), firstDir);
    QCOMPARE(SlotWidget::startDirForPath(QString(), lastDir), lastDir);
    QCOMPARE(SlotWidget::startDirForPath(tmp.path() + QStringLiteral("/missing"), lastDir),
             lastDir);
    QCOMPARE(SlotWidget::startDirForPath(filePath, lastDir, preferredDir), firstDir);
    QCOMPARE(SlotWidget::startDirForPath({}, lastDir, preferredDir), preferredDir);
    QCOMPARE(SlotWidget::startDirForPath({}, lastDir, filePath), lastDir);
    QCOMPARE(SlotWidget::startDirForPath({}, {}, filePath), QDir::homePath());
}

void TstMainWindow::exchangeInputs()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString leftPath = tmp.path() + QStringLiteral("/left.txt");
    const QString rightPath = tmp.path() + QStringLiteral("/right.txt");
    for (const QString &path : {leftPath, rightPath})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    SlotWidget left(QStringLiteral("Left"));
    SlotWidget right(QStringLiteral("Right"));
    left.setPath(leftPath);
    right.setPath(rightPath);
    left.exchangeInputWith(&right);
    QCOMPARE(left.path(), rightPath);
    QCOMPARE(right.path(), leftPath);

    SlotWidget middle(QStringLiteral("Middle"));
    middle.exchangeInputWith(&left);
    QCOMPARE(middle.path(), rightPath);
    QVERIFY(left.path().isEmpty());

    left.setPastedText(leftPath, QStringLiteral("a\r\nb"), PastedText::LineEnding::Crlf);
    left.exchangeInputWith(&middle);
    QVERIFY(middle.isPastedText());
    QCOMPARE(middle.pastedText(), QStringLiteral("a\r\nb"));
    QCOMPARE(middle.pastedTextLineEnding(), PastedText::LineEnding::Crlf);
}

void TstMainWindow::actionButtonDesign()
{
    MainWindow window;
    window.resize(800, 300);
    window.show();
    QTest::qWait(20);

    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *editor = left ? left->findChild<QLineEdit *>() : nullptr;
    auto *swap = window.findChild<QPushButton *>(QStringLiteral("swapButton"));
    auto *compare = window.findChild<QPushButton *>(QStringLiteral("compareButton"));
    QVERIFY(editor);
    QVERIFY(swap);
    QVERIFY(compare);
    QVERIFY(swap->width() < editor->width());
    QVERIFY(swap->text().contains(QChar(0x21c4)));
    QVERIFY(swap->width() >= 240);
    QVERIFY(!swap->styleSheet().isEmpty());
    QVERIFY(!compare->styleSheet().isEmpty());

    const int editorTop = editor->mapTo(&window, QPoint()).y();
    const int compareTop = compare->mapTo(&window, QPoint()).y();
    const int expandedHeight = window.height() + 300;
    window.resize(window.width(), expandedHeight);
    QCoreApplication::processEvents();
    QCOMPARE(window.height(), expandedHeight);
    QCOMPARE(editor->mapTo(&window, QPoint()).y(), editorTop);
    QCOMPARE(compare->mapTo(&window, QPoint()).y(), compareTop);
}

void TstMainWindow::clipboardPaths()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString first = tmp.path() + QStringLiteral("/first.txt");
    const QString second = tmp.path() + QStringLiteral("/second.txt");
    for (const QString &path : {first, second})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    MainWindow window;
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *middle = window.findChild<SlotWidget *>(QStringLiteral("middleSlot"));
    auto *right = window.findChild<SlotWidget *>(QStringLiteral("rightSlot"));
    QVERIFY(left);
    QVERIFY(middle);
    QVERIFY(right);

    QApplication::clipboard()->setText(QDir::toNativeSeparators(first) + QLatin1Char('\n') +
                                       QDir::toNativeSeparators(second));
    window.pasteFromClipboard(middle);
    QCOMPARE(left->path(), first);
    QCOMPARE(right->path(), second);

    QApplication::clipboard()->setText(QLatin1Char('"') + QDir::toNativeSeparators(first) +
                                       QLatin1Char('"'));
    window.pasteFromClipboard(middle);
    QCOMPARE(middle->path(), first);
}

void TstMainWindow::browseKeyboardShortcut()
{
    MainWindow window;
    window.show();
    QTest::qWait(20);
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *menu = left ? left->findChild<QMenu *>() : nullptr;
    QVERIFY(menu);

    QTest::keyClick(&window, Qt::Key_L, Qt::AltModifier);
    QTRY_VERIFY(menu->isVisible());
    QStringList numberedActions;
    for (QAction *action : menu->actions())
    {
        if (!action->isSeparator())
        {
            numberedActions << action->text();
        }
    }

    QCOMPARE(numberedActions.size(), 5);
    for (int i = 0; i < numberedActions.size(); ++i)
    {
        QVERIFY(numberedActions[i].startsWith(QStringLiteral("&%1.").arg(i + 1)));
    }

    menu->close();

    auto *title = left->findChild<QLabel *>(QStringLiteral("titleLabel"));
    QVERIFY(title);
    QTest::mouseClick(title, Qt::LeftButton);
    QTRY_VERIFY(menu->isVisible());
    menu->close();
}

void TstMainWindow::emptyInputResetsStatus()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    SlotWidget slot(QStringLiteral("Left"));
    auto *status = slot.findChild<QLabel *>(QStringLiteral("statusLabel"));
    QVERIFY(status);

    slot.setPath(tmp.path() + QStringLiteral("/missing.txt"));
    QTRY_VERIFY(!status->styleSheet().isEmpty());

    slot.setPath(QString());
    QTRY_COMPARE(status->text(), QStringLiteral("Not found"));
    QTRY_VERIFY(status->styleSheet().isEmpty());
}

void TstMainWindow::recentComparisons()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QStringList paths;
    for (const QString &name :
         {QStringLiteral("left.txt"), QStringLiteral("middle.txt"), QStringLiteral("right.txt")})
    {
        const QString path = tmp.path() + QLatin1Char('/') + name;
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        paths << path;
    }

    const QString settingsFile = tmp.path() + QStringLiteral("/settings.ini");
    Settings settings(settingsFile);
    const Comparison valid{paths[0], paths[1], paths[2]};
    settings.addToHistory(valid);
    settings.addToHistory({tmp.path() + QStringLiteral("/missing.txt"), QString(), paths[2]});

    MainWindow window(settingsFile, nullptr);
    auto *recent = window.findChild<QMenu *>(QStringLiteral("recentMenu"));
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *middle = window.findChild<SlotWidget *>(QStringLiteral("middleSlot"));
    auto *right = window.findChild<SlotWidget *>(QStringLiteral("rightSlot"));
    QVERIFY(recent);
    QTRY_COMPARE(recent->actions().size(), 1);

    recent->actions().first()->trigger();
    QCOMPARE(left->path(), valid.left);
    QCOMPARE(middle->path(), valid.middle);
    QCOMPARE(right->path(), valid.right);
}

void TstMainWindow::menuOrganization()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString settingsFile = tmp.path() + QStringLiteral("/settings.ini");
    {
        MainWindow window(settingsFile, nullptr);
        auto *mainMenu = window.findChild<QMenu *>(QStringLiteral("mainMenu"));
        auto *recentMenu = window.findChild<QMenu *>(QStringLiteral("recentMenu"));
        auto *integrationMenu = window.findChild<QMenu *>(QStringLiteral("integrationMenu"));
        auto *closeAction = window.findChild<QAction *>(QStringLiteral("closeAfterLaunchAction"));
        auto *settingsAction = window.findChild<QAction *>(QStringLiteral("settingsAction"));
        QVERIFY(mainMenu);
        QVERIFY(recentMenu);
        QVERIFY(integrationMenu);
        QVERIFY(closeAction);
        QVERIFY(settingsAction);
        QVERIFY(mainMenu->actions().contains(recentMenu->menuAction()));
        QVERIFY(mainMenu->actions().contains(integrationMenu->menuAction()));
        QVERIFY(mainMenu->actions().contains(closeAction));
        QVERIFY(mainMenu->actions().contains(settingsAction));
        QVERIFY(closeAction->isCheckable());
        QVERIFY(closeAction->isChecked());

        closeAction->setChecked(false);
        window.show();
        window.close();
    }
    QVERIFY(!Settings(settingsFile).closeAfterLaunch());
}

void TstMainWindow::compareKeyboardShortcut()
{
    MainWindow window;
    auto *compare = window.findChild<QPushButton *>(QStringLiteral("compareButton"));
    QVERIFY(compare);
    QCOMPARE(compare->text(), QStringLiteral("C̲ompare"));

    QObject::disconnect(compare, nullptr, &window, nullptr);
    QSignalSpy clicked(compare, &QPushButton::clicked);
    window.show();
    QTest::qWait(20);
    QVERIFY(QApplication::focusWidget());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_C, Qt::AltModifier);
    QTRY_COMPARE(clicked.count(), 1);
}

void TstMainWindow::swapKeyboardShortcut()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString leftPath = tmp.path() + QStringLiteral("/left.txt");
    const QString rightPath = tmp.path() + QStringLiteral("/right.txt");
    for (const QString &path : {leftPath, rightPath})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    MainWindow window;
    window.setItems({leftPath, rightPath});
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    auto *right = window.findChild<SlotWidget *>(QStringLiteral("rightSlot"));
    QVERIFY(left);
    QVERIFY(right);
    left->setPastedText(leftPath, QStringLiteral("a\r\nb"), PastedText::LineEnding::Crlf);
    window.show();
    QTest::qWait(20);
    QVERIFY(QApplication::focusWidget());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_S, Qt::AltModifier);
    QTRY_COMPARE(left->path(), rightPath);
    QCOMPARE(right->path(), leftPath);
    QVERIFY(!left->isPastedText());
    QVERIFY(right->isPastedText());
    QCOMPARE(right->pastedText(), QStringLiteral("a\r\nb"));
    QCOMPARE(right->pastedTextLineEnding(), PastedText::LineEnding::Crlf);
}

void TstMainWindow::settingsDialogCommand()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString settingsFolder = tmp.filePath(QStringLiteral("configuration"));
    const QString settingsFile = QDir(settingsFolder).filePath(QStringLiteral("diffpick.ini"));
    SettingsDialog dialog(QStringLiteral("flatpak run org.gnome.meld"), true, settingsFile);
    auto *command = dialog.findChild<QLineEdit *>(QStringLiteral("meldCommandEdit"));
    auto *detected = dialog.findChild<QLabel *>(QStringLiteral("detectedMeldLabel"));
    QVERIFY(command);
    QVERIFY(detected);
    auto *logging = dialog.findChild<QCheckBox *>(QStringLiteral("diagnosticLoggingCheckBox"));
    QVERIFY(logging);
    QVERIFY(logging->isChecked());
    QCOMPARE(dialog.meldCommand(), QStringLiteral("flatpak run org.gnome.meld"));

    command->setText(QStringLiteral("  C:/Tools/Meld.exe  "));
    QCOMPARE(dialog.meldCommand(), QStringLiteral("C:/Tools/Meld.exe"));

    auto *settingsPath = dialog.findChild<QLineEdit *>(QStringLiteral("settingsFilePath"));
    auto *openSettingsFolder =
        dialog.findChild<QPushButton *>(QStringLiteral("openSettingsFolder"));
    QVERIFY(settingsPath);
    QVERIFY(openSettingsFolder);
    QVERIFY(settingsPath->isReadOnly());
    QCOMPARE(settingsPath->text(), QDir::toNativeSeparators(settingsFile));

    FolderOpenRecorder recorder;
    QDesktopServices::setUrlHandler(QStringLiteral("file"), &recorder, "recordFolder");
    const auto restoreHandler =
        qScopeGuard([] { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); });
    openSettingsFolder->click();
    QCOMPARE(recorder.openedFolder, QUrl::fromLocalFile(settingsFolder));
    QVERIFY(QDir(settingsFolder).exists());
}

void TstMainWindow::browseLocationControls()
{
    QTemporaryDir chosenFolder;
    QVERIFY(chosenFolder.isValid());
    BrowseLocations locations;
    locations.paths[0] = QStringLiteral("/first");
    locations.paths[4] = QStringLiteral("/last");
    locations.activeIndex = 0;
    const QString settingsPath = chosenFolder.filePath(QStringLiteral("settings.ini"));
    Settings(settingsPath).setBrowseLocations(locations);
    MainWindow window(settingsPath, nullptr);
    window.show();
    auto *row = window.findChild<QWidget *>(QStringLiteral("browseLocationsRow"));
    auto *left = window.findChild<SlotWidget *>(QStringLiteral("leftSlot"));
    QVERIFY(row);
    QVERIFY(left);
    QVERIFY(row->geometry().bottom() < left->geometry().top());
    for (int index = 0; index < BrowseLocations::count; ++index)
    {
        auto *button = row->findChild<QPushButton *>(QStringLiteral("browseLocation%1").arg(index));
        QVERIFY(button);
        QCOMPARE(button->text() == QStringLiteral("+"), locations.paths[index].isEmpty());
    }

    auto *first = row->findChild<QPushButton *>(QStringLiteral("browseLocation0"));
    auto *last = row->findChild<QPushButton *>(QStringLiteral("browseLocation4"));
    QVERIFY(first);
    QVERIFY(last);
    QVERIFY(first->isChecked());
    last->click();
    QVERIFY(!first->isChecked());
    QVERIFY(last->isChecked());
    QCOMPARE(Settings(settingsPath).browseLocations().activeIndex, 4);
    last->click();
    QVERIFY(Settings(settingsPath).browseLocations().activePath().isEmpty());

    last->click();
    auto *remove = row->findChild<QPushButton *>(QStringLiteral("removeBrowseLocation4"));
    QVERIFY(remove);
    remove->click();
    QVERIFY(remove->isHidden());
    QVERIFY(Settings(settingsPath).browseLocations().paths[4].isEmpty());
    QVERIFY(Settings(settingsPath).browseLocations().activePath().isEmpty());
    QCOMPARE(last->text(), QStringLiteral("+"));

    const bool nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard([nativeDialogsDisabled] {
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled);
    });
    QTimer::singleShot(0, &window, [&window, &chosenFolder] {
        auto *picker = window.findChild<QFileDialog *>();
        if (!picker)
        {
            return;
        }

        picker->setDirectory(chosenFolder.path());
        picker->selectFile(chosenFolder.path());
        static_cast<QDialog *>(picker)->accept();
        if (picker->isVisible())
        {
            picker->reject();
        }
    });
    last->click();
    QCOMPARE(Settings(settingsPath).browseLocations().paths[4], chosenFolder.path());
    QVERIFY(last->isCheckable());
    QVERIFY(!remove->isHidden());
}

QTEST_MAIN(TstMainWindow)
#include "tst_mainwindow.moc"
