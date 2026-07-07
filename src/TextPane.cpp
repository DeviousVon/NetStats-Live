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
            const int left = contentRect.left() + i * colWidth;
            const int width = (i == count - 1) ? contentRect.right() - left + 1 : colWidth;
            const QRect col(left, contentRect.top(), width, contentRect.height());
            painter.setFont(labelFont());
            painter.setPen(valueColor());
            painter.drawText(col.adjusted(0, -1, -1, -12), Qt::AlignRight | Qt::AlignTop, columns_[i].first);
            painter.setFont(valueFont());
            painter.drawText(col.adjusted(0, 10, -1, 0), Qt::AlignRight | Qt::AlignTop, columns_[i].second);
        }
        return;
    }

    int y = contentRect.top();
    for (const auto& row : rows_) {
        painter.setFont(labelFont());
        painter.setPen(valueColor());
        painter.drawText(QRect(contentRect.left(), y, 43, 11), Qt::AlignLeft | Qt::AlignVCenter, row.first);
        painter.drawText(QRect(contentRect.left() + 47, y, contentRect.width() - 47, 11), Qt::AlignLeft | Qt::AlignVCenter, row.second);
        y += 11;
    }
}

} // namespace nsl
