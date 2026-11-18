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

#include "rs_actionmodifyoffset.h"

#include <QKeyEvent>

#include "lc_actioninfomessagebuilder.h"
#include "qg_modifyoffsetoptions.h"
#include "rs_math.h"
#include "rs_modification.h"
#include "rs_preview.h"

RS_ActionModifyOffset::RS_ActionModifyOffset(LC_ActionContext *actionContext)
    :LC_ActionModifyBase("Modify Offset", actionContext,RS2::ActionModifyOffset,
                         {RS2::EntityArc, RS2::EntityCircle, RS2::EntityEllipse, RS2::EntityLine, RS2::EntityPolyline},
                         true)
    , m_offsetData(new RS_OffsetData()){

    // Overwritten from the persisted "Distance" setting once the options widget
    // attaches; this only covers the window before that happens.
    m_offsetData->distance = 1.;
    m_offsetData->number = 1;
    m_offsetData->useCurrentAttributes = true;
    m_offsetData->useCurrentLayer = true;
}

// fixme - support remove originals mode
// fixme - number of copies support
// fixme - support attributes support

RS_ActionModifyOffset::~RS_ActionModifyOffset() = default;

void RS_ActionModifyOffset::init(int status) {
    LC_ActionModifyBase::init(status);
    if (status >= 0) {
        // AutoCAD asks for the distance before anything is selected. Anything
        // already selected when the action starts is dropped so the prompt order
        // is the same every time.
        m_distanceConfirmed = false;
        m_selectionComplete = false;
        m_selectedEntities.clear();
        setStatus(SetDistance);
        updateMouseButtonHints();
    }
}

void RS_ActionModifyOffset::confirmDistance(double distance) {
    setDistance(distance);
    m_distanceConfirmed = true;
    // Push the value into the options widget so it is the one persisted as the
    // remembered default, and so the field agrees with what was typed.
    updateOptions();
    restartObjectSelection();
}

void RS_ActionModifyOffset::restartObjectSelection() {
    m_selectionComplete = false;
    m_selectedEntities.clear();
    setStatus(SetDistance);
    updateMouseButtonHints();
}

void RS_ActionModifyOffset::doTrigger(bool keepSelected) {
    RS_Modification m(*m_container, m_viewport);
    m.offset(*m_offsetData, m_selectedEntities, false, keepSelected);
    // Stay in the command and ask for the next object, like AutoCAD. Esc or
    // right-click ends it.
    restartObjectSelection();
}

double RS_ActionModifyOffset::distanceToPickedEntity(const RS_Vector& coord) const {
    double best = RS_MAXDOUBLE;
    for (RS_Entity* e: m_selectedEntities) {
        if (e != nullptr) {
            double d = e->getDistanceToPoint(coord, nullptr, RS2::ResolveNone);
            if (d < best) {
                best = d;
            }
        }
    }
    return best < RS_MAXDOUBLE ? best : 0.;
}

void RS_ActionModifyOffset::onMouseMoveEventSelected(int status, LC_MouseEvent *e) {
    if (status != SetSide) {
        return;
    }
    RS_Vector mouse = e->snapPoint;
    m_offsetData->coord = mouse;
    if (!m_distanceIsFixed) {
        // Through-point mode: the picked point is where the offset passes.
        m_offsetData->distance = distanceToPickedEntity(mouse);
    }
    RS_Modification m(*m_preview, m_viewport, false);
    m.offset(*m_offsetData, m_selectedEntities, true, false);

    if (m_showRefEntitiesOnPreview) {
        previewRefSelectablePoint(mouse);
    }

    if (isInfoCursorForModificationEnabled()){
        msg(tr("Offset"))
            .linear(tr("Distance:"), m_offsetData->distance)
            .toInfoCursorZone2(false);
    }
}

bool RS_ActionModifyOffset::doUpdateDistanceByInteractiveInput(const QString& tag, double distance) {
    if (tag == "distance") {
        setDistance(distance);
        return true;
    }
    return false;
}

void RS_ActionModifyOffset::onMouseLeftButtonRelease(int status, LC_MouseEvent *e) {
    // Ignore picks until the distance prompt has been answered, so the prompt
    // order matches AutoCAD.
    if (!m_distanceConfirmed && !m_selectionComplete) {
        return;
    }
    LC_ActionModifyBase::onMouseLeftButtonRelease(status, e);
}

void RS_ActionModifyOffset::proceedSelectedEntity([[maybe_unused]] LC_MouseEvent* e) {
    // AutoCAD offsets one object per pick - no Ctrl needed, no Enter to close
    // the selection.
    onSelectionCompleted(true, false);
    setStatus(SetSide);
    updateMouseButtonHints();
}

void RS_ActionModifyOffset::onMouseLeftButtonReleaseSelected(int status, LC_MouseEvent *e) {
    if (status != SetSide) {
        return;
    }
    RS_Vector mouse = e->snapPoint;
    m_offsetData->coord = mouse;
    if (!m_distanceIsFixed) {
        m_offsetData->distance = distanceToPickedEntity(mouse);
    }
    trigger();
}

LC_ActionOptionsWidget* RS_ActionModifyOffset::createOptionsWidget() {
    return new QG_ModifyOffsetOptions();
}

double RS_ActionModifyOffset::getDistance() {
    return m_offsetData->distance;
}

void RS_ActionModifyOffset::setDistance(double distance) {
    m_offsetData->distance = distance;
}

void RS_ActionModifyOffset::setDistanceFixed(bool value) {
    m_distanceIsFixed = value;
}

bool RS_ActionModifyOffset::isAllowTriggerOnEmptySelection() {
    return false;
}

void RS_ActionModifyOffset::onMouseRightButtonReleaseSelected([[maybe_unused]] int status,
                                                             [[maybe_unused]] LC_MouseEvent *e) {
    // Back out of the side pick to the object prompt rather than ending, so a
    // misclick does not drop the distance.
    deletePreview();
    restartObjectSelection();
}

void RS_ActionModifyOffset::keyPressEvent(QKeyEvent* e) {
    const int key = e->key();
    if (!m_distanceConfirmed
        && (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space)
        && e->modifiers() == Qt::NoModifier) {
        // Accept the remembered distance, as Enter does at AutoCAD's <default>.
        confirmDistance(m_offsetData->distance);
        e->accept();
        return;
    }
    LC_ActionModifyBase::keyPressEvent(e);
}

bool RS_ActionModifyOffset::doProcessCommand([[maybe_unused]] int status, const QString& c) {
    if (m_distanceConfirmed) {
        return false;
    }
    bool ok = false;
    const double distance = RS_Math::eval(c, &ok);
    if (ok && std::abs(distance) > RS_TOLERANCE) {
        confirmDistance(std::abs(distance));
        return true;
    }
    commandMessage(tr("Not a valid expression"));
    return true;
}

QStringList RS_ActionModifyOffset::getAvailableCommands() {
    return {};
}

void RS_ActionModifyOffset::updateMouseButtonHintsForSelected(int status) {
    if (status == SetSide) {
        updateMouseWidgetTRBack(tr("Specify point on side to offset"));
    }
    else {
        updateMouseWidget();
    }
}

void RS_ActionModifyOffset::updateMouseButtonHintsForSelection() {
    if (m_distanceConfirmed) {
        updateMouseWidgetTRCancel(tr("Select object to offset"), MOD_SHIFT_AND_CTRL(tr("Select contour"), tr("")));
    }
    else {
        updateMouseWidgetTRCancel(tr("Specify offset distance <%1>")
                                      .arg(m_offsetData->distance));
    }
}

LC_ModifyOperationFlags* RS_ActionModifyOffset::getModifyOperationFlags() {
    return m_offsetData.get();
}
