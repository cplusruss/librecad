/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software 
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!  
**
**********************************************************************/

#include "rs_actionzoomauto.h"

#include <algorithm>
#include <vector>

#include "lc_graphicviewport.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"


/**
 * Constructor.
 *
 * @param keepAspectRatio true: keep same zoom value for x/y.
 *                        false: adjust both x and y individually
 */
RS_ActionZoomAuto::RS_ActionZoomAuto(LC_ActionContext *actionContext, bool keepAspectRatio)
        :RS_ActionInterface("Auto zoom", actionContext, RS2::ActionZoomAuto) {
    this->m_keepAspectRatio = keepAspectRatio;
}

void RS_ActionZoomAuto::init(int status) {
    RS_ActionInterface::init(status);
    trigger();
}

void RS_ActionZoomAuto::trigger() {
    if (zoomToSelection()) {
        finish(false);
        return;
    }
    m_viewport->zoomAuto(false, m_keepAspectRatio);
    finish(false);
}

bool RS_ActionZoomAuto::zoomToSelection() {
    if (m_actionContext == nullptr || m_container == nullptr || m_viewport == nullptr)
        return false;
    if (m_actionContext->getSelectedEntitiesCount() <= 0)
        return false;

    std::vector<RS_Entity*> selected;
    m_container->collectSelected(selected, true);

    RS_Vector mn{RS_MAXDOUBLE, RS_MAXDOUBLE};
    RS_Vector mx{-RS_MAXDOUBLE, -RS_MAXDOUBLE};
    bool any = false;
    for (RS_Entity* e : selected) {
        if (e == nullptr) continue;
        RS_Vector eMin = e->getMin();
        RS_Vector eMax = e->getMax();
        if (!eMin.valid || !eMax.valid) continue;
        mn.x = std::min(mn.x, eMin.x);
        mn.y = std::min(mn.y, eMin.y);
        mx.x = std::max(mx.x, eMax.x);
        mx.y = std::max(mx.y, eMax.y);
        any = true;
    }
    if (!any) return false;

    double dx = mx.x - mn.x;
    double dy = mx.y - mn.y;
    double pad = std::max({dx, dy, 1.0}) * 0.05;
    mn.x -= pad; mn.y -= pad;
    mx.x += pad; mx.y += pad;

    m_viewport->zoomWindow(mn, mx, m_keepAspectRatio);
    return true;
}
