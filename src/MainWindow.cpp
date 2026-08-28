// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "MainWindow.h"

#include "Lifecycle.h"
#include "Theme.h"

#ifdef NSL_HAS_KWINDOWSYSTEM
#include <KWindowSystem>
#endif

#ifdef NSL_HAS_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSignalBlocker>
#include <QToolButton>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>

namespace nsl {
namespace {

constexpr int WindowWidth = 238;
constexpr int TitleHeight = 18;
constexpr int MinimizeButtonWidth = 18;

std::size_t paneIndex(PaneId id) {
    return static_cast<std::size_t>(static_cast<int>(id));
}

QString pingText(const CollectorSnapshot& snapshot) {
    if (!snapshot.pingValid) {
        return QStringLiteral("n/a");
    }
    return QStringLiteral("%1 ms").arg(snapshot.averagePingMs, 0, 'f', 1);
}

QString hopText(const CollectorSnapshot& snapshot) {
    return snapshot.hopValid ? QString::number(snapshot.hopCount) : QStringLiteral("n/a");
}

} // namespace

MainWindow::MainWindow(bool simulate, QWidget* parent, bool persistenceEnabled)
    : QWidget(parent), settings_(persistenceEnabled), contextMenu_(this), statisticsMenu_(tr("Statistics"), this),
      configMenu_(tr("Config"), this), interfaceMenu_(tr("Interface"), this), unitGroup_(this),
      interfaceGroup_(this), persistenceEnabled_(persistenceEnabled) {
    setObjectName(QStringLiteral("netstats-live"));
    setWindowTitle(QStringLiteral("NetStats-Live"));
    setAccessibleName(QStringLiteral("NetStats-Live network monitor"));
    setAccessibleDescription(tr("Live network throughput, transfer totals, thread count, and CPU activity"));
    setFocusPolicy(Qt::StrongFocus);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFixedWidth(WindowWidth);

    StorageResult loadStorage;
    if (persistenceEnabled_) {
        const AppConfigLoadResult loaded = settings_.load();
        config_ = loaded.config;
        loadStorage = loaded.storage;
        storageAuthorityTrusted_ = settings_.storageAuthorityResult().ok && loaded.storage.ok;
        storageAuthorityError_ = settings_.storageAuthorityResult();
        if (!loaded.storage.ok) {
            if (!storageAuthorityError_.error.isEmpty()) {
                storageAuthorityError_.error += QLatin1Char('\n');
            }
            storageAuthorityError_.ok = false;
            storageAuthorityError_.error += loaded.storage.error;
        }
    } else {
        config_.panes.fill(true);
        config_.monthKey = AppSettings::currentMonthKey();
    }
    createPanes();
    createMenus();
    createSemanticControls();
    applyPaneVisibility();
    applyAlwaysOnTop();
    reportStorageError(settings_.initializationResult());
    reportStorageError(loadStorage);

    tray_.setContextMenu(&contextMenu_);
    connect(&tray_, &TrayIcon::toggleRequested,
            this, &MainWindow::toggleVisibleFromTrayWithToken);
    connect(&collector_, &Collector::updated, this, &MainWindow::updateFromCollector);
    connect(&collector_, &Collector::interfaceFallbackToAll, this, [this]() {
        config_.selectedInterface = QStringLiteral("ALL");
        saveConfig();
    });
    connect(&clipCap_, &ClipCap::urlCaptured, this, [this](const QString& host) {
        config_.remoteTarget = host;
        collector_.setRemoteTarget(host);
        saveConfig();
    });

    if (persistenceEnabled_ && storageAuthorityTrusted_) {
        totalsFlushTimer_.setInterval(60000);
        connect(&totalsFlushTimer_, &QTimer::timeout, this, [this]() {
            saveTotals();
        });
        totalsFlushTimer_.start();
    }
    trayAvailabilityTimer_.setInterval(2000);
    connect(&trayAvailabilityTimer_, &QTimer::timeout, this, &MainWindow::ensureTrayReachability);
    trayAvailabilityTimer_.start();

    clipCap_.setEnabled(config_.urlClipCap && !simulate);
    if (simulate) {
        collector_.startSimulation(config_.monthKey, config_.rxMonth, config_.txMonth);
    } else {
        collector_.start(config_.selectedInterface, config_.remoteTarget, config_.monthKey, config_.rxMonth, config_.txMonth);
    }

    if (!config_.windowPos.isNull()) {
        QList<QRect> screenGeometries;
        for (QScreen* screen : QGuiApplication::screens()) {
            screenGeometries.append(screen->availableGeometry());
        }
        move(visibleWindowPosition(config_.windowPos, size(), screenGeometries));
    }
}

MainWindow::~MainWindow() {
    if (persistenceEnabled_) {
        shuttingDown_ = true;
        saveTotals(false);
        saveConfig(false);
    }
}

bool MainWindow::autoMinimizeEnabled() const {
    return config_.autoMinimize;
}

bool MainWindow::trayAvailable() const {
    return tray_.isAvailable();
}

void MainWindow::setPersistenceEnabled(bool enabled) {
    persistenceEnabled_ = enabled;
}

void MainWindow::shutdownForSignal() {
    if (persistenceEnabled_) {
        shuttingDown_ = true;
        saveTotals(false);
        saveConfig(false);
    }
    QCoreApplication::quit();
}

void MainWindow::populateScreenshotDemoData() {
    disconnect(&collector_, nullptr, this, nullptr);
    totalsFlushTimer_.stop();
    config_.unitMode = UnitMode::Bytes;
    config_.panes[paneIndex(PaneId::Threads)] = false;
    incomingPane_->setUnitMode(config_.unitMode);
    outgoingPane_->setUnitMode(config_.unitMode);

    localPane_->setRows({{tr("Name"), tr("Local Machine")}, {tr("IP"), tr("x.x.x.x")}, {tr("Device"), tr("All TCP/IP Devices")}});
    remotePane_->setRows({{tr("Name"), tr("Disabled")}, {tr("IP"), tr("--")}, {tr("Ping"), tr("--")}});
    constexpr double MiB = 1024.0 * 1024.0;
    const auto bytesFromMiB = [](double value) {
        return static_cast<std::uint64_t>(value * MiB);
    };
    incomingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(bytesFromMiB(344873.0))},
                                     {tr("This Month"), totalText(bytesFromMiB(367601.4))},
                                     {tr("Last Month"), totalText(0)}});
    outgoingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(bytesFromMiB(185440.4))},
                                     {tr("This Month"), totalText(bytesFromMiB(187633.1))},
                                     {tr("Last Month"), totalText(0)}});

    incomingPane_->resetGraph();
    outgoingPane_->resetGraph();
    threadsPane_->resetGraph();
    cpuPane_->resetGraph();

    constexpr double kb = 1024.0;
    const std::array<double, 60> incoming = {
        0, 0, 0, 0, 12 * kb, 70 * kb, 18 * kb, 5 * kb, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 4 * kb, 10 * kb, 18 * kb, 40 * kb,
        86 * kb, 132 * kb, 188 * kb, 236 * kb, 285.7 * kb, 168 * kb, 112 * kb, 145 * kb, 82 * kb, 110 * kb,
        44 * kb, 16 * kb, 8 * kb, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::array<double, 60> outgoing = {
        0, 0, 0, 0, 7 * kb, 80 * kb, 20 * kb, 6 * kb, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 2 * kb, 5 * kb, 12 * kb, 21 * kb,
        44 * kb, 62 * kb, 80 * kb, 72 * kb, 38 * kb, 12 * kb, 50 * kb, 70 * kb, 0, 0,
        8 * kb, 1 * kb, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 63};
    const std::array<double, 60> cpu = {
        2, 8, 18, 6, 24, 8, 11, 7, 5, 4,
        5, 4, 5, 4, 4, 4, 6, 3, 7, 4,
        9, 3, 5, 4, 5, 4, 4, 6, 5, 8,
        12, 16, 40, 28, 57, 54, 23, 39, 38, 17,
        12, 12, 10, 10, 8, 7, 6, 6, 6, 10,
        6, 4, 5, 6, 20, 43, 5, 16, 8, 2};

    for (double value : incoming) {
        incomingPane_->pushSample(value);
    }
    for (double value : outgoing) {
        outgoingPane_->pushSample(value);
    }
    for (double value : cpu) {
        cpuPane_->pushSample(value);
    }
    for (int i = 0; i < 60; ++i) {
        threadsPane_->pushSample(1360 + ((i * 17) % 80));
    }
    applyPaneVisibility();
    tray_.updateFromSnapshot(latestSnapshot_);
}

void MainWindow::createPanes() {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(1, TitleHeight, 1, 1);
    layout_->setSpacing(0);
    layout_->setSizeConstraint(QLayout::SetNoConstraint);

    localPane_ = new TextPane(QStringLiteral("Local"), 67, this);
    remotePane_ = new TextPane(QStringLiteral("Remote"), 67, this);
    incomingTotalsPane_ = new TextPane(QStringLiteral("Incoming Totals"), 47, this);
    incomingPane_ = new GraphPane(QStringLiteral("Incoming"), GraphValueMode::NetworkRate, this);
    outgoingTotalsPane_ = new TextPane(QStringLiteral("Outgoing Totals"), 47, this);
    outgoingPane_ = new GraphPane(QStringLiteral("Outgoing"), GraphValueMode::NetworkRate, this);
    threadsPane_ = new GraphPane(QStringLiteral("Threads"), GraphValueMode::Count, this);
    cpuPane_ = new GraphPane(QStringLiteral("CPU"), GraphValueMode::Percent, this);

    for (QWidget* pane : {paneWidget(PaneId::LocalMachine), paneWidget(PaneId::RemoteMachine), paneWidget(PaneId::IncomingTotals),
                          paneWidget(PaneId::Incoming), paneWidget(PaneId::OutgoingTotals), paneWidget(PaneId::Outgoing),
                          paneWidget(PaneId::Threads), paneWidget(PaneId::Cpu)}) {
        pane->setFixedWidth(WindowWidth - 2);
        layout_->addWidget(pane);
    }
    incomingPane_->setUnitMode(config_.unitMode);
    outgoingPane_->setUnitMode(config_.unitMode);
}

void MainWindow::createSemanticControls() {
    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("title-label"));
    titleLabel_->setAccessibleName(QStringLiteral("NetStats-Live"));
    titleLabel_->setFocusPolicy(Qt::NoFocus);
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel_->setStyleSheet(QStringLiteral("background: transparent;"));

    minimizeButton_ = new QToolButton(this);
    minimizeButton_->setObjectName(QStringLiteral("minimize-button"));
    minimizeButton_->setAccessibleName(tr("Minimize NetStats-Live"));
    minimizeButton_->setFocusPolicy(Qt::StrongFocus);
    minimizeButton_->setCursor(Qt::PointingHandCursor);
    minimizeButton_->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; }"
        "QToolButton:focus { border: 1px solid #00a8a8; }"));
    connect(minimizeButton_, &QToolButton::clicked, this, &MainWindow::minimizeRequested);
    updateSemanticControlGeometry();
}

void MainWindow::updateSemanticControlGeometry() {
    if (titleLabel_ != nullptr) {
        titleLabel_->setGeometry(QRect(6, 0, width() - MinimizeButtonWidth - 10, TitleHeight));
        titleLabel_->raise();
    }
    if (minimizeButton_ != nullptr) {
        minimizeButton_->setGeometry(minimizeButtonRect());
        minimizeButton_->raise();
    }
}

void MainWindow::createMenus() {
    contextMenu_.setObjectName(QStringLiteral("main-context-menu"));
    contextMenu_.clear();
    statisticsMenu_.clear();
    configMenu_.clear();
    interfaceMenu_.clear();
    paneActions_.clear();

    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        QAction* action = statisticsMenu_.addAction(paneDisplayName(id));
        action->setCheckable(true);
        action->setChecked(config_.panes[paneIndex(id)]);
        paneActions_.insert(id, action);
        connect(action, &QAction::toggled, this, [this, id](bool checked) {
            config_.panes[paneIndex(id)] = checked;
            applyPaneVisibility();
            saveConfig();
        });
    }

    QAction* autoMinimize = configMenu_.addAction(tr("Auto Minimize"));
    autoMinimize->setCheckable(true);
    autoMinimize->setChecked(config_.autoMinimize);
    connect(autoMinimize, &QAction::toggled, this, [this](bool checked) {
        config_.autoMinimize = checked;
        saveConfig();
    });

    QAction* autoStart = configMenu_.addAction(tr("Auto Start"));
    autoStart->setCheckable(true);
    autoStart->setChecked(config_.autoStart);
    connect(autoStart, &QAction::toggled, this, [this, autoStart](bool checked) {
        if (!storageAuthorityTrusted_) {
            const QSignalBlocker blocker(autoStart);
            autoStart->setChecked(config_.autoStart);
            reportStorageError(storageAuthorityError_);
            return;
        }
        const QString executablePath = QCoreApplication::applicationFilePath();
        AutoStartSnapshot priorAutoStart;
        const StorageResult snapshotResult = settings_.snapshotAutoStart(priorAutoStart);
        if (!snapshotResult.ok) {
            const QSignalBlocker blocker(autoStart);
            autoStart->setChecked(config_.autoStart);
            reportStorageError(snapshotResult);
            return;
        }
        const StorageResult autostartResult = settings_.setAutoStart(checked, executablePath, priorAutoStart);
        if (!autostartResult.ok) {
            const StorageResult rollbackResult = settings_.restoreAutoStart(priorAutoStart);
            const QSignalBlocker blocker(autoStart);
            autoStart->setChecked(config_.autoStart);
            reportStorageError(autostartResult);
            reportStorageError(rollbackResult);
            return;
        }

        AppConfig updatedConfig = config_;
        updatedConfig.autoStart = checked;
        updatedConfig.windowPos = pos();
        const StorageResult configResult = settings_.saveConfig(updatedConfig);
        if (!configResult.ok) {
            const StorageResult rollbackResult = settings_.restoreAutoStart(priorAutoStart);
            const QSignalBlocker blocker(autoStart);
            autoStart->setChecked(config_.autoStart);
            reportStorageError(configResult);
            reportStorageError(rollbackResult);
            return;
        }
        const StorageResult commitResult = settings_.commitAutoStart(priorAutoStart);
        if (!commitResult.ok) {
            const StorageResult configRollback = settings_.saveConfig(config_);
            const StorageResult autostartRollback = settings_.restoreAutoStart(priorAutoStart);
            const QSignalBlocker blocker(autoStart);
            autoStart->setChecked(config_.autoStart);
            reportStorageError(commitResult);
            reportStorageError(configRollback);
            reportStorageError(autostartRollback);
            return;
        }
        config_ = updatedConfig;
    });

    QAction* clipCap = configMenu_.addAction(tr("URL ClipCap"));
    clipCap->setCheckable(true);
    clipCap->setChecked(config_.urlClipCap);
    connect(clipCap, &QAction::toggled, this, [this](bool checked) {
        config_.urlClipCap = checked;
        clipCap_.setEnabled(checked);
        saveConfig();
    });

    QAction* top = configMenu_.addAction(tr("Always on Top"));
    top->setCheckable(true);
    top->setChecked(config_.alwaysOnTop);
    connect(top, &QAction::toggled, this, [this](bool checked) {
        config_.alwaysOnTop = checked;
        applyAlwaysOnTop();
        saveConfig();
    });

    configMenu_.addSeparator();
    unitGroup_.setExclusive(true);
    QAction* bytes = configMenu_.addAction(tr("Display in Bytes"));
    bytes->setCheckable(true);
    bytes->setActionGroup(&unitGroup_);
    bytes->setChecked(config_.unitMode == UnitMode::Bytes);
    connect(bytes, &QAction::triggered, this, [this]() {
        config_.unitMode = UnitMode::Bytes;
        incomingPane_->setUnitMode(config_.unitMode);
        outgoingPane_->setUnitMode(config_.unitMode);
        saveConfig();
    });
    QAction* bits = configMenu_.addAction(tr("Display in Bits"));
    bits->setCheckable(true);
    bits->setActionGroup(&unitGroup_);
    bits->setChecked(config_.unitMode == UnitMode::Bits);
    connect(bits, &QAction::triggered, this, [this]() {
        config_.unitMode = UnitMode::Bits;
        incomingPane_->setUnitMode(config_.unitMode);
        outgoingPane_->setUnitMode(config_.unitMode);
        saveConfig();
    });

    configMenu_.addMenu(&interfaceMenu_);
    connect(&contextMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildMenus);

    contextMenu_.addMenu(&statisticsMenu_);
    contextMenu_.addMenu(&configMenu_);
    contextMenu_.addSeparator();
    contextMenu_.addAction(tr("Reset"), this, &MainWindow::resetStatistics);
    contextMenu_.addAction(tr("Minimize"), this, &MainWindow::minimizeRequested);
    contextMenu_.addAction(tr("Exit"), qApp, &QApplication::quit);
}

void MainWindow::rebuildMenus() {
    interfaceMenu_.clear();
    interfaceGroup_.setExclusive(true);
    auto addInterface = [this](const QString& name) {
        QAction* action = interfaceMenu_.addAction(name);
        action->setCheckable(true);
        action->setActionGroup(&interfaceGroup_);
        action->setChecked(config_.selectedInterface == name);
        connect(action, &QAction::triggered, this, [this, name]() {
            config_.selectedInterface = name;
            collector_.setSelectedInterface(name);
            saveConfig();
        });
    };
    addInterface(QStringLiteral("ALL"));
    for (const QString& iface : latestSnapshot_.interfaces) {
        addInterface(iface);
    }
}

void MainWindow::applyPaneVisibility() {
    int height = TitleHeight + 1;
    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        QWidget* pane = paneWidget(id);
        const bool visible = config_.panes[paneIndex(id)];
        pane->setVisible(visible);
        if (visible) {
            height += pane->height();
        }
        if (auto it = paneActions_.find(id); it != paneActions_.end()) {
            const QSignalBlocker blocker(it.value());
            it.value()->setChecked(visible);
        }
    }
    setFixedSize(WindowWidth, height);
    updateSemanticControlGeometry();
    configureLayerShell();
    updateGeometry();
    update();
}

void MainWindow::applyAlwaysOnTop() {
    // On Wayland, Qt window flags and layer-shell state are tied to the native
    // surface; destroy/recreate establishes a new shell-role authority.
    const bool wasVisible = isVisible();
    const bool wasMinimized = isMinimized();
    const QPoint priorPosition = pos();
    if (wasVisible) {
        hide();
    }
#ifdef NSL_HAS_LAYER_SHELL
    if (!layerShellWindow_.isNull()) {
        delete layerShellWindow_.data();
        layerShellWindow_.clear();
    }
#endif
    if (QWindow* handle = windowHandle()) {
        handle->destroy();
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, config_.alwaysOnTop);
    if (wasVisible) {
        if (wasMinimized) {
            showMinimized();
        } else {
            show();
        }
        move(priorPosition);
        if (config_.alwaysOnTop) {
            raise();
        }
    }
    configureLayerShell();
}

void MainWindow::configureLayerShell() {
#ifdef NSL_HAS_LAYER_SHELL
    // Layer-shell gives KDE Wayland a stronger always-on-top/overlay path; other
    // desktops fall back to Qt::WindowStaysOnTopHint above.
    if (config_.alwaysOnTop && windowHandle() != nullptr) {
        if (layerShellWindow_.isNull()) {
            layerShellWindow_ = LayerShellQt::Window::get(windowHandle());
        }
        if (auto* layerWindow = layerShellWindow_.data()) {
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorTop);
            anchors.setFlag(LayerShellQt::Window::AnchorLeft);
            layerWindow->setAnchors(anchors);
            layerWindow->setDesiredSize(size());
            layerWindow->setExclusiveZone(0);
            if (QScreen* screen = windowHandle()->screen()) {
                const QRect screenGeometry = screen->geometry();
                const int left = std::max(0, pos().x() - screenGeometry.left());
                const int top = std::max(0, pos().y() - screenGeometry.top());
                layerWindow->setMargins(QMargins(left, top, 0, 0));
            }
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            layerWindow->setScope(QStringLiteral("netstats-live"));
        }
    }
#endif
}

void MainWindow::updateFromCollector(const CollectorSnapshot& snapshot) {
    latestSnapshot_ = snapshot;
    reconcileLatestMonthlySnapshot();
    config_.remoteTarget = snapshot.remoteTarget == QStringLiteral("n/a") ? QString() : snapshot.remoteTarget;

    localPane_->setRows({{tr("Name"), snapshot.hostname}, {tr("IP"), snapshot.ipAddress}, {tr("Device"), snapshot.selectedInterface}});
    remotePane_->setRows({{tr("Name"), snapshot.remoteTarget}, {tr("Ping"), pingText(snapshot)}, {tr("Hops"), hopText(snapshot)}});
    incomingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(snapshot.rxSession)}, {tr("This Month"), totalText(snapshot.rxMonth)}, {tr("Last Month"), totalText(config_.lastRxMonth)}});
    outgoingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(snapshot.txSession)}, {tr("This Month"), totalText(snapshot.txMonth)}, {tr("Last Month"), totalText(config_.lastTxMonth)}});

    incomingPane_->pushSample(snapshot.rxRate);
    outgoingPane_->pushSample(snapshot.txRate);
    threadsPane_->pushSample(static_cast<double>(snapshot.threadTotal));
    cpuPane_->pushSample(snapshot.cpuPercent);
    tray_.updateFromSnapshot(snapshot);
}

void MainWindow::resetStatistics() {
    incomingPane_->resetGraph();
    outgoingPane_->resetGraph();
    threadsPane_->resetGraph();
    cpuPane_->resetGraph();
    collector_.resetSessionTotals();
}

void MainWindow::minimizeRequested() {
    focusBeforeMinimize_ = QApplication::focusWidget();
    if (persistenceEnabled_) {
        saveConfig();
    }
    showMinimized();
}

void MainWindow::toggleVisibleFromTray() {
    toggleVisibleFromTrayWithToken(QString());
}

void MainWindow::toggleVisibleFromTrayWithToken(const QString& activationToken) {
    const bool exposed = windowHandle() != nullptr && windowHandle()->isExposed();
    const TrayToggleAction action = trayToggleAction(isVisible(), isMinimized(), exposed);
    withActivationToken(activationToken, [this, action]() {
        switch (action) {
        case TrayToggleAction::Minimize:
            minimizeRequested();
            break;
        case TrayToggleAction::Restore:
            restoreFromTray();
            break;
        case TrayToggleAction::PlatformRestorePending:
#ifdef NSL_HAS_KWINDOWSYSTEM
            // Plasma clears Qt's minimized bit before emitting tray activation,
            // while KWin still owns a minimized surface. The KDE API restores that
            // surface without remapping it, so its compositor position is retained.
            KWindowSystem::activateWindow(windowHandle());
#else
            // Generic Qt cannot reverse the Wayland state desynchronization. Remap
            // as a reachability fallback; the compositor may choose a new position.
            hide();
            restoreFromTray();
#endif
            break;
        }
    });
}

void MainWindow::ensureTrayReachability() {
    const bool exposed = windowHandle() != nullptr && windowHandle()->isExposed();
    if (shouldRestoreForTrayLoss(tray_.isAvailable(), isVisible(), isMinimized(), exposed)) {
        hide();
        showNormal();
        raise();
        activateWindow();
    }
}

void MainWindow::activateFromInstanceRequest() {
    activateFromInstanceRequestWithToken(QString());
}

void MainWindow::activateFromInstanceRequestWithToken(const QString& activationToken) {
    const bool exposed = windowHandle() != nullptr && windowHandle()->isExposed();
#ifdef NSL_HAS_KWINDOWSYSTEM
    constexpr bool NativeActivationAvailable = true;
#else
    constexpr bool NativeActivationAvailable = false;
#endif

    const bool hasActivationToken = !activationToken.isEmpty();
    const InstanceActivationAction action = instanceActivationAction(
        exposed, NativeActivationAvailable, hasActivationToken);
    withActivationToken(activationToken, [this, action]() {
        if (action == InstanceActivationAction::Remap) {
            // A tokenless DBus/CLI request cannot unminimize a desynchronized
            // Wayland surface. Remapping is the reliable reachability fallback.
            hide();
            showNormal();
            raise();
            activateWindow();
            return;
        }
        restoreFromTray();
    });
}

void MainWindow::restoreFromTray() {
    showNormal();
#ifdef NSL_HAS_KWINDOWSYSTEM
    KWindowSystem::activateWindow(windowHandle());
#else
    raise();
    activateWindow();
#endif
    QTimer::singleShot(0, this, [this]() {
        QWidget* target = focusBeforeMinimize_.data();
        if (target != nullptr && target->isEnabled() && target->focusPolicy() != Qt::NoFocus) {
            target->setFocus(Qt::ActiveWindowFocusReason);
        } else {
            setFocus(Qt::ActiveWindowFocusReason);
        }
    });
}

StorageResult MainWindow::saveConfig(bool userVisible) {
    if (!persistenceEnabled_) {
        return {};
    }
    if (!storageAuthorityTrusted_) {
        reportStorageError(storageAuthorityError_, userVisible);
        return storageAuthorityError_;
    }
    config_.windowPos = pos();
    const StorageResult result = settings_.saveConfig(config_);
    reportStorageError(result, userVisible);
    return result;
}

StorageResult MainWindow::reconcileLatestMonthlySnapshot(bool userVisible) {
    if (!persistenceEnabled_ || !storageAuthorityTrusted_ || latestSnapshot_.monthKey.isEmpty()) {
        return storageAuthorityTrusted_ ? StorageResult{} : storageAuthorityError_;
    }
    if (!config_.monthKey.isEmpty() && config_.monthKey != latestSnapshot_.monthKey) {
        const StorageResult rolloverResult =
            settings_.saveMonthlyTotals(config_.monthKey, config_.rxMonth, config_.txMonth);
        reportStorageError(rolloverResult, userVisible);
        if (!rolloverResult.ok) {
            return rolloverResult;
        }
        config_.lastRxMonth = config_.rxMonth;
        config_.lastTxMonth = config_.txMonth;
    }
    config_.rxMonth = latestSnapshot_.rxMonth;
    config_.txMonth = latestSnapshot_.txMonth;
    config_.monthKey = latestSnapshot_.monthKey;
    return {};
}

StorageResult MainWindow::saveTotals(bool userVisible) {
    if (!persistenceEnabled_) {
        return {};
    }
    if (!storageAuthorityTrusted_) {
        reportStorageError(storageAuthorityError_, userVisible);
        return storageAuthorityError_;
    }
    const StorageResult reconciliationResult = reconcileLatestMonthlySnapshot(userVisible);
    if (!reconciliationResult.ok) {
        return reconciliationResult;
    }
    const StorageResult result = settings_.saveMonthlyTotals(config_.monthKey, config_.rxMonth, config_.txMonth);
    reportStorageError(result, userVisible);
    return result;
}

void MainWindow::reportStorageError(const StorageResult& result, bool userVisible) {
    if (result.ok || result.error.isEmpty()) {
        return;
    }
    qWarning().noquote() << result.error;
    if (!userVisible || shuttingDown_ || reportedStorageErrors_.contains(result.error)) {
        return;
    }
    reportedStorageErrors_.insert(result.error);
    if (storageErrorMessage_.isNull()) {
        storageErrorMessage_ = new QMessageBox(QMessageBox::Warning,
                                               tr("NetStats-Live could not save a setting"),
                                               result.error,
                                               QMessageBox::Ok,
                                               this);
        storageErrorMessage_->setObjectName(QStringLiteral("storage-error-message"));
        storageErrorMessage_->setWindowModality(Qt::NonModal);
        storageErrorMessage_->setAttribute(Qt::WA_DeleteOnClose);
    } else {
        storageErrorMessage_->setText(result.error);
    }
    storageErrorMessage_->show();
}

QRect MainWindow::minimizeButtonRect() const {
    return QRect(width() - MinimizeButtonWidth, 0, MinimizeButtonWidth, TitleHeight);
}

QString MainWindow::totalText(std::uint64_t bytes) const {
    return QString::fromStdString(formatRate(static_cast<double>(bytes), config_.unitMode));
}

QWidget* MainWindow::paneWidget(PaneId id) const {
    switch (id) {
    case PaneId::LocalMachine: return localPane_;
    case PaneId::RemoteMachine: return remotePane_;
    case PaneId::IncomingTotals: return incomingTotalsPane_;
    case PaneId::Incoming: return incomingPane_;
    case PaneId::OutgoingTotals: return outgoingTotalsPane_;
    case PaneId::Outgoing: return outgoingPane_;
    case PaneId::Threads: return threadsPane_;
    case PaneId::Cpu: return cpuPane_;
    case PaneId::Count: break;
    }
    return nullptr;
}

void MainWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), Theme::Background);
    painter.fillRect(QRect(1, 1, width() - 2, TitleHeight - 1), Theme::PanelBackground);
    painter.setPen(Theme::Border);
    painter.drawLine(1, 1, width() - 2, 1);
    painter.setFont(PaneWidget::headerFont());
    painter.setPen(Theme::HeaderText);
    painter.drawText(QRect(6, 0, width() - MinimizeButtonWidth - 10, TitleHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("NetStats-Live"));
    const QRect minimizeRect = minimizeButtonRect();
    painter.drawLine(minimizeRect.left() + 5, minimizeRect.center().y() + 3,
                     minimizeRect.right() - 5, minimizeRect.center().y() + 3);
    painter.setPen(Theme::DimRuleLine);
    painter.drawLine(1, TitleHeight - 1, width() - 2, TitleHeight - 1);
    painter.setPen(Theme::Border);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (minimizeButtonRect().contains(event->position().toPoint())) {
            minimizeRequested();
            event->accept();
            return;
        }
        if (windowHandle() != nullptr) {
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event) {
    contextMenu_.popup(event->globalPos());
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Menu ||
        (event->key() == Qt::Key_F10 && event->modifiers().testFlag(Qt::ShiftModifier))) {
        contextMenu_.popup(mapToGlobal(QPoint(6, TitleHeight)));
        for (QAction* action : contextMenu_.actions()) {
            if (action->isEnabled() && !action->isSeparator()) {
                contextMenu_.setActiveAction(action);
                break;
            }
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (persistenceEnabled_) {
        const bool wasShuttingDown = shuttingDown_;
        shuttingDown_ = true;
        saveTotals(false);
        saveConfig(false);
        shuttingDown_ = wasShuttingDown;
    }
    QWidget::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    configureLayerShell();
}

} // namespace nsl
