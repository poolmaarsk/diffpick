#include "SettingsDialog.hpp"

#include "Log.hpp"
#include "MeldLauncher.hpp"
#include "PastedText.hpp"

#include <QDateTime>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThreadPool>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const QString &meldCommand, bool diagnosticLogging,
                               const QString &settingsFilePath, QWidget *parent)
    : QDialog(parent)
    , m_command(new QLineEdit(meldCommand, this))
    , m_detected(new QLabel(tr("Checking…"), this))
    , m_diagnosticLogging(new QCheckBox(tr("Enable diagnostic logging"), this))
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(620);
    m_command->setObjectName(QStringLiteral("meldCommandEdit"));
    m_detected->setObjectName(QStringLiteral("detectedMeldLabel"));
    m_detected->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagnosticLogging->setObjectName(QStringLiteral("diagnosticLoggingCheckBox"));
    m_diagnosticLogging->setChecked(diagnosticLogging);
    m_diagnosticLogging->setToolTip(
        tr("Enable temporarily while reproducing a problem. Pasted-text content is never logged."));

    auto *choose = new QPushButton(tr("Choose…"), this);
    auto *automatic = new QPushButton(tr("Auto-detect"), this);
    auto *commandRow = new QHBoxLayout;
    commandRow->addWidget(m_command, 1);
    commandRow->addWidget(choose);
    commandRow->addWidget(automatic);

    auto *textPath = new QLabel(QDir::toNativeSeparators(PastedText::defaultDir()), this);
    textPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *openTextDir = new QPushButton(tr("Open folder"), this);
    auto *deleteTexts = new QPushButton(tr("Delete now"), this);
    auto *textRow = new QHBoxLayout;
    textRow->addWidget(textPath, 1);
    textRow->addWidget(openTextDir);
    textRow->addWidget(deleteTexts);

    auto *settingsPath = new QLineEdit(QDir::toNativeSeparators(settingsFilePath), this);
    settingsPath->setObjectName(QStringLiteral("settingsFilePath"));
    settingsPath->setReadOnly(true);
    settingsPath->setCursorPosition(0);
    settingsPath->setToolTip(settingsPath->text());
    auto *openSettingsFolder = new QPushButton(tr("Open folder"), this);
    openSettingsFolder->setObjectName(QStringLiteral("openSettingsFolder"));
    openSettingsFolder->setAccessibleName(tr("Open settings folder"));
    auto *settingsRow = new QHBoxLayout;
    settingsRow->addWidget(settingsPath, 1);
    settingsRow->addWidget(openSettingsFolder);

    auto *form = new QFormLayout;
    form->addRow(tr("Meld command:"), commandRow);
    form->addRow(tr("Detected command:"), m_detected);
    form->addRow(tr("Pasted texts:"), textRow);
    form->addRow(tr("Settings file:"), settingsRow);
    form->addRow(tr("Diagnostics:"), m_diagnosticLogging);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(choose, &QPushButton::clicked, this, &SettingsDialog::chooseMeld);
    connect(automatic, &QPushButton::clicked, this, &SettingsDialog::autoDetect);
    connect(openTextDir, &QPushButton::clicked, this,
            [this] { openFolder(PastedText::defaultDir()); });
    const QString settingsFolder = QFileInfo(settingsFilePath).absolutePath();
    connect(openSettingsFolder, &QPushButton::clicked, this,
            [this, settingsFolder] { openFolder(settingsFolder); });
    connect(deleteTexts, &QPushButton::clicked, this, &SettingsDialog::deletePastedTexts);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_command, &QLineEdit::editingFinished, this, &SettingsDialog::refreshDetection);
    refreshDetection();
}

QString SettingsDialog::meldCommand() const
{
    return m_command->text().trimmed();
}

bool SettingsDialog::diagnosticLoggingEnabled() const
{
    return m_diagnosticLogging->isChecked();
}

void SettingsDialog::chooseMeld()
{
    const QString current = meldCommand();
    const QString start =
        QFileInfo(current).isFile() ? QFileInfo(current).absolutePath() : QDir::homePath();
    const QString selected =
        QFileDialog::getOpenFileName(this, tr("Choose Meld executable"), start);
    if (selected.isEmpty())
    {
        return;
    }

    QString command = QDir::toNativeSeparators(selected);
    if (command.contains(QLatin1Char(' ')))
    {
        command = QLatin1Char('"') + command + QLatin1Char('"');
    }

    m_command->setText(command);
    refreshDetection();
}

void SettingsDialog::autoDetect()
{
    m_command->clear();
    refreshDetection();
}

void SettingsDialog::refreshDetection()
{
    const QString command = meldCommand();
    const quint64 generation = ++m_detectionGeneration;
    m_detected->setText(tr("Checking…"));
    QPointer<SettingsDialog> self(this);
    QThreadPool::globalInstance()->start([self, command, generation] {
        const MeldLauncher::Command detected = MeldLauncher::findMeld(command);
        if (!self)
        {
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self, detected, generation] {
                if (!self || self->m_detectionGeneration != generation)
                {
                    return;
                }

                if (!detected.isValid())
                {
                    self->m_detected->setText(self->tr("Not found"));
                    self->m_detected->setStyleSheet(QStringLiteral("color: #b00020"));
                    return;
                }

                self->m_detected->setStyleSheet(QString());
                self->m_detected->setText(
                    QDir::toNativeSeparators(detected.program) +
                    (detected.args.isEmpty()
                         ? QString()
                         : QLatin1Char(' ') + detected.args.join(QLatin1Char(' '))));
            },
            Qt::QueuedConnection);
    });
}

void SettingsDialog::openFolder(const QString &path)
{
    if (!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    {
        Log::error(QStringLiteral("Could not open folder from Settings: %1").arg(path));
        QMessageBox::warning(this, tr("Could not open folder"),
                             tr("The folder could not be opened."));
    }
}

void SettingsDialog::deletePastedTexts()
{
    if (QMessageBox::question(this, tr("Delete pasted texts"),
                              tr("Delete all saved pasted texts now?")) != QMessageBox::Yes)
    {
        return;
    }

    const int removed = PastedText::removeOlderThan(PastedText::defaultDir(),
                                                    QDateTime::currentDateTime().addYears(100));
    Log::info(QStringLiteral("Deleted %1 pasted-text folders").arg(removed));
    QMessageBox::information(this, tr("Pasted texts deleted"),
                             tr("Deleted %1 pasted-text folders.").arg(removed));
}
