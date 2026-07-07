#pragma once

#include "ClipCap.h"
#include "Collector.h"
#include "GraphPane.h"
#include "Settings.h"
#include "TextPane.h"
#include "TrayIcon.h"

#include <QAction>
#include <QActionGroup>
#include <QMap>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace nsl {

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(bool simulate = false, QWidget* parent = nullptr);
    ~MainWindow() override;
    bool autoMinimizeEnabled() const;
    void populateScreenshotDemoData();
    void setPersistenceEnabled(bool enabled);
    void shutdownForSignal();

public Q_SLOTS:
    Q_SCRIPTABLE void activateFromInstanceRequest();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private Q_SLOTS:
    void updateFromCollector(const CollectorSnapshot& snapshot);
    void rebuildMenus();
    void resetStatistics();
    void toggleVisibleFromTray();

private:
    void createPanes();
    void createMenus();
    void applyPaneVisibility();
    void applyAlwaysOnTop();
    void configureLayerShell();
    void saveConfig();
    void saveTotals();
    QString totalText(std::uint64_t bytes) const;
    QWidget* paneWidget(PaneId id) const;

    AppSettings settings_;
    AppConfig config_;
    Collector collector_;
    ClipCap clipCap_;
    TrayIcon tray_;
    QVBoxLayout* layout_ = nullptr;
    QMenu contextMenu_;
    QMenu statisticsMenu_;
    QMenu configMenu_;
    QMenu interfaceMenu_;
    QActionGroup unitGroup_;
    QActionGroup interfaceGroup_;
    QMap<PaneId, QAction*> paneActions_;

    TextPane* localPane_ = nullptr;
    TextPane* remotePane_ = nullptr;
    TextPane* incomingTotalsPane_ = nullptr;
    GraphPane* incomingPane_ = nullptr;
    TextPane* outgoingTotalsPane_ = nullptr;
    GraphPane* outgoingPane_ = nullptr;
    GraphPane* threadsPane_ = nullptr;
    GraphPane* cpuPane_ = nullptr;

    CollectorSnapshot latestSnapshot_;
    QTimer totalsFlushTimer_;
    bool persistenceEnabled_ = true;
};

} // namespace nsl
