// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include <QByteArray>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QPoint>

namespace nsl {

struct StatusNotifierPixmap {
    int width = 0;
    int height = 0;
    QByteArray data;
};

using StatusNotifierPixmaps = QList<StatusNotifierPixmap>;

struct StatusNotifierToolTip {
    QString icon;
    StatusNotifierPixmaps image;
    QString title;
    QString subTitle;
};

QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierPixmap& pixmap);
const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierPixmap& pixmap);
QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierPixmaps& pixmaps);
const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierPixmaps& pixmaps);
QDBusArgument& operator<<(QDBusArgument& argument, const StatusNotifierToolTip& toolTip);
const QDBusArgument& operator>>(const QDBusArgument& argument, StatusNotifierToolTip& toolTip);

class StatusNotifierItem final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category CONSTANT)
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(quint32 WindowId READ windowId CONSTANT)
    Q_PROPERTY(QString IconName READ iconName CONSTANT)
    Q_PROPERTY(nsl::StatusNotifierPixmaps IconPixmap READ iconPixmap NOTIFY NewIcon)
    Q_PROPERTY(nsl::StatusNotifierToolTip ToolTip READ toolTip NOTIFY NewToolTip)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu NOTIFY NewMenu)

public:
    explicit StatusNotifierItem(QObject* parent = nullptr);
    ~StatusNotifierItem() override;

    bool isAvailable() const;
    QString serviceName() const;

    QString category() const;
    QString id() const;
    QString title() const;
    QString status() const;
    quint32 windowId() const;
    QString iconName() const;
    bool itemIsMenu() const;
    QDBusObjectPath menu() const;
    StatusNotifierPixmaps iconPixmap() const;
    StatusNotifierToolTip toolTip() const;
    void setIcon(const QIcon& icon);
    void setMenuPath(const QDBusObjectPath& path);
    void setToolTip(const QString& toolTip);

public Q_SLOTS:
    void ProvideXdgActivationToken(const QString& token);
    void Activate(int x, int y);
    void ContextMenu(int x, int y);

Q_SIGNALS:
    void NewIcon();
    void NewMenu();
    void NewToolTip();
    void availabilityChanged(bool available);
    void activateRequested(const QPoint& point, const QString& activationToken);
    void contextMenuRequested(const QPoint& point);

private Q_SLOTS:
    void registerWithWatcher();
    void watcherHostRegistered();
    void watcherHostUnregistered();
    void watcherUnregistered(const QString& service);

private:
    void setAvailable(bool available);

    QDBusConnection bus_;
    QDBusServiceWatcher watcher_;
    QString serviceName_;
    bool serviceRegistered_ = false;
    bool objectRegistered_ = false;
    bool registeredWithWatcher_ = false;
    bool available_ = false;
    QDBusObjectPath menuPath_{QStringLiteral("/NO_DBUSMENU")};
    QString pendingActivationToken_;
    StatusNotifierPixmaps iconPixmaps_;
    StatusNotifierToolTip toolTip_;
};

} // namespace nsl

Q_DECLARE_METATYPE(nsl::StatusNotifierPixmap)
Q_DECLARE_METATYPE(nsl::StatusNotifierPixmaps)
Q_DECLARE_METATYPE(nsl::StatusNotifierToolTip)
