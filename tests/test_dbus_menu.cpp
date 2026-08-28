// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "DBusMenu.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QMenu>

#include <iostream>

namespace {

int failures = 0;

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    expectTrue(bus.isConnected(), "isolated session bus is connected");

    QMenu root;
    QAction* open = root.addAction(QStringLiteral("&Open"));
    QAction* checked = root.addAction(QStringLiteral("Always on &Top"));
    checked->setCheckable(true);
    checked->setChecked(true);
    QMenu* interfaces = root.addMenu(QStringLiteral("&Interface"));
    interfaces->addAction(QStringLiteral("ALL"));

    nsl::DBusMenu menu;
    menu.setMenu(&root);

    QDBusInterface exported(bus.baseService(), QStringLiteral("/MenuBar"),
                            QStringLiteral("com.canonical.dbusmenu"), bus);
    const QDBusMessage reply = exported.call(QStringLiteral("GetLayout"), 0, -1, QStringList{});
    expectTrue(reply.type() == QDBusMessage::ReplyMessage, "GetLayout returns a DBus reply");
    expectTrue(reply.arguments().size() == 2, "GetLayout returns revision and layout");
    int openId = -1;
    if (reply.arguments().size() == 2) {
        expectTrue(reply.arguments().first().toUInt() > 0, "GetLayout returns a positive revision");
        const auto layout = qdbus_cast<nsl::DBusMenuLayoutItem>(reply.arguments().at(1));
        expectTrue(layout.id == 0, "DBusMenu root has protocol id zero");
        expectTrue(layout.properties.value(QStringLiteral("children-display")).toString() == QStringLiteral("submenu"),
                   "DBusMenu root is marked as a submenu container");
        expectTrue(layout.children.size() == 3, "DBusMenu root exposes all top-level actions");
        if (layout.children.size() == 3) {
            openId = layout.children.at(0).id;
            expectTrue(layout.children.at(0).properties.value(QStringLiteral("label")).toString() == QStringLiteral("_Open"),
                       "DBusMenu converts Qt mnemonic markers");
            expectTrue(layout.children.at(1).properties.value(QStringLiteral("toggle-type")).toString() == QStringLiteral("checkmark"),
                       "DBusMenu exports checkable action type");
            expectTrue(layout.children.at(1).properties.value(QStringLiteral("toggle-state")).toInt() == 1,
                       "DBusMenu exports checked state");
            expectTrue(layout.children.at(2).properties.value(QStringLiteral("children-display")).toString() == QStringLiteral("submenu"),
                       "DBusMenu exports submenu identity");
            expectTrue(layout.children.at(2).children.size() == 1,
                       "DBusMenu recursively exports submenu actions");
        }
    }

    const nsl::DBusMenuItemList allProperties = menu.GetGroupProperties({}, {});
    expectTrue(allProperties.size() == 4,
               "GetGroupProperties with an empty ID list returns every known menu item");

    int openTriggers = 0;
    QObject::connect(open, &QAction::triggered, [&]() { ++openTriggers; });
    const QDBusMessage eventReply = exported.call(
        QStringLiteral("Event"), openId, QStringLiteral("clicked"),
        QVariant::fromValue(QDBusVariant(QVariant(0))), uint(1));
    expectTrue(eventReply.type() == QDBusMessage::ReplyMessage,
               "DBusMenu clicked Event returns a DBus reply");
    expectTrue(openTriggers == 1, "DBusMenu clicked Event triggers the matching QAction exactly once");

    int aboutToShowCalls = 0;
    QObject::connect(&root, &QMenu::aboutToShow, [&]() {
        ++aboutToShowCalls;
        if (aboutToShowCalls == 1) {
            root.addAction(QStringLiteral("Dynamic"));
        }
    });
    const QDBusMessage aboutReply = exported.call(QStringLiteral("AboutToShow"), 0);
    expectTrue(aboutReply.type() == QDBusMessage::ReplyMessage &&
                   !aboutReply.arguments().isEmpty() && aboutReply.arguments().first().toBool(),
               "DBusMenu AboutToShow reports that the root layout should be refreshed");
    expectTrue(aboutToShowCalls == 1,
               "DBusMenu AboutToShow invokes the configured QMenu exactly once");
    const QDBusMessage refreshedReply = exported.call(QStringLiteral("GetLayout"), 0, -1, QStringList{});
    if (refreshedReply.arguments().size() == 2) {
        const auto refreshed = qdbus_cast<nsl::DBusMenuLayoutItem>(refreshedReply.arguments().at(1));
        expectTrue(refreshed.children.size() == 4,
                   "DBusMenu refresh includes actions added by aboutToShow");
    }

    if (failures != 0) {
        std::cerr << failures << " DBusMenu test failure(s)\n";
        return 1;
    }
    return 0;
}
