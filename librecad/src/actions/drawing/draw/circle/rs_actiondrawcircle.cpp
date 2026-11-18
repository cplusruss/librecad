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

#include "rs_actiondrawcircle.h"

#include "rs_circle.h"
#include "rs_debug.h"

RS_ActionDrawCircle::RS_ActionDrawCircle(LC_ActionContext *actionContext)
        :LC_ActionDrawCircleBase("Draw circles",actionContext, RS2::ActionDrawCircle)
    , m_circleData(std::make_unique<RS_CircleData>()){
}

RS_ActionDrawCircle::~RS_ActionDrawCircle() = default;

void RS_ActionDrawCircle::reset() {
    m_circleData = std::make_unique<RS_CircleData>();
}

void RS_ActionDrawCircle::doTrigger() {
    auto* circle = new RS_Circle(m_container,*m_circleData);
    setPenAndLayerToActive(circle);

    if (m_moveRelPointAtCenterAfterTrigger){
        moveRelativeZero(circle->getCenter());
    }

    undoCycleAdd(circle);
    setStatus(SetCenter);
    reset();

    RS_DEBUG->print("RS_ActionDrawCircle::trigger(): circle added: %lu",circle->getId());
}

void RS_ActionDrawCircle::onMouseMoveEvent(int status, LC_MouseEvent *e) {
    RS_Vector mouse = e->snapPoint;
    switch (status) {
        case SetCenter: {
            m_circleData->center = mouse;
            trySnapToRelZeroCoordinateEvent(e);
            break;
        }
        case SetRadius:
        case SetDiameter: {
            const auto& center = m_circleData->center;
            if (center.valid){
                double dist = center.distanceTo(mouse);
                m_circleData->radius = (status == SetDiameter) ? dist * 0.5 : dist;
                previewToCreateCircle(*m_circleData);
                if (m_showRefEntitiesOnPreview) {
                    previewRefPoint(center);
                    previewRefSelectablePoint(mouse);
                    previewRefLine(center, mouse);
                }
            }
            break;
        }
        default:
            break;
    }
}

void RS_ActionDrawCircle::onCoordinateEvent(int status, [[maybe_unused]] bool isZero, const RS_Vector &mouse) {
    switch (status) {
        case SetCenter:
            m_circleData->center = mouse;
            moveRelativeZero(mouse);
            setStatus(SetRadius);
            break;
        case SetRadius:
        case SetDiameter:
            if (m_circleData->center.valid){
                moveRelativeZero(mouse);
                double dist = m_circleData->center.distanceTo(mouse);
                m_circleData->radius = (status == SetDiameter) ? dist * 0.5 : dist;
                trigger();
            }
            //setStatus(SetCenter);
            break;
        default:
            break;
    }
}

bool RS_ActionDrawCircle::doProcessCommand(int status, const QString &c) {
    bool accept = false;
    // AutoCAD CIRCLE "D" option: switch from radius entry to diameter entry
    if (status == SetRadius && isDimensionOptionCommand(c) && m_circleData->center.valid) {
        deletePreview();
        setStatus(SetDiameter);
        return true;
    }
    switch (status) {
        case SetRadius:
        case SetDiameter: {
            bool ok = false;
            double value = RS_Math::eval(c, &ok);
            if (ok && value > RS_TOLERANCE){
                m_circleData->radius = (status == SetDiameter) ? value * 0.5 : value;
                accept = true;
                trigger();
            } else
                commandMessage(tr("Not a valid expression"));
            break;
        }
        default:
            break;
    }
    return accept;
}

QStringList RS_ActionDrawCircle::getAvailableCommands() {
    QStringList cmd;
    if (getStatus() == SetRadius) {
        cmd += QStringLiteral("d");
    }
    return cmd;
}

void RS_ActionDrawCircle::updateMouseButtonHints() {
    switch (getStatus()) {
        case SetCenter:
            updateMouseWidgetTRCancel(tr("Specify center"), MOD_SHIFT_RELATIVE_ZERO);
            break;
        case SetRadius:
            updateMouseWidgetTRBack(tr("Specify point on circle or [d]"));
            break;
        case SetDiameter:
            updateMouseWidgetTRBack(tr("Specify diameter"));
            break;
        default:
            updateMouseWidget();
            break;
    }
}
