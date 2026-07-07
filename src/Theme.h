#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace nsl {

struct Theme {
    inline static const QColor Background{0x05, 0x05, 0x05};
    inline static const QColor PanelBackground{0x08, 0x08, 0x08};
    inline static const QColor HeaderText{0x00, 0xe0, 0xe0};
    inline static const QColor ValueText{0x40, 0xe8, 0xe8};
    inline static const QColor GraphFill{0x00, 0xe0, 0xe0};
    inline static const QColor LabelText{0xa8, 0xa0, 0x78};
    inline static const QColor RuleLine{0x00, 0x70, 0x70};
    inline static const QColor DimRuleLine{0x00, 0x32, 0x32};
    inline static const QColor Border{0x30, 0x30, 0x30};
    inline static const QColor TrayInactive{0x1c, 0x1c, 0x1c};
    inline static const QColor TrayWarning{0xff, 0xd0, 0x00};
    inline static const QColor TrayAlert{0xdf, 0x20, 0x20};

    static QFont makeFont(int pointSize, QFont::Weight weight = QFont::Normal, bool smallCaps = false) {
        QFont font;
        font.setFamilies({QStringLiteral("DejaVu Sans Condensed"),
                          QStringLiteral("Noto Sans Condensed"),
                          QStringLiteral("Liberation Sans Narrow"),
                          QStringLiteral("Arial Narrow"),
                          QStringLiteral("Sans Serif")});
        font.setPointSize(pointSize);
        font.setWeight(weight);
        font.setStyleHint(QFont::SansSerif);
        font.setKerning(false);
        if (smallCaps) {
            font.setCapitalization(QFont::SmallCaps);
        }
        return font;
    }

    static QFont headerFont() { return makeFont(9, QFont::DemiBold, true); }
    static QFont labelFont() { return makeFont(9); }
    static QFont valueFont() { return makeFont(9, QFont::DemiBold); }
    static QFont graphValueFont() { return makeFont(13, QFont::Bold); }
};

} // namespace nsl
