// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

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
#include <QMessageBox>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class QLabel;
class QKeyEvent;
class QToolButton;

#ifdef NSL_HAS_LAYER_SHELL
namespace LayerShellQt {
class Window;
}
#endif

namespace nsl {

// Frameless top-level widget that owns panes, menus, tray, collector, and settings.
class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(bool simulate = false, QWidget* parent = nullptr, bool persistenceEnabled = true);
    ~MainWindow() override;
    bool autoMinimizeEnabled() const;
    bool trayAvailable() const;
    void populateScreenshotDemoData();
    void setPersistenceEnabled(bool enabled);
    void shutdownForSignal();

public Q_SLOTS:
    Q_SCRIPTABLE void activateFromInstanceRequest();
    Q_SCRIPTABLE void activateFromInstanceRequestWithToken(const QString& activationToken);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private Q_SLOTS:
    void updateFromCollector(const CollectorSnapshot& snapshot);
    void rebuildMenus();
    void resetStatistics();
    void minimizeRequested();
    void toggleVisibleFromTray();
    void toggleVisibleFromTrayWithToken(const QString& activationToken);
    void ensureTrayReachability();

private:
    void createPanes();
    void createMenus();
    void createSemanticControls();
    void updateSemanticControlGeometry();
    void applyPaneVisibility();
    void applyAlwaysOnTop();
    void configureLayerShell();
    void restoreFromTray();
    StorageResult reconcileLatestMonthlySnapshot(bool userVisible = true);
    StorageResult saveConfig(bool userVisible = true);
    StorageResult saveTotals(bool userVisible = true);
    void reportStorageError(const StorageResult& result, bool userVisible = true);
    QRect minimizeButtonRect() const;
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
    QLabel* titleLabel_ = nullptr;
    QToolButton* minimizeButton_ = nullptr;
    QPointer<QWidget> focusBeforeMinimize_;
#ifdef NSL_HAS_LAYER_SHELL
    QPointer<LayerShellQt::Window> layerShellWindow_;
#endif

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
    QTimer trayAvailabilityTimer_;
    QPointer<QMessageBox> storageErrorMessage_;
    QSet<QString> reportedStorageErrors_;
    StorageResult storageAuthorityError_;
    bool persistenceEnabled_ = true;
    bool storageAuthorityTrusted_ = true;
    bool shuttingDown_ = false;
};

} // namespace nsl
