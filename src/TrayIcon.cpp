#include "TrayIcon.h"
#include "PaneWidget.h"

#include <QMenu>
#include <QPainter>

namespace nsl {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent) {
    tray_.setToolTip(QStringLiteral("NSL-Linux"));
    tray_.setIcon(buildIcon(false, false, -1));
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            Q_EMIT toggleRequested();
        }
    });
    tray_.show();
}

void TrayIcon::setContextMenu(QMenu* menu) {
    tray_.setContextMenu(menu);
}

bool TrayIcon::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::updateFromSnapshot(const CollectorSnapshot& snapshot) {
    const bool txActive = snapshot.txDelta > 0;
    const bool rxActive = snapshot.rxDelta > 0;
    int activityBucket = 2;
    if (snapshot.lastActivityAgeMs >= 0 && snapshot.lastActivityAgeMs < 60000) {
        activityBucket = 0;
    } else if (snapshot.lastActivityAgeMs >= 0 && snapshot.lastActivityAgeMs < 120000) {
        activityBucket = 1;
    }
    if (txActive == lastTxActive_ && rxActive == lastRxActive_ && activityBucket == lastActivityBucket_) {
        return;
    }
    lastTxActive_ = txActive;
    lastRxActive_ = rxActive;
    lastActivityBucket_ = activityBucket;
    tray_.setIcon(buildIcon(txActive, rxActive, snapshot.lastActivityAgeMs));

    const QString activity = snapshot.lastActivityAgeMs < 0
        ? QStringLiteral("no activity yet")
        : QStringLiteral("last activity %1s ago").arg(snapshot.lastActivityAgeMs / 1000);
    tray_.setToolTip(QStringLiteral("NSL-Linux\nDown %1  Up %2\n%3")
                         .arg(QString::fromStdString(formatRate(snapshot.rxRate, UnitMode::Bytes)),
                              QString::fromStdString(formatRate(snapshot.txRate, UnitMode::Bytes)),
                              activity));
}

QIcon TrayIcon::buildIcon(bool txActive, bool rxActive, qint64 lastActivityAgeMs) const {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(4, 3, 24, 22);
    const QColor dark(20, 20, 20);
    const QColor border = PaneWidget::valueColor();
    painter.setPen(QPen(border, 1.2));
    painter.setBrush(dark);
    painter.drawRoundedRect(body, 4, 4);

    const QRectF left(5, 4, 11, 20);
    const QRectF right(16, 4, 11, 20);
    painter.setPen(Qt::NoPen);
    painter.setBrush(txActive ? Qt::white : QColor(28, 28, 28));
    painter.drawRect(left);
    painter.setBrush(rxActive ? Qt::white : QColor(28, 28, 28));
    painter.drawRect(right);

    painter.setPen(QPen(border, 1));
    painter.drawLine(QPointF(16, 5), QPointF(16, 24));

    QColor triangle = Qt::red;
    if (lastActivityAgeMs >= 0 && lastActivityAgeMs < 60000) {
        triangle = QColor(0x00, 0xd8, 0x00);
    } else if (lastActivityAgeMs >= 0 && lastActivityAgeMs < 120000) {
        triangle = QColor(0xff, 0xd0, 0x00);
    }
    QPolygonF tri;
    tri << QPointF(11, 25) << QPointF(21, 25) << QPointF(16, 31);
    painter.setBrush(triangle);
    painter.setPen(QPen(QColor(0, 0, 0), 1));
    painter.drawPolygon(tri);

    return QIcon(pixmap);
}

} // namespace nsl
