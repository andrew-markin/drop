#ifndef CINNAMONTHEME_H
#define CINNAMONTHEME_H

#include <QColor>
#include <QString>

// The panel of Cinnamon has a theme of its own, the "Desktop" one of its settings, apart from the
// one of applications that Qt follows. Its color is told only by the style sheet of the theme.
namespace CinnamonTheme {

bool isCurrentDesktop();

// Empty for the default theme of Cinnamon, a null string when it is not known
QString currentName();

// Of the default theme for an empty name, empty when there is no such theme
QString styleSheetPath(const QString& name);

// Of a horizontal panel, invalid when the style sheet does not tell it
QColor panelColor(const QString& styleSheet);

} // namespace CinnamonTheme

#endif // CINNAMONTHEME_H
