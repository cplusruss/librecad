#include "lc_dialogtabkeys.h"

#include <QDialog>
#include <QKeyEvent>
#include <QTabBar>
#include <QTabWidget>

namespace {

QTabBar* tabBarFor(QWidget* w, QDialog* dlg) {
    for (QWidget* p = w; p != nullptr && p != dlg; p = p->parentWidget()) {
        if (auto* tw = qobject_cast<QTabWidget*>(p)) {
            return tw->tabBar();
        }
        if (auto* bar = qobject_cast<QTabBar*>(p)) {
            return bar;
        }
    }
    for (QTabBar* bar : dlg->findChildren<QTabBar*>()) {
        if (bar->isVisibleTo(dlg) && bar->count() > 1) {
            return bar;
        }
    }
    return nullptr;
}

int stepIndex(const QTabBar* bar, int dir) {
    const int n = bar->count();
    for (int i = 1; i < n; ++i) {
        const int idx = ((bar->currentIndex() + dir * i) % n + n) % n;
        if (bar->isTabEnabled(idx) && bar->isTabVisible(idx)) {
            return idx;
        }
    }
    return -1;
}

}

bool LC_DialogTabKeys::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type type = event->type();
    if (type != QEvent::KeyPress && type != QEvent::ShortcutOverride) {
        return false;
    }
    const auto* ke = static_cast<QKeyEvent*>(event);
    const int key = ke->key();
    if (!(ke->modifiers() & Qt::KeypadModifier) || (key != Qt::Key_Plus && key != Qt::Key_Minus)) {
        return false;
    }
    auto* w = qobject_cast<QWidget*>(watched);
    if (w == nullptr) {
        return false;
    }
    auto* dlg = qobject_cast<QDialog*>(w->window());
    if (dlg == nullptr) {
        return false;
    }
    QTabBar* bar = tabBarFor(w, dlg);
    if (bar == nullptr) {
        return false;
    }
    // Claiming the override keeps window shortcuts bound to +/- from firing.
    if (type == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    const int idx = stepIndex(bar, key == Qt::Key_Plus ? 1 : -1);
    if (idx >= 0) {
        bar->setCurrentIndex(idx);
    }
    return true;
}
