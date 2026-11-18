/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2024 LibreCAD.org
 Copyright (C) 2024 sand1024

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ******************************************************************************/

#include "lc_dialog.h"

#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>

#include "qc_applicationwindow.h"
#include "rs_settings.h"

// fixme - sand - review all dialogs and actions and make all conversions (double/string, angle/string and vise versa consistent)!
LC_Dialog::LC_Dialog(QWidget* parent, const QString& dlgName)
    :QDialog(parent)
    ,m_dialogName(dlgName){
}

void LC_Dialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (!m_positionLoaded) {
        loadDialogPosition();
        m_positionLoaded = true;
    }
}

QString LC_Dialog::getPositionSettingsGroupName() const{
    return "Dlg"+m_dialogName;
}

namespace {
// Reference frame for relative dialog positioning. Dialogs are anchored to
// this widget's frame top-left so they follow it across monitors.
QWidget* anchorWindow() {
    auto& mw = QC_ApplicationWindow::getAppWindow();
    return mw.get();
}

// Pick the screen the anchor currently lives on. screenAt(center) handles the
// case where a window straddles two monitors; falls back to the widget's own
// screen, then to the primary, so we never return null in a normal session.
QScreen* anchorScreen(QWidget* anchor) {
    if (anchor != nullptr) {
        if (auto* s = QGuiApplication::screenAt(anchor->frameGeometry().center())) return s;
        if (auto* s = anchor->screen()) return s;
    }
    return QGuiApplication::primaryScreen();
}

// Keep the rect inside the available area of the anchor's screen. Width/height
// are capped first so a too-large saved size doesn't push the dialog off-screen.
QRect clampToScreen(QRect r, QScreen* screen) {
    if (screen == nullptr) return r;
    const QRect bounds = screen->availableGeometry();
    if (r.width()  > bounds.width())  r.setWidth(bounds.width());
    if (r.height() > bounds.height()) r.setHeight(bounds.height());
    if (r.right()  > bounds.right())  r.moveRight(bounds.right());
    if (r.bottom() > bounds.bottom()) r.moveBottom(bounds.bottom());
    if (r.left()   < bounds.left())   r.moveLeft(bounds.left());
    if (r.top()    < bounds.top())    r.moveTop(bounds.top());
    return r;
}
} // namespace

void LC_Dialog::saveRelativeGeometry(const QWidget* dlg, const QString& key) {
    if (dlg == nullptr || key.isEmpty()) return;
    QWidget* anchor = anchorWindow();
    const QPoint anchorPos = (anchor != nullptr) ? anchor->frameGeometry().topLeft() : QPoint(0, 0);
    const QPoint dlgPos = dlg->frameGeometry().topLeft();
    const QPoint delta = dlgPos - anchorPos;

    LC_GROUP_GUARD("DialogAnchor");
    {
        LC_GROUP_GUARD(key);
        LC_SET("hasGeometry", true);
        LC_SET("DX", delta.x());
        LC_SET("DY", delta.y());
        LC_SET("Width", dlg->width());
        LC_SET("Height", dlg->height());
    }
}

void LC_Dialog::loadRelativeGeometry(QWidget* dlg, const QString& key,
                                     const QSize& fallbackSize) {
    if (dlg == nullptr || key.isEmpty()) return;
    bool hasGeometry = false;
    int dx = 0, dy = 0, w = 0, h = 0;
    {
        LC_GROUP_GUARD("DialogAnchor");
        {
            LC_GROUP_GUARD(key);
            hasGeometry = LC_GET_BOOL("hasGeometry", false);
            dx = LC_GET_INT("DX", 0);
            dy = LC_GET_INT("DY", 0);
            w  = LC_GET_INT("Width", 0);
            h  = LC_GET_INT("Height", 0);
        }
    }
    if (!hasGeometry || w <= 0 || h <= 0) {
        if (fallbackSize.isValid() && !fallbackSize.isEmpty()) dlg->resize(fallbackSize);
        return;
    }
    QWidget* anchor = anchorWindow();
    const QPoint anchorPos = (anchor != nullptr) ? anchor->frameGeometry().topLeft() : QPoint(0, 0);
    QRect target(anchorPos + QPoint(dx, dy), QSize(w, h));
    target = clampToScreen(target, anchorScreen(anchor));
    dlg->resize(target.size());
    dlg->move(target.topLeft());
}

void LC_Dialog::loadDialogPosition() {
    const bool persistentDialogPositions = LC_GET_ONE_BOOL("Appearance", "PersistDialogPositions", false);
    if (!persistentDialogPositions) return;
    const bool restoreSizeOnly = LC_GET_ONE_BOOL("Appearance", "PersistDialogRestoreSizeOnly", false);
    const QPoint before = pos();
    loadRelativeGeometry(this, m_dialogName);
    // Size-only mode preserves the user's "open where I clicked" preference
    // while still honoring a remembered size.
    if (restoreSizeOnly) move(before);
}

void LC_Dialog::saveDialogPosition() const {
    bool persistentDialogPositions = LC_GET_ONE_BOOL("Appearance","PersistDialogPositions", false);
    if (!persistentDialogPositions) return;
    saveRelativeGeometry(this, m_dialogName);
}

void LC_Dialog::accept() {
    saveDialogPosition();
    QDialog::accept();
}

void LC_Dialog::reject() {
    saveDialogPosition();
    QDialog::reject();
}
