#include "lc_appicon.h"

#include <QApplication>
#include <QDebug>
#include <QSize>

#include "rs_settings.h"

namespace {
const QStringList kKeys{"flat", "darktile", "bold", "lighttile", "classic"};
const QStringList kLabels{"Flat", "Dark tile", "Bold", "Light tile", "Classic"};
constexpr int kSizes[] = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
}

QStringList LC_AppIcon::keys() {
    return kKeys;
}

QString LC_AppIcon::label(const QString& key) {
    const qsizetype i = kKeys.indexOf(key);
    if (i < 0) {
        qWarning() << "LC_AppIcon::label: unknown app icon key" << key;
        return kLabels.first();
    }
    return kLabels.at(i);
}

QIcon LC_AppIcon::icon(const QString& key) {
    QString use = key;
    if (!kKeys.contains(key)) {
        qWarning() << "LC_AppIcon::icon: unknown app icon key" << key;
        use = kKeys.first();
    }
    QIcon ic;
    for (int s : kSizes)
        ic.addFile(QStringLiteral(":/images/appicon/%1_%2.png").arg(use).arg(s), QSize(s, s));
    return ic;
}

QString LC_AppIcon::currentKey() {
    return LC_GET_ONE_STR("Appearance", "AppIcon", kKeys.first());
}

void LC_AppIcon::setCurrentKey(const QString& key) {
    LC_SET_ONE("Appearance", "AppIcon", key);
}

void LC_AppIcon::apply() {
    QApplication::setWindowIcon(icon(currentKey()));
}
