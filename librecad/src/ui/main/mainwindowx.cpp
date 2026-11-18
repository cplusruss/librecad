/*
**********************************************************************************
**
** This file was created for the LibreCAD project (librecad.org), a 2D CAD program.
**
** Copyright (C) 2015 ravas (github.com/r-a-v-a-s)
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
**********************************************************************************
*/

#include "mainwindowx.h"

#include <QDockWidget>
#include <QToolBar>

namespace
{
    namespace Sorting
    {
        bool byWindowTitle(QWidget* left, QWidget* right) {
            return left->windowTitle() < right->windowTitle();
        }

        /**
             * @brief getGroup find the integer widget property ("_group"), the default value is -100
             * @param widget - a widget
             * @return the "_group" property
             */
        int getGroup(const QWidget* widget) {
            const int defaultGroup = -100;
            if (widget == nullptr)
                return defaultGroup;
            const QVariant groupProperty = widget->property("_group");
            bool okay = false;
            const int ret = groupProperty.toInt(&okay);
            return okay ? ret : defaultGroup;
        }

        bool byGroupAndWindowTitle(QWidget* left, QWidget* right) {
            const int iLeftGroup = getGroup(left);
            const int iRightGroup = getGroup(right);

            //        LC_ERR << iLeftGroup << " " << iRightGroup << " " << result;

            return (iLeftGroup < iRightGroup)
                || (iLeftGroup == iRightGroup
                    && QString::compare(left->windowTitle(), right->windowTitle()) < 0);
        }
    }
}

MainWindowX::MainWindowX(QWidget* parent)
    : QMainWindow(parent) {
}

void MainWindowX::sortWidgetsByTitle(QList<QDockWidget*>& list) {
    std::sort(list.begin(), list.end(), Sorting::byWindowTitle);
}

void MainWindowX::sortWidgetsByTitle(QList<QToolBar*>& list) {
    std::sort(list.begin(), list.end(), Sorting::byWindowTitle);
}

void MainWindowX::sortWidgetsByGroupAndTitle(QList<QToolBar*>& list) {
    std::sort(list.begin(), list.end(), Sorting::byGroupAndWindowTitle);
}

/**
 * Hide/restore pair for one dock area.
 *
 * Switching the area on used to setVisible(true) on every dock widget assigned
 * to it, which unhid every tool palette the user had closed. Instead, remember
 * what was visible when the area was switched off and restore exactly that.
 * With nothing recorded there is nothing to restore, so the layout is left
 * alone rather than guessed at.
 */
void MainWindowX::toggleDockArea(Qt::DockWidgetArea area, bool state) {
    if (state) {
        const auto it = m_hiddenByAreaToggle.find(area);
        if (it == m_hiddenByAreaToggle.end()) {
            return;
        }
        for (const QPointer<QDockWidget>& dw: *it) {
            if (!dw.isNull())
                dw->setVisible(true);
        }
        m_hiddenByAreaToggle.erase(it);
        return;
    }
    QList<QPointer<QDockWidget>> hidden;
    foreach(QDockWidget* dw, findChildren<QDockWidget*>()) {
        if (dockWidgetArea(dw) == area && !dw->isFloating() && dw->isVisible()) {
            hidden.append(dw);
            dw->setVisible(false);
        }
    }
    m_hiddenByAreaToggle.insert(area, hidden);
}

void MainWindowX::toggleLeftDockArea(bool state) {
    toggleDockArea(Qt::LeftDockWidgetArea, state);
}

void MainWindowX::toggleRightDockArea(bool state) {
    toggleDockArea(Qt::RightDockWidgetArea, state);
}

void MainWindowX::toggleTopDockArea(bool state) {
    toggleDockArea(Qt::TopDockWidgetArea, state);
}

void MainWindowX::toggleBottomDockArea(bool state) {
    toggleDockArea(Qt::BottomDockWidgetArea, state);
}

void MainWindowX::toggleFloatingDockwidgets(bool state) {
    foreach(QDockWidget* dw, findChildren<QDockWidget*>()) {
        if (dw->isFloating())
            dw->setVisible(state);
    }
}
