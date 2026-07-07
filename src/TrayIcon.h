#pragma once

#include "Collector.h"

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

Q_SIGNALS:
    void toggleRequested();

private:
    QIcon buildIcon(bool txActive, bool rxActive, qint64 lastActivityAgeMs) const;

    QSystemTrayIcon tray_;
    bool lastTxActive_ = false;
    bool lastRxActive_ = false;
    int lastActivityBucket_ = -2;
};

} // namespace nsl
