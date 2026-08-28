// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "DBusMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QDBusMetaType>
#include <QLocale>
#include <QMenu>

#include <algorithm>

namespace nsl {
namespace {

const QString MenuPath = QStringLiteral("/MenuBar");

QString dbusLabel(QString label) {
    const int mnemonic = label.indexOf(QLatin1Char('&'));
    if (mnemonic >= 0 && mnemonic + 1 < label.size()) {
        label[mnemonic] = QLatin1Char('_');
    }
    return label;
}

QVariantMap filteredProperties(const QVariantMap& properties, const QStringList& names) {
    if (names.isEmpty()) {
        return properties;
    }
    QVariantMap filtered;
    for (const QString& name : names) {
        const auto iterator = properties.constFind(name);
        if (iterator != properties.constEnd()) {
            filtered.insert(name, iterator.value());
        }
    }
    return filtered;
}

} // namespace

QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuItem& item) {
    argument.beginStructure();
    argument << item.id << item.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuItem& item) {
    argument.beginStructure();
    argument >> item.id >> item.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuItemKeys& keys) {
    argument.beginStructure();
    argument << keys.id << keys.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuItemKeys& keys) {
    argument.beginStructure();
    argument >> keys.id >> keys.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuLayoutItem& item) {
    argument.beginStructure();
    argument << item.id << item.properties;
    argument.beginArray(qMetaTypeId<QDBusVariant>());
    for (const DBusMenuLayoutItem& child : item.children) {
        argument << QDBusVariant(QVariant::fromValue(child));
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuLayoutItem& item) {
    argument.beginStructure();
    argument >> item.id >> item.properties;
    argument.beginArray();
    item.children.clear();
    while (!argument.atEnd()) {
        QDBusVariant childVariant;
        argument >> childVariant;
        const QDBusArgument childArgument = qvariant_cast<QDBusArgument>(childVariant.variant());
        DBusMenuLayoutItem child;
        childArgument >> child;
        item.children.append(child);
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusMenuEvent& event) {
    argument.beginStructure();
    argument << event.id << event.eventId << event.data << event.timestamp;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusMenuEvent& event) {
    argument.beginStructure();
    argument >> event.id >> event.eventId >> event.data >> event.timestamp;
    argument.endStructure();
    return argument;
}

DBusMenu::DBusMenu(QObject* parent)
    : QObject(parent),
      bus_(QDBusConnection::sessionBus()) {
    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();
    qDBusRegisterMetaType<DBusMenuItemKeys>();
    qDBusRegisterMetaType<DBusMenuItemKeysList>();
    qDBusRegisterMetaType<DBusMenuLayoutItem>();
    qDBusRegisterMetaType<DBusMenuEvent>();
    qDBusRegisterMetaType<DBusMenuEventList>();
    if (bus_.isConnected()) {
        objectRegistered_ = bus_.registerObject(MenuPath,
                                                this,
                                                QDBusConnection::ExportAllSlots |
                                                    QDBusConnection::ExportAllProperties |
                                                    QDBusConnection::ExportAllSignals);
    }
}

DBusMenu::~DBusMenu() {
    if (objectRegistered_) {
        bus_.unregisterObject(MenuPath);
    }
}

void DBusMenu::setMenu(QMenu* menu) {
    if (menu_ == menu) {
        return;
    }
    menu_ = menu;
    noteLayoutChange();
}

uint DBusMenu::version() const { return 4; }
QString DBusMenu::textDirection() const {
    return QLocale().textDirection() == Qt::RightToLeft ? QStringLiteral("rtl") : QStringLiteral("ltr");
}
QString DBusMenu::status() const { return QStringLiteral("normal"); }
QStringList DBusMenu::iconThemePath() const { return {}; }

bool DBusMenu::AboutToShow(int id) {
    QMenu* menu = menuForId(id);
    if (!menu) {
        return false;
    }
    QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection);
    noteLayoutChange(id);
    return true;
}

QList<int> DBusMenu::AboutToShowGroup(const QList<int>& ids, QList<int>& idErrors) {
    QList<int> updates;
    idErrors.clear();
    for (int id : ids) {
        if (AboutToShow(id)) {
            updates.append(id);
        } else {
            idErrors.append(id);
        }
    }
    return updates;
}

void DBusMenu::Event(int id, const QString& eventId, const QDBusVariant&, uint) {
    QAction* action = actionForId(id);
    if (!action) {
        return;
    }
    if (eventId == QStringLiteral("clicked")) {
        action->trigger();
    } else if (eventId == QStringLiteral("hovered")) {
        action->hover();
    }
}

QList<int> DBusMenu::EventGroup(const DBusMenuEventList& events) {
    QList<int> errors;
    for (const DBusMenuEvent& event : events) {
        if (!actionForId(event.id)) {
            errors.append(event.id);
            continue;
        }
        Event(event.id, event.eventId, event.data, event.timestamp);
    }
    return errors;
}

DBusMenuItemList DBusMenu::GetGroupProperties(const QList<int>& ids,
                                              const QStringList& propertyNames) {
    DBusMenuItemList items;
    QList<int> requestedIds = ids;
    if (requestedIds.isEmpty()) {
        requestedIds = actionsById_.keys();
        std::sort(requestedIds.begin(), requestedIds.end());
    }
    for (int id : std::as_const(requestedIds)) {
        QAction* action = actionForId(id);
        if (action) {
            items.append({id, propertiesForAction(action, propertyNames)});
        }
    }
    return items;
}

uint DBusMenu::GetLayout(int parentId,
                         int recursionDepth,
                         const QStringList& propertyNames,
                         DBusMenuLayoutItem& layout) {
    QMenu* menu = menuForId(parentId);
    if (!menu) {
        layout = {};
        layout.id = parentId;
        return revision_;
    }
    layout = layoutForMenu(menu, parentId, recursionDepth, propertyNames);
    return revision_;
}

QDBusVariant DBusMenu::GetProperty(int id, const QString& name) {
    QAction* action = actionForId(id);
    if (!action) {
        return QDBusVariant(QVariant{});
    }
    return QDBusVariant(propertiesForAction(action, {name}).value(name));
}

int DBusMenu::idForAction(QAction* action) {
    const auto existing = idsByAction_.constFind(action);
    if (existing != idsByAction_.constEnd()) {
        return existing.value();
    }
    const int id = nextId_++;
    idsByAction_.insert(action, id);
    actionsById_.insert(id, action);
    connect(action, &QObject::destroyed, this, [this, action, id]() {
        idsByAction_.remove(action);
        actionsById_.remove(id);
        noteLayoutChange();
    });
    connect(action, &QAction::changed, this, [this]() { noteLayoutChange(); });
    return id;
}

QAction* DBusMenu::actionForId(int id) const {
    const auto iterator = actionsById_.constFind(id);
    return iterator == actionsById_.constEnd() ? nullptr : iterator.value().data();
}

QMenu* DBusMenu::menuForId(int id) const {
    if (id == 0) {
        return menu_;
    }
    QAction* action = actionForId(id);
    return action ? action->menu() : nullptr;
}

QVariantMap DBusMenu::propertiesForAction(QAction* action, const QStringList& propertyNames) {
    QVariantMap properties;
    if (action->isSeparator()) {
        properties.insert(QStringLiteral("type"), QStringLiteral("separator"));
    } else {
        properties.insert(QStringLiteral("label"), dbusLabel(action->text()));
        properties.insert(QStringLiteral("enabled"), action->isEnabled());
        if (action->menu()) {
            properties.insert(QStringLiteral("children-display"), QStringLiteral("submenu"));
        }
        if (action->isCheckable()) {
            const bool exclusive = action->actionGroup() && action->actionGroup()->isExclusive();
            properties.insert(QStringLiteral("toggle-type"),
                              exclusive ? QStringLiteral("radio") : QStringLiteral("checkmark"));
            properties.insert(QStringLiteral("toggle-state"), action->isChecked() ? 1 : 0);
        }
    }
    properties.insert(QStringLiteral("visible"), action->isVisible());
    return filteredProperties(properties, propertyNames);
}

DBusMenuLayoutItem DBusMenu::layoutForMenu(QMenu* menu,
                                            int id,
                                            int recursionDepth,
                                            const QStringList& propertyNames) {
    DBusMenuLayoutItem layout;
    layout.id = id;
    layout.properties.insert(QStringLiteral("children-display"), QStringLiteral("submenu"));
    if (recursionDepth == 0) {
        return layout;
    }
    for (QAction* action : menu->actions()) {
        DBusMenuLayoutItem child;
        child.id = idForAction(action);
        child.properties = propertiesForAction(action, propertyNames);
        if (action->menu() && recursionDepth != 1) {
            const int childDepth = recursionDepth < 0 ? -1 : recursionDepth - 1;
            child = layoutForMenu(action->menu(), child.id, childDepth, propertyNames);
            child.properties = propertiesForAction(action, propertyNames);
        }
        layout.children.append(child);
    }
    return layout;
}

void DBusMenu::noteLayoutChange(int parent) {
    ++revision_;
    Q_EMIT LayoutUpdated(revision_, parent);
}

} // namespace nsl
