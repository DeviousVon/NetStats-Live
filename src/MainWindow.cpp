#include "MainWindow.h"

#ifdef NSL_HAS_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

#include <array>
#include <cmath>

namespace nsl {
namespace {

constexpr int WindowWidth = 224;
constexpr int TitleHeight = 18;

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

MainWindow::MainWindow(bool simulate, QWidget* parent)
    : QWidget(parent), contextMenu_(this), statisticsMenu_(tr("Statistics"), this), configMenu_(tr("Config"), this), interfaceMenu_(tr("Interface"), this), unitGroup_(this), interfaceGroup_(this) {
    setObjectName(QStringLiteral("nsl-linux"));
    setWindowTitle(QStringLiteral("nsl-linux"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFixedWidth(WindowWidth);

    config_ = settings_.load();
    createPanes();
    createMenus();
    applyPaneVisibility();
    applyAlwaysOnTop();

    tray_.setContextMenu(&contextMenu_);
    connect(&tray_, &TrayIcon::toggleRequested, this, &MainWindow::toggleVisibleFromTray);
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

    totalsFlushTimer_.setInterval(60000);
    connect(&totalsFlushTimer_, &QTimer::timeout, this, &MainWindow::saveTotals);
    totalsFlushTimer_.start();

    clipCap_.setEnabled(config_.urlClipCap && !simulate);
    if (simulate) {
        collector_.startSimulation(config_.monthKey, config_.rxMonth, config_.txMonth);
    } else {
        collector_.start(config_.selectedInterface, config_.remoteTarget, config_.monthKey, config_.rxMonth, config_.txMonth);
    }

    if (!config_.windowPos.isNull()) {
        move(config_.windowPos);
    }
}

MainWindow::~MainWindow() {
    if (persistenceEnabled_) {
        saveTotals();
        saveConfig();
    }
}

bool MainWindow::autoMinimizeEnabled() const {
    return config_.autoMinimize;
}

void MainWindow::setPersistenceEnabled(bool enabled) {
    persistenceEnabled_ = enabled;
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
    incomingTotalsPane_->setColumns({{tr("Last Reboot"), tr("3.9MB")}, {tr("This Month"), tr("3.9MB")}, {tr("Last Month"), tr("0B")}});
    outgoingTotalsPane_->setColumns({{tr("Last Reboot"), tr("1.0MB")}, {tr("This Month"), tr("1.0MB")}, {tr("Last Month"), tr("0B")}});

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
    layout_->setSizeConstraint(QLayout::SetFixedSize);

    localPane_ = new TextPane(QStringLiteral("Local"), 66, this);
    remotePane_ = new TextPane(QStringLiteral("Remote"), 66, this);
    incomingTotalsPane_ = new TextPane(QStringLiteral("Incoming Totals"), 43, this);
    incomingPane_ = new GraphPane(QStringLiteral("Incoming"), GraphValueMode::NetworkRate, this);
    outgoingTotalsPane_ = new TextPane(QStringLiteral("Outgoing Totals"), 43, this);
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

void MainWindow::createMenus() {
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
    connect(autoStart, &QAction::toggled, this, [this](bool checked) {
        config_.autoStart = checked;
        settings_.setAutoStart(checked, QCoreApplication::applicationFilePath());
        saveConfig();
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
    contextMenu_.addAction(tr("Minimize"), this, &QWidget::hide);
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
            it.value()->setChecked(visible);
        }
    }
    setFixedSize(WindowWidth, height);
    updateGeometry();
    update();
}

void MainWindow::applyAlwaysOnTop() {
    setWindowFlag(Qt::WindowStaysOnTopHint, config_.alwaysOnTop);
#ifdef NSL_HAS_LAYER_SHELL
    if (config_.alwaysOnTop && windowHandle() != nullptr) {
        if (auto* layerWindow = LayerShellQt::Window::get(windowHandle())) {
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            layerWindow->setScope(QStringLiteral("nsl-linux"));
        }
    }
#endif
    if (isVisible()) {
        show();
    }
}

void MainWindow::updateFromCollector(const CollectorSnapshot& snapshot) {
    latestSnapshot_ = snapshot;
    config_.rxMonth = snapshot.rxMonth;
    config_.txMonth = snapshot.txMonth;
    config_.monthKey = snapshot.monthKey;
    config_.remoteTarget = snapshot.remoteTarget == QStringLiteral("n/a") ? QString() : snapshot.remoteTarget;

    localPane_->setRows({{tr("Name"), snapshot.hostname}, {tr("IP"), snapshot.ipAddress}, {tr("Device"), snapshot.selectedInterface}});
    remotePane_->setRows({{tr("Name"), snapshot.remoteTarget}, {tr("Ping"), pingText(snapshot)}, {tr("Hops"), hopText(snapshot)}});
    incomingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(snapshot.rxSession)}, {tr("This Month"), totalText(snapshot.rxMonth)}, {tr("Last Month"), totalText(0)}});
    outgoingTotalsPane_->setColumns({{tr("Last Reboot"), totalText(snapshot.txSession)}, {tr("This Month"), totalText(snapshot.txMonth)}, {tr("Last Month"), totalText(0)}});

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

void MainWindow::toggleVisibleFromTray() {
    if (isVisible()) {
        hide();
    } else {
        show();
        raise();
        activateWindow();
    }
}

void MainWindow::saveConfig() {
    config_.windowPos = pos();
    settings_.saveConfig(config_);
}

void MainWindow::saveTotals() {
    settings_.saveMonthlyTotals(config_.monthKey, config_.rxMonth, config_.txMonth);
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
    painter.fillRect(rect(), PaneWidget::backgroundColor());
    painter.fillRect(QRect(1, 1, width() - 2, TitleHeight - 1), PaneWidget::panelColor());
    painter.setPen(QColor(0x24, 0x24, 0x24));
    painter.drawLine(1, 1, width() - 2, 1);
    painter.setFont(PaneWidget::headerFont());
    painter.setPen(PaneWidget::valueColor());
    painter.drawText(QRect(6, 0, width() - 12, TitleHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("nsl-linux"));
    painter.setPen(QColor(0x00, 0x1b, 0x16));
    painter.drawLine(1, TitleHeight - 1, width() - 2, TitleHeight - 1);
    painter.setPen(PaneWidget::borderColor());
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
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

void MainWindow::closeEvent(QCloseEvent* event) {
    if (persistenceEnabled_) {
        saveTotals();
        saveConfig();
    }
    QWidget::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyAlwaysOnTop();
}

} // namespace nsl
