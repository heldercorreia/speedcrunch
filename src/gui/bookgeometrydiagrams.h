// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_BOOKGEOMETRYDIAGRAMS_H
#define GUI_BOOKGEOMETRYDIAGRAMS_H

#include <QSizeF>
#include <QStringView>

class QColor;
class QPainter;
class QRectF;

namespace BookGeometryDiagrams {

inline constexpr QSizeF canvasSize{144, 108};

void draw(QPainter& painter, const QRectF& rect, QStringView shape,
          const QColor& foreground, const QColor& background);

}

#endif // GUI_BOOKGEOMETRYDIAGRAMS_H
