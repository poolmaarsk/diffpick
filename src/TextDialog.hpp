#pragma once

#include <QDialog>

class QPlainTextEdit;

class TextDialog : public QDialog
{
    Q_OBJECT

    public:
    explicit TextDialog(const QString &text, QWidget *parent = nullptr);

    QString text() const;

    private:
    QPlainTextEdit *m_editor;
};
