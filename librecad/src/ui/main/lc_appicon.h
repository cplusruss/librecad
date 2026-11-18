#ifndef LC_APPICON_H
#define LC_APPICON_H

#include <QIcon>
#include <QString>
#include <QStringList>

namespace LC_AppIcon {
QStringList keys();
QString label(const QString& key);
QIcon icon(const QString& key);
QString currentKey();
void setCurrentKey(const QString& key);
void apply();
}

#endif
