#ifndef COLORS_H
#define COLORS_H

#include <QColor>
#include <QPalette>

// Colors the palette has no role for, made from the ones it has, so they suit any theme
namespace Colors {

QColor mix(const QColor& from, const QColor& to, qreal amount);

// Green and red whatever the theme, as a red or green selection color would mix them up.
// Saturation and lightness come from the selection color, within limits for a gray or a too dark
// or light one.
QColor progress(const QPalette& palette, QPalette::ColorGroup group = QPalette::Active);
QColor error(const QPalette& palette, QPalette::ColorGroup group = QPalette::Active);

} // namespace Colors

#endif // COLORS_H
