#include "TextPane.h"

#include <QPainter>
#include <utility>

namespace nsl {

TextPane::TextPane(QString title, int preferredHeight, QWidget* parent)
    : PaneWidget(std::move(title), preferredHeight, parent) {}

void TextPane::setRows(const QVector<QPair<QString, QString>>& rows) {
    rows_ = rows;
    columns_.clear();
    update();
}

void TextPane::setColumns(const QVector<QPair<QString, QString>>& columns) {
    columns_ = columns;
    rows_.clear();
    update();
}

void TextPane::paintContent(QPainter& painter, const QRect& contentRect) {
    painter.setFont(labelFont());
    painter.setPen(valueColor());

    if (!columns_.isEmpty()) {
        const int count = columns_.size();
        const int colWidth = contentRect.width() / count;
        for (int i = 0; i < count; ++i) {
            const QRect col(contentRect.left() + i * colWidth, contentRect.top(), colWidth, contentRect.height());
            painter.setFont(labelFont());
            painter.setPen(valueColor());
            painter.drawText(col.adjusted(0, 0, 0, -14), Qt::AlignHCenter | Qt::AlignTop, columns_[i].first);
            painter.setFont(valueFont());
            painter.drawText(col.adjusted(0, 12, 0, 0), Qt::AlignHCenter | Qt::AlignTop, columns_[i].second);
        }
        return;
    }

    int y = contentRect.top() + 1;
    for (const auto& row : rows_) {
        painter.setFont(labelFont());
        painter.setPen(valueColor());
        painter.drawText(QRect(contentRect.left(), y, 50, 12), Qt::AlignLeft | Qt::AlignVCenter, row.first);
        painter.setFont(labelFont());
        painter.drawText(QRect(contentRect.left() + 55, y, contentRect.width() - 55, 12), Qt::AlignLeft | Qt::AlignVCenter, row.second);
        y += 12;
    }
}

} // namespace nsl
