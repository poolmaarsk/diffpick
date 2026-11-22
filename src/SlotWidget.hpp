#pragma once

#include "PathUtil.hpp"
#include "PastedText.hpp"

#include <QPoint>
#include <QRect>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QToolButton;

class SlotWidget : public QWidget
{
    Q_OBJECT

    public:
    explicit SlotWidget(const QString &title, QWidget *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    void setLastDir(const QString &dir);
    void setPreferredBrowseDir(const QString &dir);
    static QString startDirForPath(const QString &path, const QString &lastDir,
                                   const QString &preferredBrowseDir = {});
    void exchangeInputWith(SlotWidget *other);
    QRect editorGeometry() const;
    bool isPastedText() const;
    QString pastedText() const;
    PastedText::LineEnding pastedTextLineEnding() const;
    void setPastedText(const QString &path, const QString &text, PastedText::LineEnding lineEnding);
    void focusEditor();
    void showBrowseMenu();
    void setBrowseShortcutText(const QString &shortcut);

    signals:
    void itemsDropped(const QStringList &items, SlotWidget *target);
    void lastDirChanged(const QString &dir);
    void textRequested(SlotWidget *target);
    void clipboardRequested(SlotWidget *target);
    void plainTextDropped(const QString &text, SlotWidget *target);
    void openPastedTextDirRequested();

    protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    private:
    void normalize();
    void scheduleClassification();
    void classifyNow();
    void showKind(PathUtil::Kind kind);
    QString dialogStartDir() const;

    QLineEdit *m_editor;
    QLabel *m_titleLabel;
    QLabel *m_status;
    QPushButton *m_browseButton;
    QTimer *m_timer;
    QToolButton *m_dragHandle;
    QPoint m_dragStartPosition;
    QString m_lastDir;
    QString m_preferredBrowseDir;
    QString m_pastedText;
    PastedText::LineEnding m_pastedTextLineEnding = PastedText::LineEnding::Lf;
    bool m_isPastedText = false;
    quint64 m_generation = 0;
};
