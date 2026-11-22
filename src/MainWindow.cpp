#include "MainWindow.hpp"

#include "BuildConfig.hpp"
#include "GitInfo.hpp"
#include "Integration.hpp"
#include "Log.hpp"
#include "MeldLauncher.hpp"
#include "PastedText.hpp"
#include "PathUtil.hpp"
#include "SlotWidget.hpp"
#include "SettingsDialog.hpp"
#include "TextDialog.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPushButton>
#include <QPointer>
#include <QPromise>
#include <QRegularExpression>
#include <QShortcut>
#include <QStatusBar>
#include <QThreadPool>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(Settings().fileName(), parent)
{}

MainWindow::MainWindow(const QString &settingsFile, QWidget *parent)
    : QMainWindow(parent)
    , m_left(new SlotWidget(tr("<b><u>L</u></b>eft"), this))
    , m_middle(new SlotWidget(tr("<b><u>M</u></b>iddle"), this))
    , m_right(new SlotWidget(tr("<b><u>R</u></b>ight"), this))
    , m_swapButton(new QPushButton(tr("⇄  S̲wap Left / Right"), this))
    , m_compareButton(new QPushButton(tr("C̲ompare"), this))
    , m_recentMenu(new QMenu(tr("Recent"), this))
    , m_closeAfterLaunchAction(new QAction(tr("Close after launch"), this))
    , m_integrationNotice(new QWidget(this))
    , m_settings(settingsFile)
{
    PastedText::removeOlderThan(PastedText::defaultDir(), QDateTime::currentDateTime().addDays(-7));
    setWindowTitle(tr("diffpick"));
    m_left->setObjectName(QStringLiteral("leftSlot"));
    m_middle->setObjectName(QStringLiteral("middleSlot"));
    m_right->setObjectName(QStringLiteral("rightSlot"));
    m_swapButton->setObjectName(QStringLiteral("swapButton"));
    m_swapButton->setMinimumWidth(240);
    m_swapButton->setToolTip(tr("Swap Left and Right (Alt+S)"));
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->addWidget(createBrowseLocationsRow(central));
    layout->addWidget(m_left);
    layout->addWidget(m_right);

    m_swapButton->setStyleSheet(
        QStringLiteral("QPushButton { padding: 5px 16px; border: 1px solid palette(mid);"
                       " border-radius: 5px; background: palette(button); font-weight: 600; }"
                       "QPushButton:hover { background: palette(midlight); }"
                       "QPushButton:pressed { background: palette(mid); }"));
    auto *swapLayout = new QHBoxLayout;
    swapLayout->addStretch();
    swapLayout->addWidget(m_swapButton);
    swapLayout->addStretch();
    layout->addLayout(swapLayout);
    layout->addWidget(m_middle);

    auto *mainMenu = menuBar()->addMenu(tr("Menu"));
    mainMenu->setObjectName(QStringLiteral("mainMenu"));
    m_recentMenu->setObjectName(QStringLiteral("recentMenu"));
    mainMenu->addMenu(m_recentMenu);

    auto *integrationMenu = new QMenu(tr("System integration"), mainMenu);
    integrationMenu->setObjectName(QStringLiteral("integrationMenu"));
    integrationMenu->addAction(tr("Install"), this, &MainWindow::installIntegration);
    integrationMenu->addAction(tr("Remove"), this, &MainWindow::removeIntegration);
    integrationMenu->addAction(tr("Repair"), this, &MainWindow::installIntegration);
    mainMenu->addMenu(integrationMenu);
    mainMenu->addSeparator();
    QAction *settingsAction = mainMenu->addAction(tr("Settings…"), this, &MainWindow::openSettings);
    settingsAction->setObjectName(QStringLiteral("settingsAction"));
    m_closeAfterLaunchAction->setObjectName(QStringLiteral("closeAfterLaunchAction"));
    m_closeAfterLaunchAction->setCheckable(true);
    mainMenu->addAction(m_closeAfterLaunchAction);

    auto *buttons = new QHBoxLayout;
    m_compareButton->setObjectName(QStringLiteral("compareButton"));
    m_compareButton->setDefault(true);
    m_compareButton->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 22px; border: 1px solid #1d4ed8; border-radius: 5px;"
        " background: #2563eb; color: white; font-weight: 700; }"
        "QPushButton:hover { background: #1d4ed8; }"
        "QPushButton:pressed { background: #1e40af; }"
        "QPushButton:disabled { background: #94a3b8; border-color: #94a3b8; }"));
    buttons->addStretch();
    buttons->addWidget(m_compareButton);
    layout->addLayout(buttons);
    setCentralWidget(central);

    m_integrationNotice->setObjectName(QStringLiteral("integrationNotice"));
    auto *noticeLayout = new QHBoxLayout(m_integrationNotice);
    noticeLayout->setContentsMargins(0, 0, 0, 0);
    noticeLayout->addWidget(
        new QLabel(tr("System integration points to an old location."), m_integrationNotice));
    auto *repairIntegrationButton = new QPushButton(tr("Repair"), m_integrationNotice);
    repairIntegrationButton->setAutoDefault(false);
    noticeLayout->addWidget(repairIntegrationButton);
    connect(repairIntegrationButton, &QPushButton::clicked, this, &MainWindow::installIntegration);
    statusBar()->addPermanentWidget(m_integrationNotice);
    refreshIntegrationNotice();

    m_closeAfterLaunchAction->setChecked(m_settings.closeAfterLaunch());
    if (!m_settings.geometry().isEmpty())
    {
        restoreGeometry(m_settings.geometry());
    }

    rebuildRecentMenu(m_settings.history());
    checkRecentInBackground();

    connect(m_swapButton, &QPushButton::clicked, this,
            [this] { m_left->exchangeInputWith(m_right); });
    auto *swapShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+S")), this);
    connect(swapShortcut, &QShortcut::activated, m_swapButton, &QPushButton::animateClick);
    connect(m_compareButton, &QPushButton::clicked, this, &MainWindow::compare);
    const QList<SlotWidget *> slotWidgets{m_left, m_middle, m_right};
    for (SlotWidget *slot : slotWidgets)
    {
        slot->setLastDir(m_settings.lastDir());
        connect(
            slot, &SlotWidget::itemsDropped, this,
            [this](const QStringList &dropped, SlotWidget *target) { setItems(dropped, target); });
        connect(slot, &SlotWidget::lastDirChanged, this, [this, slotWidgets](const QString &dir) {
            m_settings.setLastDir(dir);
            for (SlotWidget *other : slotWidgets)
            {
                other->setLastDir(dir);
            }
        });
        connect(slot, &SlotWidget::textRequested, this, &MainWindow::editText);
        connect(slot, &SlotWidget::clipboardRequested, this, &MainWindow::pasteFromClipboard);
        connect(slot, &SlotWidget::plainTextDropped, this,
                [this](const QString &text, SlotWidget *target) {
                    storePastedText(target, text, PastedText::lineEndingFor(text));
                });
        connect(slot, &SlotWidget::openPastedTextDirRequested, this,
                &MainWindow::openPastedTextDir);
    }

    auto addFocusShortcut = [this](const QKeySequence &key, SlotWidget *slot) {
        auto *shortcut = new QShortcut(key, this);
        connect(shortcut, &QShortcut::activated, slot, &SlotWidget::focusEditor);
    };
    addFocusShortcut(QKeySequence(QStringLiteral("Ctrl+1")), m_left);
    addFocusShortcut(QKeySequence(QStringLiteral("Ctrl+2")), m_middle);
    addFocusShortcut(QKeySequence(QStringLiteral("Ctrl+3")), m_right);

    auto addBrowseShortcut = [this](const QString &key, SlotWidget *slot) {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        connect(shortcut, &QShortcut::activated, slot, &SlotWidget::showBrowseMenu);
        slot->setBrowseShortcutText(QKeySequence(key).toString(QKeySequence::NativeText));
    };
    addBrowseShortcut(QStringLiteral("Alt+L"), m_left);
    addBrowseShortcut(QStringLiteral("Alt+R"), m_right);
    addBrowseShortcut(QStringLiteral("Alt+M"), m_middle);

    auto *compareShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(compareShortcut, &QShortcut::activated, m_compareButton, &QPushButton::animateClick);
    auto *compareLetterShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+C")), this);
    connect(compareLetterShortcut, &QShortcut::activated, m_compareButton,
            &QPushButton::animateClick);
    m_compareButton->setToolTip(tr("Compare (Alt+C or Ctrl+Enter)"));
    m_left->focusEditor();
}

void MainWindow::compareNow()
{
    compare();
}

void MainWindow::rebuildRecentMenu(const QList<Comparison> &history)
{
    m_recentMenu->clear();
    if (history.isEmpty())
    {
        QAction *empty = m_recentMenu->addAction(tr("No recent comparisons"));
        empty->setEnabled(false);
        return;
    }

    const auto displayName = [](const QString &path) {
        if (path.isEmpty())
        {
            return QString();
        }

        const QString name = QFileInfo(path).fileName();
        return name.isEmpty() ? QDir::toNativeSeparators(path) : name;
    };
    for (const Comparison &comparison : history)
    {
        QStringList names{displayName(comparison.left)};
        if (!comparison.middle.isEmpty())
        {
            names << displayName(comparison.middle);
        }

        names << displayName(comparison.right);
        QAction *action = m_recentMenu->addAction(names.join(QStringLiteral(" ↔ ")));
        QStringList paths{QDir::toNativeSeparators(comparison.left)};
        if (!comparison.middle.isEmpty())
        {
            paths << QDir::toNativeSeparators(comparison.middle);
        }

        paths << QDir::toNativeSeparators(comparison.right);
        action->setToolTip(paths.join(QLatin1Char('\n')));
        connect(action, &QAction::triggered, this,
                [this, comparison] { restoreComparison(comparison); });
    }
}

void MainWindow::checkRecentInBackground()
{
    const QList<Comparison> history = m_settings.history();
    if (history.isEmpty())
    {
        return;
    }

    const quint64 generation = ++m_recentCheckGeneration;
    QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start([self, history, generation] {
        QList<Comparison> available;
        for (const Comparison &comparison : history)
        {
            const QStringList paths{comparison.left, comparison.middle, comparison.right};
            bool exists = true;
            for (const QString &path : paths)
            {
                if (!path.isEmpty() && PathUtil::classify(path) == PathUtil::Kind::Missing)
                {
                    exists = false;
                    break;
                }
            }

            if (exists)
            {
                available << comparison;
            }
        }

        if (!self)
        {
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self, available, generation] {
                if (self && self->m_recentCheckGeneration == generation)
                {
                    self->rebuildRecentMenu(available);
                }
            },
            Qt::QueuedConnection);
    });
}

void MainWindow::restoreComparison(const Comparison &comparison)
{
    m_left->setPath(comparison.left);
    m_middle->setPath(comparison.middle);
    m_right->setPath(comparison.right);
    statusBar()->showMessage(tr("Recent comparison restored."), 3000);
}

void MainWindow::pasteFromClipboard(SlotWidget *target)
{
    if (!target)
    {
        return;
    }

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (mimeData->hasUrls())
    {
        QStringList files;
        for (const QUrl &url : mimeData->urls())
        {
            if (url.isLocalFile())
            {
                files << url.toLocalFile();
            }
        }

        if (!files.isEmpty())
        {
            setItems(files, target);
            return;
        }
    }

    const QString text = mimeData->text();
    QStringList paths;
    const QStringList lines =
        text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    if (!lines.isEmpty() && lines.size() <= 3)
    {
        bool allPaths = true;
        for (const QString &line : lines)
        {
            const QString path = PathUtil::normalizePath(line);
            if (QDir::isRelativePath(path) || PathUtil::classify(path) == PathUtil::Kind::Missing)
            {
                allPaths = false;
                break;
            }

            paths << path;
        }

        if (allPaths)
        {
            setItems(paths, target);
            return;
        }
    }

    storePastedText(target, text, PastedText::lineEndingFor(text));
}

void MainWindow::setItems(const QStringList &items, SlotWidget *target)
{
    if (items.isEmpty())
    {
        return;
    }

    if (items.size() > 3)
    {
        statusBar()->showMessage(tr("Choose at most three items."), 5000);
        return;
    }

    if (items.size() == 1)
    {
        (target ? target : m_left)->setPath(items.first());
        return;
    }

    m_left->setPath(items[0]);
    m_middle->setPath(items.size() == 3 ? items[1] : QString());
    m_right->setPath(items.last());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_meldDetection)
    {
        m_meldDetection->cancel();
    }

    m_settings.setGeometry(saveGeometry());
    m_settings.setCloseAfterLaunch(m_closeAfterLaunchAction->isChecked());
    QMainWindow::closeEvent(event);
}

void MainWindow::editText(SlotWidget *target)
{
    if (!target)
    {
        return;
    }

    const QString original = target->isPastedText() ? target->pastedText() : QString();
    const PastedText::LineEnding ending = target->isPastedText()
                                              ? target->pastedTextLineEnding()
                                              : PastedText::lineEndingFor(original);
    TextDialog dialog(original, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString text = PastedText::withLineEnding(dialog.text(), ending);
    storePastedText(target, text, ending);
}

void MainWindow::storePastedText(SlotWidget *target, const QString &text,
                                 PastedText::LineEnding lineEnding)
{
    PastedText::Slot slot = PastedText::Slot::Left;
    if (target == m_middle)
    {
        slot = PastedText::Slot::Middle;
    }
    else if (target == m_right)
    {
        slot = PastedText::Slot::Right;
    }

    const QString path =
        PastedText::save(PastedText::defaultDir(), slot, text, QDateTime::currentDateTime());
    if (path.isEmpty())
    {
        Log::error(QStringLiteral("Pasted text could not be saved"));
        QMessageBox::warning(this, tr("Could not save text"),
                             tr("The pasted text could not be saved."));
        return;
    }

    target->setPastedText(path, text, lineEnding);
}

void MainWindow::openPastedTextDir()
{
    const QString path = PastedText::defaultDir();
    if (!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    {
        Log::error(QStringLiteral("Pasted texts folder could not be opened"));
        QMessageBox::warning(this, tr("Could not open folder"),
                             tr("The pasted texts folder could not be opened."));
    }
}

void MainWindow::removeIntegration()
{
    const Integration::Result result = Integration::remove();
    if (!result.ok)
    {
        Log::error(result.message);
        QMessageBox::warning(this, tr("System integration"), result.message);
        return;
    }

    Log::info(result.message);
    m_settings.setIntegrationExePath(QString());
    refreshIntegrationNotice();
    QMessageBox::information(this, tr("System integration"), result.message);
}

void MainWindow::installIntegration()
{
    const QString executable = Integration::executablePath();
    const Integration::Result result = Integration::install(executable, executable);
    if (!result.ok)
    {
        Log::error(result.message);
        QMessageBox::warning(this, tr("System integration"), result.message);
        return;
    }

    Log::info(result.message);
    m_settings.setIntegrationExePath(executable);
    refreshIntegrationNotice();
    QMessageBox::information(this, tr("System integration"), result.message);
}

void MainWindow::refreshIntegrationNotice()
{
    const QString installedPath = m_settings.integrationExePath();
    const QString currentPath = Integration::executablePath();
#ifdef Q_OS_WIN
    constexpr auto pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr auto pathCaseSensitivity = Qt::CaseSensitive;
#endif  // Q_OS_WIN

    const bool integrationNeedsRepair =
        !installedPath.isEmpty() &&
        QDir::cleanPath(installedPath).compare(QDir::cleanPath(currentPath), pathCaseSensitivity) !=
            0;
    m_integrationNotice->setVisible(integrationNeedsRepair);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_settings.meldCommand(), m_settings.diagnosticLoggingEnabled(),
                          m_settings.fileName(), this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    m_settings.setMeldCommand(dialog.meldCommand());
    const bool logging = dialog.diagnosticLoggingEnabled();
    m_settings.setDiagnosticLoggingEnabled(logging);
    if (!logging)
    {
        Log::disable();
        return;
    }

    Log::initialize(PastedText::defaultDir());
    Log::info(QStringLiteral("Diagnostic logging enabled: version=%1 cmake=%2 gitTag=%3 gitHash=%4")
                  .arg(QLatin1String(BuildInfo::applicationVersion),
                       QLatin1String(BuildInfo::cmakeVersion), QLatin1String(BuildInfo::gitTag),
                       QLatin1String(BuildInfo::gitHash)));
}

QWidget *MainWindow::createBrowseLocationsRow(QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("browseLocationsRow"));
    row->setAccessibleName(tr("Browse locations"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    for (int index = 0; index < BrowseLocations::count; ++index)
    {
        auto *slotLayout = new QHBoxLayout;
        slotLayout->setSpacing(2);
        auto *location = new QPushButton(row);
        location->setObjectName(QStringLiteral("browseLocation%1").arg(index));
        auto *remove = new QPushButton(QStringLiteral("×"), row);
        remove->setObjectName(QStringLiteral("removeBrowseLocation%1").arg(index));
        remove->setFixedWidth(26);
        remove->setAccessibleName(tr("Remove browse location %1").arg(index + 1));
        remove->setToolTip(remove->accessibleName());
        slotLayout->addWidget(location, 1);
        slotLayout->addWidget(remove);
        rowLayout->addLayout(slotLayout, 1);
        m_browseLocationButtons[index] = {location, remove};

        connect(location, &QPushButton::clicked, this,
                [this, index](bool active) { handleBrowseLocationClick(index, active); });
        connect(remove, &QPushButton::clicked, this,
                [this, index] { removeBrowseLocation(index); });
    }

    refreshBrowseLocations(m_settings.browseLocations());
    return row;
}

void MainWindow::handleBrowseLocationClick(int index, bool active)
{
    BrowseLocations locations = m_settings.browseLocations();
    if (locations.paths[index].isEmpty())
    {
        const QString startDir =
            SlotWidget::startDirForPath({}, m_settings.lastDir(), locations.activePath());
        const QString folder =
            QFileDialog::getExistingDirectory(this, tr("Choose browse location"), startDir);
        if (folder.isEmpty())
        {
            return;
        }

        locations.paths[index] = folder;
    }
    else
    {
        locations.activeIndex = active ? index : -1;
    }

    m_settings.setBrowseLocations(locations);
    refreshBrowseLocations(locations);
}

void MainWindow::removeBrowseLocation(int index)
{
    BrowseLocations locations = m_settings.browseLocations();
    locations.paths[index].clear();
    if (locations.activeIndex == index)
    {
        locations.activeIndex = -1;
    }

    m_settings.setBrowseLocations(locations);
    refreshBrowseLocations(locations);
}

void MainWindow::refreshBrowseLocations(const BrowseLocations &locations)
{
    for (int index = 0; index < BrowseLocations::count; ++index)
    {
        const auto &buttons = m_browseLocationButtons[index];
        const QString &path = locations.paths[index];
        const bool saved = !path.isEmpty();
        const bool active = saved && locations.activeIndex == index;
        buttons.location->setCheckable(saved);
        buttons.location->setChecked(active);
        buttons.remove->setVisible(saved);
        if (!saved)
        {
            buttons.location->setText(QStringLiteral("+"));
            buttons.location->setAccessibleName(tr("Add browse location %1").arg(index + 1));
            buttons.location->setToolTip(buttons.location->accessibleName());
            continue;
        }

        const QString nativePath = QDir::toNativeSeparators(path);
        const QString folderName = QFileInfo(path).fileName();
        const QString label = folderName.isEmpty() ? nativePath : folderName;
        buttons.location->setText(
            buttons.location->fontMetrics().elidedText(label, Qt::ElideRight, 90));
        const QString action =
            active ? tr("Deactivate browse location") : tr("Activate browse location");
        buttons.location->setAccessibleName(tr("%1: %2").arg(action, nativePath));
        buttons.location->setToolTip(nativePath + QLatin1Char('\n') + action);
    }

    const QString preferredBrowseDir = locations.activePath();
    for (SlotWidget *slot : {m_left, m_middle, m_right})
    {
        slot->setPreferredBrowseDir(preferredBrowseDir);
    }
}

QStringList MainWindow::items() const
{
    QStringList result{m_left->path()};
    if (!m_middle->path().isEmpty())
    {
        result << m_middle->path();
    }

    result << m_right->path();
    return result;
}

void MainWindow::compare()
{
    if (m_meldDetection)
    {
        return;
    }

    const Comparison comparison{m_left->path(), m_middle->path(), m_right->path()};
    QStringList selected = items();
    const MeldLauncher::Validation validation = MeldLauncher::validate(selected);
    if (validation.problem != MeldLauncher::Problem::None)
    {
        showValidationError(validation);
        return;
    }

    QList<PathUtil::Kind> itemKinds;
    for (const QString &item : selected)
    {
        itemKinds << PathUtil::classify(item);
    }

    const bool hasMixedKinds =
        itemKinds.contains(PathUtil::Kind::File) && itemKinds.contains(PathUtil::Kind::Directory);

    QStringList launchArgs;
    if (selected.size() == 2 && hasMixedKinds)
    {
        const bool fixedOnLeft = itemKinds[0] == PathUtil::Kind::File;
        const QString fixedFile = selected[fixedOnLeft ? 0 : 1];
        const QString folder = selected[fixedOnLeft ? 1 : 0];
        QFileDialog dialog(this, tr("Choose files to compare"), folder);
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.setNameFilter(tr("All files (*)"));
        const QString fixedFileName = QFileInfo(fixedFile).fileName();
        if (QFileInfo(QDir(folder).filePath(fixedFileName)).isFile())
        {
            dialog.selectFile(fixedFileName);
        }

        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }

        const QStringList picked = dialog.selectedFiles();
        if (picked.size() > 10 &&
            QMessageBox::question(this, tr("Open many comparisons"),
                                  tr("Open %1 comparisons in Meld?").arg(picked.size())) !=
                QMessageBox::Yes)
        {
            return;
        }

        launchArgs = MeldLauncher::multiTabArgs(fixedFile, picked, fixedOnLeft);
        if (launchArgs.isEmpty())
        {
            return;
        }
    }
    else if (selected.size() == 3 && hasMixedKinds)
    {
        for (int i = 0; i < selected.size(); ++i)
        {
            if (itemKinds[i] != PathUtil::Kind::Directory)
            {
                continue;
            }

            const QString picked =
                QFileDialog::getOpenFileName(this, tr("Choose file"), selected[i]);
            if (picked.isEmpty())
            {
                return;
            }

            selected[i] = picked;
        }

        const MeldLauncher::Validation pickedValidation = MeldLauncher::validate(selected);
        if (pickedValidation.problem != MeldLauncher::Problem::None)
        {
            showValidationError(pickedValidation);
            return;
        }

        launchArgs = MeldLauncher::compareArgs(selected);
    }
    else
    {
        launchArgs = MeldLauncher::compareArgs(selected);
    }

    const QString customCommand = m_settings.meldCommand();
    auto promise = std::make_shared<QPromise<MeldLauncher::Command>>();
    promise->start();
    auto *detection = new QFutureWatcher<MeldLauncher::Command>(this);
    m_meldDetection = detection;
    m_compareButton->setEnabled(false);
    statusBar()->showMessage(tr("Looking for Meld…"));
    connect(detection, &QFutureWatcher<MeldLauncher::Command>::finished, this,
            [this, detection, launchArgs, comparison] {
                m_meldDetection = nullptr;
                m_compareButton->setEnabled(true);
                statusBar()->clearMessage();
                detection->deleteLater();
                if (!detection->isCanceled())
                {
                    launchComparison(detection->result(), launchArgs, comparison);
                }
            });
    detection->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise, customCommand] {
        promise->addResult(MeldLauncher::findMeld(customCommand));
        promise->finish();
    });
}

void MainWindow::launchComparison(const MeldLauncher::Command &meld, const QStringList &launchArgs,
                                  const Comparison &comparison)
{
    if (!meld.isValid())
    {
        Log::error(QStringLiteral("Compare stopped because Meld was not found"));
        QMessageBox::warning(this, tr("Meld not found"),
                             tr("Meld was not found. Configure its command in Settings."));
        return;
    }

    if (!MeldLauncher::launch(meld, launchArgs))
    {
        QMessageBox::warning(this, tr("Could not start Meld"), tr("Meld could not be started."));
        return;
    }

    m_settings.addToHistory(comparison);
    ++m_recentCheckGeneration;
    rebuildRecentMenu(m_settings.history());
    if (m_closeAfterLaunchAction->isChecked())
    {
        close();
    }
}

void MainWindow::showValidationError(const MeldLauncher::Validation &validation)
{
    QString message;
    switch (validation.problem)
    {
    case MeldLauncher::Problem::WrongCount:
        message = tr("Choose a left and a right item.");
        break;
    case MeldLauncher::Problem::Missing:
        message = tr("Item %1 does not exist.").arg(validation.index + 1);
        break;
    case MeldLauncher::Problem::Duplicate:
        message = tr("Item %1 is already used in another slot.").arg(validation.index + 1);
        break;
    case MeldLauncher::Problem::None:
        return;
    }

    Log::error(message);
    QMessageBox::warning(this, tr("Cannot compare"), message);
}
