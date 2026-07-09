// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 NAME

#pragma once

#include "PaneWidget.h"

#include <QString>
#include <QVector>

namespace nsl {

class TextPane : public PaneWidget {
    Q_OBJECT
public:
    explicit TextPane(QString title, int preferredHeight, QWidget* parent = nullptr);
    void setRows(const QVector<QPair<QString, QString>>& rows);
    void setColumns(const QVector<QPair<QString, QString>>& columns);

protected:
    void paintContent(QPainter& painter, const QRect& contentRect) override;

private:
    QVector<QPair<QString, QString>> rows_;
    QVector<QPair<QString, QString>> columns_;
};

} // namespace nsl
