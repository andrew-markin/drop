#include "Colors.h"

namespace {

static const qreal ProgressHue = 120.0 / 360;
static const qreal ErrorHue = 0.0;
static const qreal MinimumSaturation = 0.45;
static const qreal MinimumLightness = 0.35;
static const qreal MaximumLightness = 0.6;

QColor withHue(const QColor& color, qreal hue)
{
    QColor hsl = color.toHsl();
    return QColor::fromHslF(float(hue), float(qMax(qreal(hsl.hslSaturationF()), MinimumSaturation)),
                            float(qBound(MinimumLightness, qreal(hsl.lightnessF()), MaximumLightness)));
}

} // namespace

namespace Colors {

QColor mix(const QColor& from, const QColor& to, qreal amount)
{
    return QColor::fromRgbF(float(from.redF() + (to.redF() - from.redF()) * amount),
                            float(from.greenF() + (to.greenF() - from.greenF()) * amount),
                            float(from.blueF() + (to.blueF() - from.blueF()) * amount));
}

QColor progress(const QPalette& palette, QPalette::ColorGroup group)
{
    return withHue(palette.color(group, QPalette::Highlight), ProgressHue);
}

QColor error(const QPalette& palette, QPalette::ColorGroup group)
{
    return withHue(palette.color(group, QPalette::Highlight), ErrorHue);
}

} // namespace Colors
