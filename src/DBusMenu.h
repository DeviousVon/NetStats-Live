// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>

class QAction;
class QMenu;

namespace nsl {

struct DBusMenuItem {
    int id = 0;
    QVariantMap properties;
};
using DBusMenuItemList = QList<DBusMenuItem>;

struct DBusMenuItemKeys {
    int id = 0;
    QStringList properties;
};
using DBusMenuItemKeysList = QList<DBusMenuItemKeys>;

struct DBusMenuLayoutItem {
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuLayoutItem> children;
};

struct DBusMenuEvent {
    int id = 0;
    QString eventId;
    QDBusVariant data;
    uint timestamp = 0;
};
using DBusMenuEventList = QList<DBusMenuEvent>;

QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuItem& item);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuItem& item);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuItemKeys& keys);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuItemKeys& keys);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuLayoutItem& item);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuLayoutItem& item);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuEvent& event);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuEvent& event);

class DBusMenu final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(uint Version READ version CONSTANT)
    Q_PROPERTY(QString TextDirection READ textDirection CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QStringList IconThemePath READ iconThemePath CONSTANT)

public:
    explicit DBusMenu(QObject* parent = nullptr);
    ~DBusMenu() override;

    void setMenu(QMenu* menu);
    uint version() const;
    QString textDirection() const;
    QString status() const;
    QStringList iconThemePath() const;

public Q_SLOTS:
    bool AboutToShow(int id);
    QList<int> AboutToShowGroup(const QList<int>& ids, QList<int>& idErrors);
    void Event(int id, const QString& eventId, const QDBusVariant& data, uint timestamp);
    QList<int> EventGroup(const nsl::DBusMenuEventList& events);
    nsl::DBusMenuItemList GetGroupProperties(const QList<int>& ids, const QStringList& propertyNames);
    uint GetLayout(int parentId,
                   int recursionDepth,
                   const QStringList& propertyNames,
                   nsl::DBusMenuLayoutItem& layout);
    QDBusVariant GetProperty(int id, const QString& name);

Q_SIGNALS:
    void ItemActivationRequested(int id, uint timestamp);
    void ItemsPropertiesUpdated(const nsl::DBusMenuItemList& updatedProps,
                                const nsl::DBusMenuItemKeysList& removedProps);
    void LayoutUpdated(uint revision, int parent);

private:
    int idForAction(QAction* action);
    QAction* actionForId(int id) const;
    QMenu* menuForId(int id) const;
    QVariantMap propertiesForAction(QAction* action, const QStringList& propertyNames);
    DBusMenuLayoutItem layoutForMenu(QMenu* menu,
                                     int id,
                                     int recursionDepth,
                                     const QStringList& propertyNames);
    void noteLayoutChange(int parent = 0);

    QDBusConnection bus_;
    QPointer<QMenu> menu_;
    bool objectRegistered_ = false;
    uint revision_ = 1;
    int nextId_ = 1;
    QHash<QAction*, int> idsByAction_;
    QHash<int, QPointer<QAction>> actionsById_;
};

} // namespace nsl

Q_DECLARE_METATYPE(nsl::DBusMenuItem)
Q_DECLARE_METATYPE(nsl::DBusMenuItemList)
Q_DECLARE_METATYPE(nsl::DBusMenuItemKeys)
Q_DECLARE_METATYPE(nsl::DBusMenuItemKeysList)
Q_DECLARE_METATYPE(nsl::DBusMenuLayoutItem)
Q_DECLARE_METATYPE(nsl::DBusMenuEvent)
Q_DECLARE_METATYPE(nsl::DBusMenuEventList)
