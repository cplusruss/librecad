#ifndef LC_THEME_H
#define LC_THEME_H

#include <QString>
#include <QStringList>

// Application theme, read once at startup (a change takes effect on restart).
namespace LC_Theme {
QStringList keys();
QString label(const QString& key);
QString currentKey();
void setCurrentKey(const QString& key);
bool isClassic();
// style, color scheme and palette for the current theme; call after RS_Settings::init, before any widget exists
void apply();
}

#endif
