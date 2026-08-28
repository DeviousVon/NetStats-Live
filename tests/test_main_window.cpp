// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "MainWindow.h"
#include "Lifecycle.h"

#ifdef NSL_HAS_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

#include <QApplication>
#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>

#include <iostream>

namespace {

int failures = 0;

void expectTrue(bool value, const char* expression) {
    if (!value) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

template <typename L, typename R>
void expectEqual(const L& left, const R& right, const char* expression) {
    if (!(left == right)) {
        std::cerr << "FAIL: " << expression << "\n";
        ++failures;
    }
}

void processWindowEvents() {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

class HideEventCounter final : public QObject {
public:
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::Hide) {
            ++count;
        }
        return false;
    }
};

} // namespace

int main(int argc, char** argv) {
    QTemporaryDir tempConfig;
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    qputenv("XDG_CONFIG_HOME", tempConfig.path().toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", QStringLiteral("unix:path=%1/no-session-bus").arg(tempConfig.path()).toUtf8());

    QApplication app(argc, argv);

    if (QCoreApplication::arguments().contains(QStringLiteral("--live-layer-transition-check"))) {
        const QString liveSettingsPath = QDir(tempConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
        QDir().mkpath(QFileInfo(liveSettingsPath).absolutePath());
        {
            QSettings seeded(liveSettingsPath, QSettings::IniFormat);
            seeded.setValue(QStringLiteral("config/alwaysOnTop"), true);
            seeded.sync();
        }
        nsl::MainWindow liveWindow(true);
        liveWindow.setPersistenceEnabled(false);
        liveWindow.show();
        processWindowEvents();
#ifdef NSL_HAS_LAYER_SHELL
        auto* initialLayer = liveWindow.windowHandle()->findChild<LayerShellQt::Window*>();
        expectTrue(initialLayer != nullptr, "live Wayland transition begins with layer-shell authority");
        QAction* alwaysOnTopAction = nullptr;
        for (QAction* action : liveWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("Always on Top")) {
                alwaysOnTopAction = action;
                break;
            }
        }
        expectTrue(alwaysOnTopAction != nullptr, "live Wayland transition finds Always on Top action");
        if (alwaysOnTopAction != nullptr) {
            QPointer<LayerShellQt::Window> initialPointer(initialLayer);
            alwaysOnTopAction->setChecked(false);
            processWindowEvents();
            expectTrue(initialPointer.isNull(), "live Wayland off transition destroys layer authority");
            expectTrue(liveWindow.windowHandle()->findChild<LayerShellQt::Window*>() == nullptr,
                       "live Wayland off transition has no layer object");
            alwaysOnTopAction->setChecked(true);
            processWindowEvents();
            auto* replacement = liveWindow.windowHandle()->findChild<LayerShellQt::Window*>();
            expectTrue(replacement != nullptr,
                       "live Wayland on transition creates fresh layer authority");
        }
#endif
        liveWindow.close();
        return failures == 0 ? 0 : 1;
    }

    const QString blockedAutostartDirectory = QDir(tempConfig.path()).filePath(QStringLiteral("autostart"));
    QFile blockedAutostartFile(blockedAutostartDirectory);
    expectTrue(blockedAutostartFile.open(QIODevice::WriteOnly | QIODevice::Text),
               "file blocks autostart directory creation for failure reporting");
    blockedAutostartFile.write("not a directory\n");
    blockedAutostartFile.close();

    const QString settingsPath = QDir(tempConfig.path()).filePath(QStringLiteral("netstats-live/netstats-live.conf"));
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());
    {
        QSettings seeded(settingsPath, QSettings::IniFormat);
        seeded.setValue(QStringLiteral("window/pos"), QPoint(5000, 5000));
        seeded.setValue(QStringLiteral("config/alwaysOnTop"), true);
        for (int i = 0; i < nsl::PaneCount; ++i) {
            const auto id = static_cast<nsl::PaneId>(i);
            seeded.setValue(QStringLiteral("panes/%1").arg(nsl::paneConfigKey(id)), false);
        }
        seeded.sync();
    }

    nsl::MainWindow window(true);
    expectEqual(window.width(), 238, "disabling every pane preserves a reachable full-width title strip");
    QList<QRect> screenGeometries;
    for (QScreen* screen : QGuiApplication::screens()) {
        screenGeometries.append(screen->availableGeometry());
    }
    expectEqual(window.pos(), nsl::visibleWindowPosition(QPoint(5000, 5000), window.size(), screenGeometries),
                "persisted offscreen window position is clamped during construction");
    expectTrue(window.metaObject()->indexOfMethod("activateFromInstanceRequest()") >= 0,
               "legacy no-argument single-instance activation slot remains exported");
    expectTrue(window.metaObject()->indexOfMethod("activateFromInstanceRequestWithToken(QString)") >= 0,
               "token-aware single-instance activation slot is exported through the Qt metaobject");
    window.move(120, 80);
    window.show();
    processWindowEvents();
#ifdef NSL_HAS_LAYER_SHELL
    auto* layerWindow = LayerShellQt::Window::get(window.windowHandle());
    expectTrue(layerWindow != nullptr, "layer-shell window object is available in the optional build");
    if (layerWindow != nullptr) {
        LayerShellQt::Window::Anchors expectedAnchors;
        expectedAnchors.setFlag(LayerShellQt::Window::AnchorTop);
        expectedAnchors.setFlag(LayerShellQt::Window::AnchorLeft);
        expectEqual(layerWindow->desiredSize(), window.size(), "layer-shell desired size stays equal to the compact widget");
        expectEqual(layerWindow->anchors(), expectedAnchors, "layer-shell uses top-left anchors instead of stretching across the work area");
        expectEqual(layerWindow->exclusionZone(), 0, "floating monitor reserves no desktop work area");
        QAction* cpuAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("CPU")) {
                cpuAction = action;
                break;
            }
        }
        expectTrue(cpuAction != nullptr, "CPU pane action is discoverable for layer-shell resize regression");
        if (cpuAction != nullptr) {
            const QSize initialSize = window.size();
            cpuAction->setChecked(true);
            processWindowEvents();
            expectTrue(window.size() != initialSize, "pane visibility toggle changes the fixed widget size");
            expectEqual(layerWindow->desiredSize(), window.size(),
                        "layer-shell desired size follows pane visibility changes");
        }

        QAction* alwaysOnTopAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("Always on Top")) {
                alwaysOnTopAction = action;
                break;
            }
        }
        expectTrue(alwaysOnTopAction != nullptr, "Always on Top action is discoverable for layer authority transitions");
        if (alwaysOnTopAction != nullptr) {
            QPointer<LayerShellQt::Window> initialLayerWindow(layerWindow);
            alwaysOnTopAction->setChecked(false);
            processWindowEvents();
            expectTrue(initialLayerWindow.isNull(), "disabling Always on Top deletes prior layer-shell authority");
            expectTrue(window.windowHandle()->findChild<LayerShellQt::Window*>() == nullptr,
                       "disabled Always on Top leaves no layer-shell object on the normal surface");
            alwaysOnTopAction->setChecked(true);
            processWindowEvents();
            auto* replacementLayerWindow = window.windowHandle()->findChild<LayerShellQt::Window*>();
            expectTrue(replacementLayerWindow != nullptr, "re-enabling Always on Top attaches fresh layer-shell authority");
        }
    }
#endif
    const QPoint placedPosition = window.pos();

    QMenu* keyboardMenu = window.findChild<QMenu*>(QStringLiteral("main-context-menu"));
    expectTrue(keyboardMenu != nullptr, "context menu has stable semantic object identity");
    QKeyEvent menuKey(QEvent::KeyPress, Qt::Key_Menu, Qt::NoModifier);
    QApplication::sendEvent(&window, &menuKey);
    processWindowEvents();
    expectTrue(keyboardMenu != nullptr && keyboardMenu->isVisible(), "Menu key opens the context menu");
    if (keyboardMenu != nullptr) {
        keyboardMenu->hide();
    }
    QKeyEvent shiftF10(QEvent::KeyPress, Qt::Key_F10, Qt::ShiftModifier);
    QApplication::sendEvent(&window, &shiftF10);
    processWindowEvents();
    expectTrue(keyboardMenu != nullptr && keyboardMenu->isVisible(), "Shift+F10 opens the context menu");
    if (keyboardMenu != nullptr) {
        keyboardMenu->hide();
    }

    QLabel* titleLabel = window.findChild<QLabel*>(QStringLiteral("title-label"));
    expectTrue(titleLabel != nullptr, "painted title has a semantic QLabel overlay");
    if (titleLabel != nullptr) {
        QAccessibleInterface* titleInterface = QAccessible::queryAccessibleInterface(titleLabel);
        expectTrue(titleInterface != nullptr, "title semantic overlay has an accessible interface");
        if (titleInterface != nullptr) {
            expectEqual(titleInterface->role(), QAccessible::StaticText, "title semantic overlay exposes StaticText role");
            expectEqual(titleInterface->text(QAccessible::Name), QStringLiteral("NetStats-Live"),
                        "title semantic overlay exposes stable accessible name");
        }
    }

    QToolButton* minimizeButton = window.findChild<QToolButton*>(QStringLiteral("minimize-button"));
    expectTrue(minimizeButton != nullptr, "painted minimize target has a semantic keyboard button overlay");
    if (minimizeButton != nullptr) {
        expectTrue(minimizeButton->focusPolicy() == Qt::StrongFocus, "minimize semantic button is keyboard focusable");
        QAccessibleInterface* buttonInterface = QAccessible::queryAccessibleInterface(minimizeButton);
        expectTrue(buttonInterface != nullptr, "minimize semantic button has an accessible interface");
        if (buttonInterface != nullptr) {
            expectEqual(buttonInterface->role(), QAccessible::Button, "minimize semantic button exposes Button role");
            expectEqual(buttonInterface->text(QAccessible::Name), QStringLiteral("Minimize NetStats-Live"),
                        "minimize semantic button exposes stable accessible name");
            QAccessibleActionInterface* actions = buttonInterface->actionInterface();
            expectTrue(actions != nullptr && actions->actionNames().contains(QAccessibleActionInterface::pressAction()),
                       "minimize semantic button exposes accessible press action");
            if (actions != nullptr) {
                minimizeButton->setFocus(Qt::TabFocusReason);
                actions->doAction(QAccessibleActionInterface::pressAction());
                processWindowEvents();
                expectTrue(window.isMinimized(), "accessible press action minimizes the real window");
                expectTrue(QMetaObject::invokeMethod(&window, "toggleVisibleFromTray", Qt::DirectConnection),
                           "tray restore is invokable after accessible minimize");
                processWindowEvents();
                processWindowEvents();
                expectTrue(!window.isMinimized(), "tray restores after accessible minimize");
                expectTrue(QApplication::focusWidget() == minimizeButton,
                           "tray restore returns keyboard focus to the minimize control");
            }
        }
        minimizeButton->setFocus(Qt::TabFocusReason);
        QKeyEvent spacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent spaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(minimizeButton, &spacePress);
        QApplication::sendEvent(minimizeButton, &spaceRelease);
        processWindowEvents();
        expectTrue(window.isMinimized(), "Space on semantic minimize button minimizes the window");
        window.showNormal();
        processWindowEvents();
    }

    QAction* autoStartAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("Auto Start")) {
            autoStartAction = action;
            break;
        }
    }
    expectTrue(autoStartAction != nullptr, "Auto Start action is discoverable for failure rollback");
    if (autoStartAction != nullptr) {
        autoStartAction->setChecked(true);
        processWindowEvents();
        expectTrue(!autoStartAction->isChecked(), "failed autostart creation reverts the menu action");
        {
            QSettings failedAutostartSettings(settingsPath, QSettings::IniFormat);
            expectTrue(!failedAutostartSettings.value(QStringLiteral("config/autoStart"), false).toBool(),
                       "failed autostart creation cannot persist enabled config state");
        }
        const QList<QMessageBox*> firstErrors = window.findChildren<QMessageBox*>(QStringLiteral("storage-error-message"));
        expectEqual(firstErrors.size(), 1, "failed autostart creation opens one user-visible error");
        if (!firstErrors.isEmpty()) {
            expectTrue(firstErrors.constFirst()->isVisible(), "storage error is visible");
            expectEqual(firstErrors.constFirst()->windowModality(), Qt::NonModal, "storage error is nonmodal");
        }

        autoStartAction->setChecked(true);
        processWindowEvents();
        expectEqual(window.findChildren<QMessageBox*>(QStringLiteral("storage-error-message")).size(), 1,
                    "repeated identical autostart failure does not create a dialog storm");

        expectTrue(QFile::remove(blockedAutostartDirectory), "remove autostart directory blocker");
        expectTrue(QDir().mkpath(blockedAutostartDirectory), "create writable autostart directory");
        const QString autoStartPath = QDir(blockedAutostartDirectory).filePath(QStringLiteral("netstats-live.desktop"));
        const QByteArray customAutoStartBytes = QByteArrayLiteral("[Desktop Entry]\nName=Custom NetStats\nExec=/custom/netstats\n");
        const QFile::Permissions customAutoStartPermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
        const QDateTime customAutoStartModified = QDateTime::currentDateTimeUtc().addSecs(-3600);
        QFile customAutoStart(autoStartPath);
        expectTrue(customAutoStart.open(QIODevice::WriteOnly | QIODevice::Text), "custom autostart entry is created");
        expectEqual(customAutoStart.write(customAutoStartBytes), customAutoStartBytes.size(), "custom autostart bytes are written");
        customAutoStart.close();
        expectTrue(QFile::setPermissions(autoStartPath, customAutoStartPermissions), "custom autostart permissions are set");
        expectTrue(customAutoStart.open(QIODevice::ReadWrite), "custom autostart entry reopens for timestamp setup");
        expectTrue(customAutoStart.setFileTime(customAutoStartModified, QFileDevice::FileModificationTime),
                   "custom autostart modification time is set");
        customAutoStart.close();
        const QFile::Permissions customAutoStartObservedPermissions = QFileInfo(autoStartPath).permissions();
        const QString settingsDirectory = QFileInfo(settingsPath).absolutePath();
        expectTrue(QDir(settingsDirectory).removeRecursively(), "remove settings directory for transaction rollback test");
        QFile settingsDirectoryBlocker(settingsDirectory);
        expectTrue(settingsDirectoryBlocker.open(QIODevice::WriteOnly | QIODevice::Text),
                   "file blocks config persistence after autostart creation");
        settingsDirectoryBlocker.write("not a directory\n");
        settingsDirectoryBlocker.close();

        autoStartAction->setChecked(true);
        processWindowEvents();
        expectTrue(!autoStartAction->isChecked(), "config failure rolls back Auto Start action state");
        QFile restoredAutoStart(autoStartPath);
        expectTrue(restoredAutoStart.open(QIODevice::ReadOnly | QIODevice::Text),
                   "config failure restores the prior autostart entry");
        expectEqual(restoredAutoStart.readAll(), customAutoStartBytes,
                    "autostart rollback restores exact prior bytes");
        restoredAutoStart.close();
        expectEqual(QFileInfo(autoStartPath).permissions(), customAutoStartObservedPermissions,
                    "autostart rollback restores prior permissions");
        expectEqual(QFileInfo(autoStartPath).lastModified().toSecsSinceEpoch(), customAutoStartModified.toSecsSinceEpoch(),
                    "autostart rollback restores prior modification time");
        const QList<QMessageBox*> updatedErrors = window.findChildren<QMessageBox*>(QStringLiteral("storage-error-message"));
        expectEqual(updatedErrors.size(), 1, "distinct config failure reuses the nonmodal error box");
        if (!updatedErrors.isEmpty()) {
            expectTrue(updatedErrors.constFirst()->text().contains(QStringLiteral("save configuration"), Qt::CaseInsensitive),
                       "reused error box updates to the distinct config failure");
        }
        expectTrue(QFile::remove(settingsDirectory), "remove config directory blocker after rollback test");
        expectTrue(QDir().mkpath(settingsDirectory), "restore writable config directory for remaining window tests");
    }

    qputenv("XDG_ACTIVATION_TOKEN", "nsl-test-prior-token");
    window.activateFromInstanceRequestWithToken(QStringLiteral("nsl-test-activation-token"));
    processWindowEvents();
    expectTrue(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"),
               "forwarded activation token is not retained in the existing process environment");

    expectTrue(QMetaObject::invokeMethod(&window, "toggleVisibleFromTray", Qt::DirectConnection),
               "tray toggle slot is invokable");
    processWindowEvents();
    expectTrue(window.isVisible(), "tray toggle minimizes without hiding and remapping the window");
    expectTrue(window.isMinimized(), "visible window becomes minimized from the tray");

    QSettings persisted(settingsPath, QSettings::IniFormat);
    expectEqual(persisted.value(QStringLiteral("window/pos")).toPoint(), placedPosition,
                "minimizing immediately persists the current window position");

    qputenv("XDG_ACTIVATION_TOKEN", "nsl-test-stale-tray-token");
    expectTrue(QMetaObject::invokeMethod(&window, "toggleVisibleFromTrayWithToken", Qt::DirectConnection,
                                         Q_ARG(QString, QStringLiteral("nsl-test-tray-token"))),
               "token-aware tray restore slot is invokable");
    processWindowEvents();
    expectTrue(window.isVisible(), "tray toggle restores a minimized window");
    expectTrue(!window.isMinimized(), "restored tray window is normal rather than still minimized");
    expectTrue(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"),
               "tray activation token is cleared after the synchronous restore path");
    expectEqual(window.pos(), placedPosition, "tray minimize and restore preserve window position");

    HideEventCounter trayLossRemapCounter;
    window.installEventFilter(&trayLossRemapCounter);
    window.showMinimized();
    processWindowEvents();
    const int hidesBeforeTrayLossRecovery = trayLossRemapCounter.count;
    expectTrue(QMetaObject::invokeMethod(&window, "ensureTrayReachability", Qt::DirectConnection),
               "tray-loss reachability slot is invokable");
    processWindowEvents();
    expectTrue(trayLossRemapCounter.count > hidesBeforeTrayLossRecovery,
               "tray-loss recovery remaps a minimized generic Wayland surface");
    expectTrue(!window.isMinimized(), "tray-loss remap leaves the window normal and reachable");
    window.removeEventFilter(&trayLossRemapCounter);

    qputenv("XDG_ACTIVATION_TOKEN", "nsl-test-stale-token");
    window.showMinimized();
    processWindowEvents();
    window.activateFromInstanceRequest();
    processWindowEvents();
    expectTrue(!window.isMinimized(), "single-instance activation restores a minimized window");
    expectTrue(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"),
               "tokenless activation clears a stale process activation token");

    const QPoint minimizeButtonPoint(window.width() - 9, 9);
    QMouseEvent minimizePress(QEvent::MouseButtonPress, QPointF(minimizeButtonPoint),
                              QPointF(window.mapToGlobal(minimizeButtonPoint)),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &minimizePress);
    processWindowEvents();
    expectTrue(window.isMinimized(), "title-strip minimize button minimizes the window");

    const QList<QMessageBox*> errorsBeforeShutdown = window.findChildren<QMessageBox*>(QStringLiteral("storage-error-message"));
    const QString errorTextBeforeShutdown = errorsBeforeShutdown.isEmpty() ? QString() : errorsBeforeShutdown.constFirst()->text();
    const QString shutdownSettingsDirectory = QFileInfo(settingsPath).absolutePath();
    expectTrue(QDir(shutdownSettingsDirectory).removeRecursively(), "remove settings directory for shutdown failure test");
    QFile shutdownSettingsBlocker(shutdownSettingsDirectory);
    expectTrue(shutdownSettingsBlocker.open(QIODevice::WriteOnly | QIODevice::Text),
               "file blocks persistence during shutdown");
    shutdownSettingsBlocker.write("not a directory\n");
    shutdownSettingsBlocker.close();
    window.shutdownForSignal();
    processWindowEvents();
    const QList<QMessageBox*> errorsAfterShutdown = window.findChildren<QMessageBox*>(QStringLiteral("storage-error-message"));
    expectEqual(errorsAfterShutdown.size(), errorsBeforeShutdown.size(),
                "shutdown persistence failures do not open another error box");
    if (!errorsAfterShutdown.isEmpty()) {
        expectEqual(errorsAfterShutdown.constFirst()->text(), errorTextBeforeShutdown,
                    "shutdown persistence failures do not update the visible user error");
    }

    window.setPersistenceEnabled(false);
    window.close();

    nsl::MainWindow untrustedWindow(true);
    expectTrue(QFile::remove(shutdownSettingsDirectory), "remove unreadable settings blocker after untrusted load");
    expectTrue(QDir().mkpath(shutdownSettingsDirectory), "make settings path writable before untrusted shutdown");
    untrustedWindow.shutdownForSignal();
    processWindowEvents();
    expectTrue(!QFile::exists(settingsPath),
               "session with failed initial settings read cannot overwrite unknown persistence with defaults");
    untrustedWindow.setPersistenceEnabled(false);
    untrustedWindow.close();

    if (failures != 0) {
        std::cerr << failures << " main window test failure(s)\n";
        return 1;
    }
    return 0;
}
