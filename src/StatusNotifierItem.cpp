// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "StatusNotifierItem.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QtEndian>

#include <algorithm>

namespace nsl {
namespace {

const QString WatcherService = QStringLiteral("org.kde.StatusNotifierWatcher");
const QString WatcherPath = QStringLiteral("/StatusNotifierWatcher");
const QString WatcherInterface = QStringLiteral("org.kde.StatusNotifierWatcher");
const QString ItemPath = QStringLiteral("/StatusNotifierItem");

StatusNotifierPixmaps iconToStatusNotifierPixmaps(const QIcon& icon) {
    StatusNotifierPixmaps result;
    if (icon.isNull()) {
        return result;
    }

    QList<QSize> sizes = icon.availableSizes();
    bool hasSmall = false;
    bool hasMedium = false;
    for (auto iterator = sizes.begin(); iterator != sizes.end();) {
        const int maximum = std::max(iterator->width(), iterator->height());
        if (maximum <= 22) {
            hasSmall = true;
        } else if (maximum <= 64) {
            hasMedium = true;
        }
        if (maximum > 64) {
            iterator = sizes.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (!hasSmall) {
        sizes.append(QSize(22, 22));
    }
    if (!hasMedium) {
        sizes.append(QSize(64, 64));
    }

    result.reserve(sizes.size());
    for (const QSize& size : std::as_const(sizes)) {
        QImage image = icon.pixmap(size).toImage().convertToFormat(QImage::Format_ARGB32);
        if (image.width() != image.height()) {
            const int edge = std::max(image.width(), image.height());
            QImage padded(edge, edge, QImage::Format_ARGB32);
            padded.fill(Qt::transparent);
            QPainter painter(&padded);
            painter.drawImage((edge - image.width()) / 2, (edge - image.height()) / 2, image);
            image = padded;
        }

        StatusNotifierPixmap wirePixmap;
        wirePixmap.width = image.width();
        wirePixmap.height = image.height();
        wirePixmap.data.resize(image.width() * image.height() * 4);
        for (int y = 0; y < image.height(); ++y) {
            const auto* source = reinterpret_cast<const quint32*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                auto* destination = reinterpret_cast<uchar*>(wirePixmap.data.data() + ((y * image.width() + x) * 4));
                qToBigEndian<quint32>(source[x], destination);
            }
        }
        result.append(wirePixmap);
    }
    return result;
}

} // namespace

QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierPixmap& pixmap) {
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierPixmap& pixmap) {
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierPixmaps& pixmaps) {
    argument.beginArray(qMetaTypeId<StatusNotifierPixmap>());
    for (const StatusNotifierPixmap& pixmap : pixmaps) {
        argument << pixmap;
    }
    argument.endArray();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierPixmaps& pixmaps) {
    argument.beginArray();
    pixmaps.clear();
    while (!argument.atEnd()) {
        StatusNotifierPixmap pixmap;
        argument >> pixmap;
        pixmaps.append(pixmap);
    }
    argument.endArray();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierToolTip& toolTip) {
    argument.beginStructure();
    argument << toolTip.icon << toolTip.image << toolTip.title << toolTip.subTitle;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierToolTip& toolTip) {
    argument.beginStructure();
    argument >> toolTip.icon >> toolTip.image >> toolTip.title >> toolTip.subTitle;
    argument.endStructure();
    return argument;
}

StatusNotifierItem::StatusNotifierItem(QObject* parent)
    : QObject(parent),
      bus_(QDBusConnection::sessionBus()),
      watcher_(WatcherService,
               bus_,
               QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
               this),
      serviceName_(QStringLiteral("org.kde.StatusNotifierItem-%1-1").arg(QCoreApplication::applicationPid())) {
    qDBusRegisterMetaType<StatusNotifierPixmap>();
    qDBusRegisterMetaType<StatusNotifierPixmaps>();
    qDBusRegisterMetaType<StatusNotifierToolTip>();
    if (!bus_.isConnected()) {
        return;
    }
    serviceRegistered_ = bus_.registerService(serviceName_);
    if (!serviceRegistered_) {
        return;
    }
    objectRegistered_ = bus_.registerObject(ItemPath,
                                            this,
                                            QDBusConnection::ExportAllSlots |
                                                QDBusConnection::ExportAllProperties |
                                                QDBusConnection::ExportAllSignals);
    if (!objectRegistered_) {
        bus_.unregisterService(serviceName_);
        serviceRegistered_ = false;
        return;
    }

    connect(&watcher_, &QDBusServiceWatcher::serviceRegistered,
            this, &StatusNotifierItem::registerWithWatcher);
    connect(&watcher_, &QDBusServiceWatcher::serviceUnregistered,
            this, &StatusNotifierItem::watcherUnregistered);
    bus_.connect(WatcherService, WatcherPath, WatcherInterface,
                 QStringLiteral("StatusNotifierHostRegistered"),
                 this, SLOT(watcherHostRegistered()));
    bus_.connect(WatcherService, WatcherPath, WatcherInterface,
                 QStringLiteral("StatusNotifierHostUnregistered"),
                 this, SLOT(watcherHostUnregistered()));

    QDBusInterface watcher(WatcherService, WatcherPath, WatcherInterface, bus_);
    if (watcher.isValid()) {
        registerWithWatcher();
    }
}

StatusNotifierItem::~StatusNotifierItem() {
    if (objectRegistered_) {
        bus_.unregisterObject(ItemPath);
    }
    if (serviceRegistered_) {
        bus_.unregisterService(serviceName_);
    }
}

bool StatusNotifierItem::isAvailable() const {
    return available_;
}

QString StatusNotifierItem::serviceName() const {
    return serviceName_;
}

QString StatusNotifierItem::category() const { return QStringLiteral("SystemServices"); }
QString StatusNotifierItem::id() const { return QStringLiteral("netstats-live"); }
QString StatusNotifierItem::title() const { return QStringLiteral("NetStats-Live"); }
QString StatusNotifierItem::status() const { return QStringLiteral("Active"); }
quint32 StatusNotifierItem::windowId() const { return 0; }
QString StatusNotifierItem::iconName() const { return {}; }
bool StatusNotifierItem::itemIsMenu() const { return false; }
QDBusObjectPath StatusNotifierItem::menu() const { return menuPath_; }
StatusNotifierPixmaps StatusNotifierItem::iconPixmap() const { return iconPixmaps_; }
StatusNotifierToolTip StatusNotifierItem::toolTip() const { return toolTip_; }

void StatusNotifierItem::setIcon(const QIcon& icon) {
    iconPixmaps_ = iconToStatusNotifierPixmaps(icon);
    Q_EMIT NewIcon();
}

void StatusNotifierItem::setMenuPath(const QDBusObjectPath& path) {
    if (menuPath_.path() == path.path()) {
        return;
    }
    menuPath_ = path;
    Q_EMIT NewMenu();
}

void StatusNotifierItem::setToolTip(const QString& toolTip) {
    toolTip_.title = toolTip;
    Q_EMIT NewToolTip();
}

void StatusNotifierItem::ProvideXdgActivationToken(const QString& token) {
    pendingActivationToken_ = token;
}

void StatusNotifierItem::Activate(int x, int y) {
    const QString activationToken = pendingActivationToken_;
    pendingActivationToken_.clear();
    Q_EMIT activateRequested(QPoint(x, y), activationToken);
}

void StatusNotifierItem::ContextMenu(int x, int y) {
    Q_EMIT contextMenuRequested(QPoint(x, y));
}

void StatusNotifierItem::registerWithWatcher() {
    if (!objectRegistered_) {
        return;
    }
    QDBusInterface watcher(WatcherService, WatcherPath, WatcherInterface, bus_);
    if (!watcher.isValid()) {
        registeredWithWatcher_ = false;
        setAvailable(false);
        return;
    }
    const QDBusMessage reply = watcher.call(QStringLiteral("RegisterStatusNotifierItem"), serviceName_);
    registeredWithWatcher_ = reply.type() == QDBusMessage::ReplyMessage;
    const QVariant hostRegistered = watcher.property("IsStatusNotifierHostRegistered");
    setAvailable(registeredWithWatcher_ && hostRegistered.isValid() && hostRegistered.toBool());
}

void StatusNotifierItem::watcherHostRegistered() {
    setAvailable(registeredWithWatcher_);
}

void StatusNotifierItem::watcherHostUnregistered() {
    setAvailable(false);
}

void StatusNotifierItem::watcherUnregistered(const QString&) {
    registeredWithWatcher_ = false;
    setAvailable(false);
}

void StatusNotifierItem::setAvailable(bool available) {
    if (available_ == available) {
        return;
    }
    available_ = available;
    Q_EMIT availabilityChanged(available_);
}

} // namespace nsl
