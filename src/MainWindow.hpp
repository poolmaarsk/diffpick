#pragma once

#include "MeldLauncher.hpp"
#include "PastedText.hpp"
#include "Settings.hpp"

#include <QMainWindow>

#include <array>

class QAction;
template <typename T> class QFutureWatcher;
class QMenu;
class QPushButton;
class SlotWidget;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
    explicit MainWindow(QWidget *parent = nullptr);
    MainWindow(const QString &settingsFile, QWidget *parent);

    void setItems(const QStringList &items, SlotWidget *target = nullptr);
    void pasteFromClipboard(SlotWidget *target);
    void compareNow();

    protected:
    void closeEvent(QCloseEvent *event) override;

    private:
    void compare();
    void launchComparison(const MeldLauncher::Command &meld, const QStringList &launchArgs,
                          const Comparison &comparison);
    QStringList items() const;
    void showValidationError(const MeldLauncher::Validation &validation);
    void editText(SlotWidget *target);
    void storePastedText(SlotWidget *target, const QString &text,
                         PastedText::LineEnding lineEnding);
    void openPastedTextDir();
    void removeIntegration();
    void installIntegration();
    void refreshIntegrationNotice();
    void openSettings();
    QWidget *createBrowseLocationsRow(QWidget *parent);
    void handleBrowseLocationClick(int index, bool active);
    void removeBrowseLocation(int index);
    void refreshBrowseLocations(const BrowseLocations &locations);
    void rebuildRecentMenu(const QList<Comparison> &history);
    void checkRecentInBackground();
    void restoreComparison(const Comparison &comparison);

    SlotWidget *m_left;
    SlotWidget *m_middle;
    SlotWidget *m_right;
    QPushButton *m_swapButton;
    QPushButton *m_compareButton;
    QMenu *m_recentMenu;
    QAction *m_closeAfterLaunchAction;
    QWidget *m_integrationNotice;
    Settings m_settings;
    struct BrowseLocationButtons
    {
        QPushButton *location;
        QPushButton *remove;
    };
    std::array<BrowseLocationButtons, BrowseLocations::count> m_browseLocationButtons{};
    QFutureWatcher<MeldLauncher::Command> *m_meldDetection = nullptr;
    quint64 m_recentCheckGeneration = 0;
};
