#include "TextDialog.hpp"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QVBoxLayout>

TextDialog::TextDialog(const QString &text, QWidget *parent)
    : QDialog(parent)
    , m_editor(new QPlainTextEdit(this))
{
    setWindowTitle(tr("Pasted text"));
    resize(700, 450);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlainText(text);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *applyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(applyShortcut, &QShortcut::activated, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_editor);
    layout->addWidget(buttons);
    m_editor->setFocus();
}

QString TextDialog::text() const
{
    return m_editor->toPlainText();
}
