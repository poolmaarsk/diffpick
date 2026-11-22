#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QCheckBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

    public:
    explicit SettingsDialog(const QString &meldCommand, bool diagnosticLogging,
                            const QString &settingsFilePath, QWidget *parent = nullptr);

    QString meldCommand() const;
    bool diagnosticLoggingEnabled() const;

    private:
    void chooseMeld();
    void autoDetect();
    void refreshDetection();
    void openFolder(const QString &path);
    void deletePastedTexts();

    QLineEdit *m_command;
    QLabel *m_detected;
    QCheckBox *m_diagnosticLogging;
    quint64 m_detectionGeneration = 0;
};
