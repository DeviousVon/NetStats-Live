#pragma once

#include <QColor>
#include <QFont>
#include <QRect>
#include <QString>
#include <QWidget>

namespace nsl {

class PaneWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaneWidget(QString title, int preferredHeight, QWidget* parent = nullptr);
    int preferredHeight() const;
    void setPaneTitle(const QString& title);

    static QColor backgroundColor();
    static QColor valueColor();
    static QColor dimColor();
    static QFont labelFont();
    static QFont valueFont();

protected:
    void paintEvent(QPaintEvent* event) override;
    virtual void paintContent(QPainter& painter, const QRect& contentRect) = 0;

private:
    QString title_;
    int preferredHeight_ = 0;
};

} // namespace nsl
