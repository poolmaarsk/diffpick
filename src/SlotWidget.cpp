#include "SlotWidget.hpp"

#include <QCompleter>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVariant>

SlotWidget::SlotWidget(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_editor(new QLineEdit(this))
    , m_titleLabel(new QLabel(title, this))
    , m_status(new QLabel(tr("Not found"), this))
    , m_browseButton(new QPushButton(tr("Browse…"), this))
    , m_timer(new QTimer(this))
    , m_dragHandle(new QToolButton(this))
{
    setAcceptDrops(true);
    m_status->setObjectName(QStringLiteral("statusLabel"));

    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setMinimumWidth(70);
    m_titleLabel->setCursor(Qt::PointingHandCursor);
    m_titleLabel->setToolTip(tr("Open input menu"));
    m_titleLabel->installEventFilter(this);
    m_dragHandle->setText(QStringLiteral("☷"));
    m_dragHandle->setToolTip(tr("Drag this input to another slot"));
    m_dragHandle->setCursor(Qt::OpenHandCursor);
    m_dragHandle->installEventFilter(this);
    m_editor->setClearButtonEnabled(true);
    m_editor->installEventFilter(this);

    auto *model = new QFileSystemModel(this);
    model->setRootPath(QString());
    auto *completer = new QCompleter(model, this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    m_editor->setCompleter(completer);

    auto *menu = new QMenu(m_browseButton);
    menu->addAction(tr("&1. File…"), this, [this] {
        const QString selected =
            QFileDialog::getOpenFileName(this, tr("Choose file"), dialogStartDir());
        if (selected.isEmpty())
        {
            return;
        }

        setPath(selected);
        emit lastDirChanged(QFileInfo(selected).absolutePath());
    });
    menu->addAction(tr("&2. Folder…"), this, [this] {
        const QString selected =
            QFileDialog::getExistingDirectory(this, tr("Choose folder"), dialogStartDir());
        if (selected.isEmpty())
        {
            return;
        }

        setPath(selected);
        emit lastDirChanged(selected);
    });
    menu->addSeparator();
    menu->addAction(tr("&3. Text…"), this, [this] { emit textRequested(this); });
    menu->addAction(tr("&4. From clipboard"), this, [this] { emit clipboardRequested(this); });
    menu->addSeparator();
    menu->addAction(tr("&5. Open pasted texts folder"), this,
                    [this] { emit openPastedTextDirRequested(); });
    m_browseButton->setMenu(menu);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_dragHandle);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_editor, 1);
    layout->addWidget(m_status);
    layout->addWidget(m_browseButton);

    m_timer->setSingleShot(true);
    m_timer->setInterval(300);
    connect(m_timer, &QTimer::timeout, this, &SlotWidget::classifyNow);
    connect(m_editor, &QLineEdit::textChanged, this, &SlotWidget::scheduleClassification);
    connect(m_editor, &QLineEdit::textEdited, this, [this] {
        m_isPastedText = false;
        m_pastedText.clear();
    });
    connect(m_editor, &QLineEdit::editingFinished, this, &SlotWidget::normalize);
}

QString SlotWidget::path() const
{
    return PathUtil::normalizePath(m_editor->text());
}

void SlotWidget::setPath(const QString &path)
{
    m_isPastedText = false;
    m_pastedText.clear();
    m_editor->setText(QDir::toNativeSeparators(PathUtil::normalizePath(path)));
    classifyNow();
}

void SlotWidget::setLastDir(const QString &dir)
{
    m_lastDir = dir;
}

void SlotWidget::setPreferredBrowseDir(const QString &dir)
{
    m_preferredBrowseDir = dir;
}

void SlotWidget::exchangeInputWith(SlotWidget *other)
{
    if (!other || other == this)
    {
        return;
    }

    const QString ownPath = path();
    const QString ownText = m_pastedText;
    const PastedText::LineEnding ownEnding = m_pastedTextLineEnding;
    const bool ownIsText = m_isPastedText;
    const QString otherPath = other->path();
    const QString otherText = other->m_pastedText;
    const PastedText::LineEnding otherEnding = other->m_pastedTextLineEnding;
    const bool otherIsText = other->m_isPastedText;

    setPath(otherPath);
    if (otherIsText)
    {
        setPastedText(otherPath, otherText, otherEnding);
    }

    other->setPath(ownPath);
    if (ownIsText)
    {
        other->setPastedText(ownPath, ownText, ownEnding);
    }
}

QRect SlotWidget::editorGeometry() const
{
    return m_editor->geometry();
}

bool SlotWidget::isPastedText() const
{
    return m_isPastedText;
}

QString SlotWidget::pastedText() const
{
    return m_pastedText;
}

PastedText::LineEnding SlotWidget::pastedTextLineEnding() const
{
    return m_pastedTextLineEnding;
}

void SlotWidget::setPastedText(const QString &path, const QString &text,
                               PastedText::LineEnding lineEnding)
{
    setPath(path);
    m_isPastedText = true;
    m_pastedText = text;
    m_pastedTextLineEnding = lineEnding;
    m_status->setText(tr("Pasted text"));
    m_status->setStyleSheet(QString());
}

QString SlotWidget::startDirForPath(const QString &path, const QString &lastDir,
                                    const QString &preferredBrowseDir)
{
    const QFileInfo info(path);
    if (info.isDir())
    {
        return info.absoluteFilePath();
    }

    if (info.isFile())
    {
        return info.absolutePath();
    }

    if (QFileInfo(preferredBrowseDir).isDir())
    {
        return preferredBrowseDir;
    }

    if (QFileInfo(lastDir).isDir())
    {
        return lastDir;
    }

    return QDir::homePath();
}

void SlotWidget::focusEditor()
{
    m_editor->setFocus();
    m_editor->selectAll();
}

void SlotWidget::showBrowseMenu()
{
    // QPushButton::showMenu() runs a nested event loop on older Qt versions.
    m_browseButton->menu()->popup(m_browseButton->mapToGlobal(QPoint(0, m_browseButton->height())));
}

void SlotWidget::setBrowseShortcutText(const QString &shortcut)
{
    m_browseButton->setToolTip(tr("Open input menu (%1)").arg(shortcut));
}

bool SlotWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleLabel && event->type() == QEvent::MouseButtonRelease)
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            showBrowseMenu();
            return true;
        }
    }

    if (watched == m_editor && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Paste))
        {
            const QString clipboardText = QApplication::clipboard()->text();
            if (clipboardText.contains(QLatin1Char('\n')) ||
                clipboardText.contains(QLatin1Char('\r')))
            {
                emit clipboardRequested(this);
                return true;
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    if (watched != m_dragHandle)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            m_dragStartPosition = mouseEvent->position().toPoint();
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if ((mouseEvent->buttons() & Qt::LeftButton) && !path().isEmpty() &&
            (mouseEvent->position().toPoint() - m_dragStartPosition).manhattanLength() >=
                QApplication::startDragDistance())
        {
            auto *mimeData = new QMimeData;
            mimeData->setData("application/x-diffpick-slot", QByteArrayLiteral("1"));
            mimeData->setProperty("sourceSlot", QVariant::fromValue<QObject *>(this));
            auto *drag = new QDrag(this);
            drag->setMimeData(mimeData);
            m_dragHandle->setCursor(Qt::ClosedHandCursor);
            drag->exec(Qt::MoveAction);
            m_dragHandle->setCursor(Qt::OpenHandCursor);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SlotWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-diffpick-slot") ||
        event->mimeData()->hasUrls() || event->mimeData()->hasText())
    {
        event->acceptProposedAction();
    }
}

void SlotWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-diffpick-slot"))
    {
        auto *source = qobject_cast<SlotWidget *>(
            event->mimeData()->property("sourceSlot").value<QObject *>());
        exchangeInputWith(source);
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    QStringList items;
    for (const QUrl &url : event->mimeData()->urls())
    {
        if (url.isLocalFile())
        {
            items << url.toLocalFile();
        }
    }

    if (!items.isEmpty())
    {
        emit itemsDropped(items, this);
        event->acceptProposedAction();
        return;
    }

    if (!event->mimeData()->hasText())
    {
        return;
    }

    emit plainTextDropped(event->mimeData()->text(), this);
    event->acceptProposedAction();
}

void SlotWidget::normalize()
{
    const QString normalized = path();
    if (normalized != m_editor->text())
    {
        m_editor->setText(QDir::toNativeSeparators(normalized));
    }
}

void SlotWidget::scheduleClassification()
{
    ++m_generation;
    m_status->setText(tr("Checking…"));
    m_status->setStyleSheet(QString());
    m_timer->start();
}

void SlotWidget::classifyNow()
{
    m_timer->stop();
    const QString value = path();
    const quint64 generation = ++m_generation;
    QPointer<SlotWidget> self(this);
    QThreadPool::globalInstance()->start([self, value, generation] {
        const PathUtil::Kind kind = PathUtil::classify(value);
        if (!self)
        {
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self, kind, generation] {
                if (self && self->m_generation == generation)
                {
                    self->showKind(kind);
                }
            },
            Qt::QueuedConnection);
    });
}

void SlotWidget::showKind(PathUtil::Kind kind)
{
    if (m_isPastedText && kind == PathUtil::Kind::File)
    {
        m_status->setText(tr("Pasted text"));
        m_status->setStyleSheet(QString());
        return;
    }

    switch (kind)
    {
    case PathUtil::Kind::File:
        m_status->setText(tr("File"));
        m_status->setStyleSheet(QString());
        break;
    case PathUtil::Kind::Directory:
        m_status->setText(tr("Folder"));
        m_status->setStyleSheet(QString());
        break;
    case PathUtil::Kind::Missing:
        m_status->setText(tr("Not found"));
        m_status->setStyleSheet(path().isEmpty() ? QString() : QStringLiteral("color: #b00020"));
        break;
    }
}

QString SlotWidget::dialogStartDir() const
{
    return startDirForPath(path(), m_lastDir, m_preferredBrowseDir);
}
