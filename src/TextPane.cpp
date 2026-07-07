#include "TextPane.h"

#include <QPainter>
#include <utility>

namespace nsl {
namespace {

constexpr int RowHeight = 14;
constexpr int ColumnLabelHeight = 14;
constexpr int ColumnValueOffset = 13;

Qt::Alignment columnAlignment(int index, int count) {
    if (index == 0) {
        return Qt::AlignLeft | Qt::AlignTop;
    }
    if (index == count - 1) {
        return Qt::AlignRight | Qt::AlignTop;
    }
    return Qt::AlignHCenter | Qt::AlignTop;
}

} // namespace

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
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!columns_.isEmpty()) {
        const int count = columns_.size();
        const int colWidth = contentRect.width() / count;
        for (int i = 0; i < count; ++i) {
            const int left = contentRect.left() + i * colWidth;
            const int width = (i == count - 1) ? contentRect.right() - left + 1 : colWidth;
            const QRect col(left, contentRect.top(), width, contentRect.height());
            painter.setFont(labelFont());
            painter.setPen(dimColor());
            painter.drawText(QRect(col.left(), col.top() - 1, col.width(), ColumnLabelHeight), columnAlignment(i, count), columns_[i].first);
            painter.setFont(valueFont());
            painter.setPen(valueColor());
            painter.drawText(QRect(col.left(), col.top() + ColumnValueOffset, col.width(), col.height() - ColumnValueOffset), columnAlignment(i, count), columns_[i].second);
        }
        return;
    }

    int y = contentRect.top();
    for (const auto& row : rows_) {
        painter.setFont(labelFont());
        painter.setPen(dimColor());
        painter.drawText(QRect(contentRect.left(), y, 55, RowHeight), Qt::AlignLeft | Qt::AlignVCenter, row.first);
        painter.setFont(valueFont());
        painter.setPen(valueColor());
        painter.drawText(QRect(contentRect.left() + 60, y, contentRect.width() - 60, RowHeight), Qt::AlignLeft | Qt::AlignVCenter, row.second);
        y += RowHeight;
    }
}

} // namespace nsl
