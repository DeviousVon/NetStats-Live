// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "DBusMenu.h"
#include "TrayIcon.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QMenu>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <functional>
#include <iostream>

namespace {

int failures = 0;
QStringList capturedMessages;

void captureMessage(QtMsgType, const QMessageLogContext&, const QString& message) {
    capturedMessages.append(message);
}

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    return predicate();
}

class FakeStatusNotifierWatcher final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered CONSTANT)
public:
    QString registeredService;
    int registrationCalls = 0;

    bool isHostRegistered() const { return true; }

public Q_SLOTS:
    void RegisterStatusNotifierItem(const QString& service) {
        registeredService = service;
        ++registrationCalls;
    }
};

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    expectTrue(bus.isConnected(), "isolated session bus is connected");

    const QtMessageHandler previousMessageHandler = qInstallMessageHandler(captureMessage);
    nsl::TrayIcon tray;
    qInstallMessageHandler(previousMessageHandler);
    expectTrue(std::none_of(capturedMessages.cbegin(), capturedMessages.cend(), [](const QString& message) {
                   return message.contains(QStringLiteral("systemTrayWindowChanged"));
               }),
               "native SNI path does not instantiate the unavailable Qt tray backend");
    expectTrue(!tray.isAvailable(), "tray starts unavailable without a watcher");

    FakeStatusNotifierWatcher watcher;
    expectTrue(bus.registerObject(QStringLiteral("/StatusNotifierWatcher"), &watcher,
                                  QDBusConnection::ExportAllSlots |
                                      QDBusConnection::ExportAllProperties),
               "fake watcher object registers");
    expectTrue(bus.registerService(QStringLiteral("org.kde.StatusNotifierWatcher")),
               "fake watcher service registers");
    expectTrue(waitUntil([&]() { return watcher.registrationCalls == 1 && tray.isAvailable(); }),
               "TrayIcon becomes available when watcher appears after startup");

    int toggleRequests = 0;
    QString toggleToken;
    QObject::connect(&tray, &nsl::TrayIcon::toggleRequested,
                     [&](const QString& token) {
                         ++toggleRequests;
                         toggleToken = token;
                     });
    QDBusInterface exportedItem(watcher.registeredService,
                                QStringLiteral("/StatusNotifierItem"),
                                QStringLiteral("org.kde.StatusNotifierItem"),
                                bus);
    const auto initialPixmaps = qdbus_cast<nsl::StatusNotifierPixmaps>(exportedItem.property("IconPixmap"));
    expectTrue(!initialPixmaps.isEmpty(), "TrayIcon exports its initial runtime pixmap through SNI");
    nsl::CollectorSnapshot activeSnapshot;
    activeSnapshot.txDelta = 512;
    activeSnapshot.lastActivityAgeMs = 0;
    tray.updateFromSnapshot(activeSnapshot);
    const auto activePixmaps = qdbus_cast<nsl::StatusNotifierPixmaps>(exportedItem.property("IconPixmap"));
    expectTrue(!activePixmaps.isEmpty(), "TrayIcon keeps a runtime pixmap after a traffic update");
    if (!initialPixmaps.isEmpty() && !activePixmaps.isEmpty()) {
        expectTrue(activePixmaps.first().data != initialPixmaps.first().data,
                   "traffic-state change updates the exported SNI pixmap");
    }
    const auto activeToolTip = qdbus_cast<nsl::StatusNotifierToolTip>(exportedItem.property("ToolTip"));
    expectTrue(activeToolTip.title.contains(QStringLiteral("Down")) &&
                   activeToolTip.title.contains(QStringLiteral("Up")),
               "collector update exports live Down/Up text through SNI ToolTip");
    QMenu contextMenu;
    QAction* quitAction = contextMenu.addAction(QStringLiteral("Quit"));
    tray.setContextMenu(&contextMenu);
    expectTrue(exportedItem.property("Menu").value<QDBusObjectPath>().path() == QStringLiteral("/MenuBar"),
               "TrayIcon points SNI hosts at its DBusMenu object");
    QDBusInterface exportedMenu(watcher.registeredService,
                                QStringLiteral("/MenuBar"),
                                QStringLiteral("com.canonical.dbusmenu"),
                                bus);
    const QDBusMessage layoutReply = exportedMenu.call(QStringLiteral("GetLayout"), 0, -1, QStringList{});
    expectTrue(layoutReply.type() == QDBusMessage::ReplyMessage && layoutReply.arguments().size() == 2,
               "TrayIcon exports its configured context menu through DBusMenu");
    int quitId = -1;
    if (layoutReply.arguments().size() == 2) {
        const auto layout = qdbus_cast<nsl::DBusMenuLayoutItem>(layoutReply.arguments().at(1));
        expectTrue(layout.children.size() == 1 &&
                       layout.children.first().properties.value(QStringLiteral("label")).toString() == QStringLiteral("Quit"),
                   "TrayIcon DBusMenu contains the configured QAction");
        if (!layout.children.isEmpty()) {
            quitId = layout.children.first().id;
        }
    }
    int quitTriggers = 0;
    QObject::connect(quitAction, &QAction::triggered, [&]() { ++quitTriggers; });
    const QDBusMessage menuEventReply = exportedMenu.call(
        QStringLiteral("Event"), quitId, QStringLiteral("clicked"),
        QVariant::fromValue(QDBusVariant(QVariant(0))), uint(1));
    expectTrue(menuEventReply.type() == QDBusMessage::ReplyMessage && quitTriggers == 1,
               "TrayIcon DBusMenu click triggers the configured QAction exactly once");
    expectTrue(exportedItem.call(QStringLiteral("ContextMenu"), 30, 40).type() == QDBusMessage::ReplyMessage,
               "TrayIcon SNI ContextMenu call succeeds");
    expectTrue(waitUntil([&]() { return contextMenu.isVisible(); }),
               "TrayIcon displays its configured menu for SNI ContextMenu");
    contextMenu.hide();
    expectTrue(exportedItem.call(QStringLiteral("ProvideXdgActivationToken"),
                                 QStringLiteral("tray-test-token")).type() == QDBusMessage::ReplyMessage,
               "TrayIcon SNI accepts the compositor activation token");
    expectTrue(exportedItem.call(QStringLiteral("Activate"), 10, 20).type() == QDBusMessage::ReplyMessage,
               "TrayIcon SNI Activate call succeeds");
    expectTrue(waitUntil([&]() { return toggleRequests == 1; }),
               "TrayIcon forwards one SNI Activate call to one toggle request");
    expectTrue(toggleToken == QStringLiteral("tray-test-token"),
               "TrayIcon forwards the compositor activation token with the toggle request");

    bus.unregisterService(QStringLiteral("org.kde.StatusNotifierWatcher"));
    bus.unregisterObject(QStringLiteral("/StatusNotifierWatcher"));

    if (failures != 0) {
        std::cerr << failures << " tray backend test failure(s)\n";
        return 1;
    }
    return 0;
}

#include "test_tray_backend.moc"
