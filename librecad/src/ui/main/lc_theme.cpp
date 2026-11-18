#include "lc_theme.h"

#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

#include "qc_mdiwindow.h"
#include "rs_settings.h"

namespace {
const QStringList kKeys{"classic", "win11dark"};
const QStringList kLabels{"Windows Classic", "Windows 11 Dark"};
}

QStringList LC_Theme::keys() {
    return kKeys;
}

QString LC_Theme::label(const QString& key) {
    const qsizetype i = kKeys.indexOf(key);
    if (i < 0) {
        qWarning() << "LC_Theme::label: unknown theme key" << key;
        return kLabels.first();
    }
    return kLabels.at(i);
}

QString LC_Theme::currentKey() {
    const QString key = LC_GET_ONE_STR("Appearance", "Theme", kKeys.first());
    if (!kKeys.contains(key)) {
        qWarning() << "LC_Theme::currentKey: unknown theme key" << key;
        return kKeys.first();
    }
    return key;
}

void LC_Theme::setCurrentKey(const QString& key) {
    LC_SET_ONE("Appearance", "Theme", key);
}

bool LC_Theme::isClassic() {
    return currentKey() == kKeys.first();
}

void LC_Theme::apply() {
    if (!isClassic()) {
        // the windows11 style takes its palette from the color scheme
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        if (QStyle* s = QStyleFactory::create(QStringLiteral("windows11"))) {
            QApplication::setStyle(s);
        }
        else {
            qWarning() << "LC_Theme::apply: windows11 style plugin (styles/qmodernwindowsstyle.dll) not found";
        }
        return;
    }
#ifdef Q_OS_WIN
    // Classic native Windows look. "Windows" is built into Qt; no plugin needed.
    // The proxy adds native caption buttons for MDI children, including the
    // maximized-child controls drawn in the menubar.
    if (QStyle* classic = QC_MDIWindow::classicStyle()) {
        QApplication::setStyle(classic);
    }
    else {
        QApplication::setStyle(QStyleFactory::create("Windows"));
    }
#endif
    // Highlight color used for menubar/menu selection. Navy on white, applied
    // to Active AND Inactive groups so out-of-focus menus don't fall back to
    // the platform's black-on-blue default (which is unreadable).
    QPalette pal = QApplication::palette();
    const QColor navy(10, 36, 106);
    const QColor white(Qt::white);
    for (auto grp : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        pal.setColor(grp, QPalette::Highlight, navy);
        pal.setColor(grp, QPalette::HighlightedText, white);
    }
    QApplication::setPalette(pal);
}
