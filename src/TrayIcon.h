#pragma once

#include "Collector.h"
#include "TrayIconVisual.h"

#include <QObject>
#include <QSystemTrayIcon>

namespace nsl {

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void setContextMenu(QMenu* menu);
    void updateFromSnapshot(const CollectorSnapshot& snapshot);
    bool isAvailable() const;
    int iconRegenerationCount() const;

Q_SIGNALS:
    void toggleRequested();

private:
    QSystemTrayIcon tray_;
    TrayIconRenderer renderer_;
};

} // namespace nsl
