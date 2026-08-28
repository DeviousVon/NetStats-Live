// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "StatusNotifierItem.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QThread>

#include <functional>
#include <iostream>

namespace {

int failures = 0;

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
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered)
public:
    QString registeredService;
    int registrationCalls = 0;
    bool hostRegistered = false;

    bool isHostRegistered() const { return hostRegistered; }

    void setHostRegistered(bool registered) {
        if (hostRegistered == registered) {
            return;
        }
        hostRegistered = registered;
        if (hostRegistered) {
            Q_EMIT StatusNotifierHostRegistered();
        } else {
            Q_EMIT StatusNotifierHostUnregistered();
        }
    }

public Q_SLOTS:
    void RegisterStatusNotifierItem(const QString& service) {
        registeredService = service;
        ++registrationCalls;
    }

Q_SIGNALS:
    void StatusNotifierHostRegistered();
    void StatusNotifierHostUnregistered();
};

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    expectTrue(bus.isConnected(), "isolated session bus is connected");

    nsl::StatusNotifierItem item;
    expectTrue(!item.isAvailable(), "item starts unavailable without a watcher");

    FakeStatusNotifierWatcher watcher;
    expectTrue(bus.registerObject(QStringLiteral("/StatusNotifierWatcher"), &watcher,
                                  QDBusConnection::ExportAllSlots |
                                      QDBusConnection::ExportAllSignals |
                                      QDBusConnection::ExportAllProperties),
               "fake watcher object registers");
    expectTrue(bus.registerService(QStringLiteral("org.kde.StatusNotifierWatcher")),
               "fake watcher service registers");

    expectTrue(waitUntil([&]() { return watcher.registrationCalls == 1; }),
               "item registers when watcher appears after startup");
    expectTrue(!item.isAvailable(),
               "item remains unavailable while the watcher has no visualization host");
    watcher.setHostRegistered(true);
    expectTrue(waitUntil([&]() { return item.isAvailable(); }),
               "item becomes available when a visualization host registers");
    expectTrue(watcher.registeredService == item.serviceName(),
               "watcher receives the item's owned DBus service name");

    bool activateReceived = false;
    int activateCalls = 0;
    QString activateToken;
    bool contextMenuReceived = false;
    QPoint activatePoint;
    QPoint contextMenuPoint;
    QObject::connect(&item, &nsl::StatusNotifierItem::activateRequested,
                     [&](const QPoint& point, const QString& token) {
                         activateReceived = true;
                         ++activateCalls;
                         activatePoint = point;
                         activateToken = token;
                     });
    QObject::connect(&item, &nsl::StatusNotifierItem::contextMenuRequested,
                     [&](const QPoint& point) { contextMenuReceived = true; contextMenuPoint = point; });
    QDBusInterface exportedItem(item.serviceName(),
                                QStringLiteral("/StatusNotifierItem"),
                                QStringLiteral("org.kde.StatusNotifierItem"),
                                bus);

    expectTrue(exportedItem.property("Menu").value<QDBusObjectPath>().path() == QStringLiteral("/NO_DBUSMENU"),
               "StatusNotifierItem explicitly advertises no DBusMenu until one is configured");
    int newMenuSignals = 0;
    QObject::connect(&item, &nsl::StatusNotifierItem::NewMenu,
                     [&]() { ++newMenuSignals; });
    item.setMenuPath(QDBusObjectPath(QStringLiteral("/MenuBar")));
    expectTrue(waitUntil([&]() { return newMenuSignals == 1; }),
               "changing the DBusMenu path emits NewMenu once");
    expectTrue(exportedItem.property("Menu").value<QDBusObjectPath>().path() == QStringLiteral("/MenuBar"),
               "StatusNotifierItem exports the configured DBusMenu path");

    QImage knownIcon(QSize(2, 2), QImage::Format_ARGB32);
    knownIcon.fill(qRgba(0x11, 0x22, 0x33, 0xff));
    int newIconSignals = 0;
    QObject::connect(&item, &nsl::StatusNotifierItem::NewIcon,
                     [&]() { ++newIconSignals; });
    item.setIcon(QIcon(QPixmap::fromImage(knownIcon)));
    expectTrue(newIconSignals == 1, "setting an icon emits one NewIcon signal");
    expectTrue(exportedItem.property("IconName").toString().isEmpty(),
               "dynamic IconPixmap is not shadowed by a static IconName");
    const QVariant iconProperty = exportedItem.property("IconPixmap");
    expectTrue(iconProperty.isValid(), "IconPixmap is exported over DBus");
    const auto wirePixmaps = qdbus_cast<nsl::StatusNotifierPixmaps>(iconProperty);
    expectTrue(!wirePixmaps.isEmpty(), "IconPixmap contains the supplied icon");
    if (!wirePixmaps.isEmpty()) {
        expectTrue(wirePixmaps.first().width == 2 && wirePixmaps.first().height == 2,
                   "IconPixmap preserves dimensions");
        const QByteArray firstPixel = wirePixmaps.first().data.left(4);
        if (firstPixel != QByteArray::fromHex("ff112233")) {
            std::cerr << "IconPixmap first pixel: " << firstPixel.toHex().constData() << "\n";
        }
        expectTrue(firstPixel == QByteArray::fromHex("ff112233"),
                   "IconPixmap stores ARGB32 in network byte order");
    }

    int newToolTipSignals = 0;
    QObject::connect(&item, &nsl::StatusNotifierItem::NewToolTip,
                     [&]() { ++newToolTipSignals; });
    item.setToolTip(QStringLiteral("Down 2.0 MiB/s  Up 1.0 MiB/s"));
    expectTrue(newToolTipSignals == 1, "setting a tooltip emits one NewToolTip signal");
    const auto wireToolTip = qdbus_cast<nsl::StatusNotifierToolTip>(exportedItem.property("ToolTip"));
    expectTrue(wireToolTip.title == QStringLiteral("Down 2.0 MiB/s  Up 1.0 MiB/s"),
               "ToolTip exports the supplied runtime text");

    expectTrue(exportedItem.call(QStringLiteral("ProvideXdgActivationToken"),
                                 QStringLiteral("nsl-test-token")).type() == QDBusMessage::ReplyMessage,
               "ProvideXdgActivationToken DBus call succeeds");
    expectTrue(exportedItem.call(QStringLiteral("Activate"), 10, 20).type() == QDBusMessage::ReplyMessage,
               "Activate DBus call succeeds");
    expectTrue(exportedItem.call(QStringLiteral("ContextMenu"), 30, 40).type() == QDBusMessage::ReplyMessage,
               "ContextMenu DBus call succeeds");
    expectTrue(waitUntil([&]() { return activateReceived && contextMenuReceived; }),
               "DBus activation methods reach application signals");
    expectTrue(activatePoint == QPoint(10, 20), "Activate preserves coordinates");
    expectTrue(activateToken == QStringLiteral("nsl-test-token"),
               "Activate forwards the pending compositor activation token");
    expectTrue(exportedItem.call(QStringLiteral("Activate"), 11, 21).type() == QDBusMessage::ReplyMessage,
               "second Activate DBus call succeeds");
    expectTrue(waitUntil([&]() { return activateCalls == 2; }),
               "second Activate reaches the application");
    expectTrue(activateToken.isEmpty(), "activation token is consumed exactly once");
    expectTrue(contextMenuPoint == QPoint(30, 40), "ContextMenu preserves coordinates");

    watcher.setHostRegistered(false);
    expectTrue(waitUntil([&]() { return !item.isAvailable(); }),
               "item becomes unavailable when the visualization host disappears");
    watcher.setHostRegistered(true);
    expectTrue(waitUntil([&]() { return item.isAvailable(); }),
               "item becomes available when the visualization host returns");

    expectTrue(bus.unregisterService(QStringLiteral("org.kde.StatusNotifierWatcher")),
               "fake watcher service unregisters");
    expectTrue(waitUntil([&]() { return !item.isAvailable(); }),
               "item becomes unavailable when watcher disappears");
    expectTrue(bus.registerService(QStringLiteral("org.kde.StatusNotifierWatcher")),
               "fake watcher service returns");
    expectTrue(waitUntil([&]() { return watcher.registrationCalls == 2 && item.isAvailable(); }),
               "same item re-registers when watcher returns");
    expectTrue(watcher.registeredService == item.serviceName(),
               "watcher return preserves the item's owned DBus service name");

    bus.unregisterService(QStringLiteral("org.kde.StatusNotifierWatcher"));
    bus.unregisterObject(QStringLiteral("/StatusNotifierWatcher"));

    if (failures != 0) {
        std::cerr << failures << " status notifier test failure(s)\n";
        return 1;
    }
    return 0;
}

#include "test_status_notifier_item.moc"
