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

#include "rs_actionmodifytrim.h"

#include <algorithm>

#include <QKeyEvent>

#include "lc_actioninfomessagebuilder.h"
#include "lc_graphicviewport.h"
#include "lc_undosection.h"
#include "rs_arc.h"
#include "rs_atomicentity.h"
#include "rs_debug.h"
#include "rs_ellipse.h"
#include "rs_entitycontainer.h"
#include "rs_information.h"
#include "rs_modification.h"

struct RS_ActionModifyTrim::TrimActionData {
    RS_Vector limitCoord;
    RS_Vector trimCoord;
};

/**
 * @param both Trim both entities.
 */
RS_ActionModifyTrim::RS_ActionModifyTrim(LC_ActionContext *actionContext, bool both)
    : LC_OverlayBoxAction("Trim Entity",actionContext, both ? RS2::ActionModifyTrim2 : RS2::ActionModifyTrim)
    , m_trimEntity{nullptr}, m_limitEntity{nullptr}
    , m_actionData(std::make_unique<TrimActionData>()), m_both{both} {
}

RS_ActionModifyTrim::~RS_ActionModifyTrim() = default;

void RS_ActionModifyTrim::init(int status) {
    m_snapMode.clear();
    m_snapMode.restriction = RS2::RestrictNothing;

    // AutoCAD classic trim: harvest the current selection as the cutter set.
    // If anything is pre-selected, skip the "pick limiting entity" step and go
    // straight to clicking targets to trim. Cutters remain visually selected
    // throughout the action and are NOT consumed.
    m_cutterEntities.clear();
    if (m_container != nullptr && m_container->countSelected(true) > 0) {
        m_container->collectSelected(m_cutterEntities, true);
    }

    RS_PreviewActionInterface::init(status);

    if (!m_cutterEntities.empty()) {
        // Already have cutters: jump straight to clicking targets to trim.
        setStatus(ChooseTrimEntity);
    }
}

void RS_ActionModifyTrim::doInitWithContextEntity(RS_Entity* contextEntity, const RS_Vector& clickPos) {
    if (isAtomic(contextEntity)) {
        m_trimEntity = static_cast<RS_AtomicEntity*>(contextEntity);
        m_actionData->trimCoord = clickPos;
    }
}

void RS_ActionModifyTrim::finish(bool updateTB) {
    RS_PreviewActionInterface::finish(updateTB);
}

void RS_ActionModifyTrim::doTrigger() {
    RS_DEBUG->print("RS_ActionModifyTrim::trigger()");

    if (!m_cutterEntities.empty()) {
        // Cutter-set (AutoCAD classic) mode.
        if (m_boxMode) {
            // Box drag: trim every caught entity, one undo step (see trimWindow).
            trimWindow(m_boxCorner1, m_boxCorner2);
            m_boxMode = false;
            m_boxCorner1 = RS_Vector(false);
        } else if (m_crossTarget != nullptr) {
            RS_Modification m(*m_container, m_viewport);
            m.trimCrossing(m_actionData->trimCoord, m_crossTarget, m_cutterEntities, false);
            // The original target is now replaced by its trimmed pieces; drop it
            // from the cutter set so we never reference the removed entity.
            m_cutterEntities.erase(
                std::remove(m_cutterEntities.begin(), m_cutterEntities.end(), m_crossTarget),
                m_cutterEntities.end());
            m_crossTarget = nullptr;
        }
        setStatus(ChooseTrimEntity);
        return;
    }

    if (isAtomic(m_trimEntity) && m_limitEntity /* && limitEntity->isAtomic()*/) {
        RS_Modification m(*m_container, m_viewport);
        [[maybe_unused]] LC_TrimResult trimResult =  m.trim(m_actionData->trimCoord,  m_trimEntity,
               m_actionData->limitCoord, m_limitEntity,
               m_both);

        m_trimEntity = nullptr;
        if (m_both) {
            m_limitEntity = nullptr;
            setStatus(ChooseLimitEntity);
        } else {
            setStatus(ChooseTrimEntity);
        }
    }
}

void RS_ActionModifyTrim::previewTrim(RS_Entity* entityToTrimCandidate, RS_Entity* limitingEntity, RS_Vector trimCoordinates, RS_Vector limitCoordinates, bool& trimInvalid) {
    if (entityToTrimCandidate != nullptr && entityToTrimCandidate != limitingEntity) {
        if (entityToTrimCandidate->isAtomic()) {
            auto *atomicTrimCandidate = dynamic_cast<RS_AtomicEntity *>(entityToTrimCandidate);

            RS_Modification m(*m_container, m_viewport);
            LC_TrimResult trimResult = m.trim(trimCoordinates, atomicTrimCandidate,
                                              limitCoordinates, limitingEntity,
                                              m_both, true);
            if (trimResult.result) {
                trimInvalid = false;
                highlightHover(entityToTrimCandidate);
                if (m_showRefEntitiesOnPreview) {
                    previewRefPoint(trimResult.intersection1);
                    previewRefTrimmedEntity(trimResult.trimmed1, entityToTrimCandidate);
                    if (trimResult.intersection2.valid) {
                        previewRefPoint(trimResult.intersection2);
                    }
                    if (m_both) {
                        if (trimResult.trimmed2 != nullptr) {
                            previewRefTrimmedEntity(trimResult.trimmed2, limitingEntity);
                        }
                    }
                    if (isInfoCursorForModificationEnabled()){
                        auto msg = rtti() == RS2::ActionModifyTrim2 ? tr("Trim Two") : tr("Trim");
                        auto builder = msgStart().string(msg).vector(
                            tr("Intersection:"), trimResult.intersection1);
                        if (trimResult.intersection2.valid) {
                            builder.vector(tr("Intersection 2:"), trimResult.intersection2);
                        }
                        builder.toInfoCursorZone2(false);
                    }
                }
            }
        }
    }
}

// todo - check trim both mode - it seems that limiting entity should be atomic too...
void RS_ActionModifyTrim::onMouseMoveEvent(int status, LC_MouseEvent *e) {
    RS_Vector mouse = e->graphPoint;
    bool trimInvalid = true;
    switch (status) {
        case ChooseLimitEntity: {
            RS_Entity *se = catchAndDescribe(e, RS2::ResolveAllButTextImage);
            if (m_trimEntity != nullptr) {
                previewTrim(m_trimEntity, se, m_actionData->trimCoord, mouse, trimInvalid);
                if (trimInvalid) {
                    highlightSelected(m_trimEntity);
                }
            }
            else  if (se != nullptr) {
                highlightHover(se);
            }

            break;
        }
        case ChooseTrimEntity: {
            if (!m_cutterEntities.empty()) {
                // Grow the click into a box drag once past the threshold.
                if (m_boxCorner1.valid &&
                    m_viewport->toGuiDX(m_boxCorner1.distanceTo(mouse)) > 10.0) {
                    m_boxMode = true;
                }
                if (m_boxMode) {
                    // Cutter-set (AutoCAD classic) mode: rubber-band selection box.
                    drawOverlayBox(m_boxCorner1, mouse);
                } else {
                    // Preview trimming/splitting the hovered target.
                    RS_Entity *entityToTrimCandidate = catchAndDescribe(e, RS2::ResolveNone);
                    previewCrossing(entityToTrimCandidate, mouse);
                }
            } else {
                RS_Entity *entityToTrimCandidate = catchAndDescribe(e, RS2::ResolveNone);
                previewTrim(entityToTrimCandidate, m_limitEntity, mouse, m_actionData->limitCoord, trimInvalid);
                if (trimInvalid) {
                    highlightSelected(m_limitEntity);
                }
            }
            break;
        }
        default:
            break;
    }
}

void RS_ActionModifyTrim::onMouseLeftButtonRelease(int status, LC_MouseEvent *e) {
    RS_Vector mouse = e->graphPoint;
    switch (status) {
        case ChooseLimitEntity: {
            RS_Entity *se = catchEntityByEvent(e, RS2::ResolveAllButTextImage);
            if (se != nullptr) {
                m_limitEntity = se;
                if (m_limitEntity->rtti() != RS2::EntityPolyline/*&& limitEntity->isAtomic()*/) {
                    m_actionData->limitCoord = mouse;
                    if (m_trimEntity != nullptr) {
                        trigger();
                    }
                    else {
                        setStatus(ChooseTrimEntity);
                    }
                }
            }
            break;
        }
        case ChooseTrimEntity: {
            RS_Entity *se = catchEntityByEvent(e, RS2::ResolveNone);
            if (!m_cutterEntities.empty()) {
                // Cutter-set (AutoCAD classic) mode. Route through trigger() so the
                // preview/overlay box is cleared and the view redraws.
                if (m_boxMode) {
                    m_boxCorner2 = mouse;
                    trigger();
                } else if (isCrossingTarget(se)) {
                    m_crossTarget = se;
                    m_actionData->trimCoord = mouse;
                    trigger();
                }
                m_boxCorner1 = RS_Vector(false);
            } else if (isAtomic(se) && se != m_limitEntity) {
                m_actionData->trimCoord = mouse;
                m_trimEntity = dynamic_cast<RS_AtomicEntity*>(se);
                trigger();
            }
            break;
        }
        default:
            break;
    }
}

void RS_ActionModifyTrim::onMouseLeftButtonPress(int status, LC_MouseEvent *e) {
    // In cutter-set mode remember where the press started so a drag can grow into
    // a selection box (the "lasso"); a plain click leaves m_boxMode false.
    if (status == ChooseTrimEntity && !m_cutterEntities.empty()) {
        m_boxCorner1 = e->graphPoint;
        m_boxMode = false;
    }
}

void RS_ActionModifyTrim::trimWindow(const RS_Vector& corner1, const RS_Vector& corner2) {
    if (!corner1.valid || !corner2.valid)
        return;
    // Window box (WCS). Left-to-right encloses (entity fully inside); right-to-left
    // crosses (bounding boxes overlap). (Assumes an unrotated UCS.)
    const bool crossing = corner1.x > corner2.x;
    const RS_Vector wmin(std::min(corner1.x, corner2.x), std::min(corner1.y, corner2.y));
    const RS_Vector wmax(std::max(corner1.x, corner2.x), std::max(corner1.y, corner2.y));
    const RS_Vector boxCenter = (corner1 + corner2) / 2.0;

    // Collect the caught entities first (trimming mutates the container).
    std::vector<RS_Entity*> targets;
    for (RS_Entity* e : *m_container) {
        if (e == nullptr || !e->isVisible() || e->isLocked() || !isCrossingTarget(e))
            continue;
        bool caught;
        if (crossing) {
            const RS_Vector emin = e->getMin();
            const RS_Vector emax = e->getMax();
            caught = !(emax.x < wmin.x || emin.x > wmax.x || emax.y < wmin.y || emin.y > wmax.y);
        } else {
            caught = e->isInWindow(wmin, wmax);
        }
        if (caught)
            targets.push_back(e);
    }
    if (targets.empty())
        return;

    // Trim each caught entity against the ORIGINAL cutter set, all as one undo
    // step (nested trimCrossing undo sections join this outer cycle by refcount).
    const std::vector<RS_Entity*> cutterSnapshot = m_cutterEntities;
    LC_UndoSection undo(m_document, m_viewport);
    RS_Modification m(*m_container, m_viewport);
    for (RS_Entity* t : targets) {
        const RS_Vector click = t->getNearestPointOnEntity(boxCenter, true);
        m.trimCrossing(click, t, cutterSnapshot, false);
    }
    // Drop the trimmed originals from the live cutter set (their pieces replace
    // them); their geometry was still valid for cutting during this batch.
    for (RS_Entity* t : targets) {
        m_cutterEntities.erase(
            std::remove(m_cutterEntities.begin(), m_cutterEntities.end(), t),
            m_cutterEntities.end());
    }
}

void RS_ActionModifyTrim::onMouseRightButtonRelease(int status, [[maybe_unused]] LC_MouseEvent *e) {
    deletePreview();
    if (!m_cutterEntities.empty()) {
        // In cutter-set mode right-click finishes (matches AutoCAD's Enter/Esc).
        finish(true);
        return;
    }
    initPrevious(status);
}

void RS_ActionModifyTrim::keyPressEvent(QKeyEvent* e) {
    if (e == nullptr) return;
    const int key = e->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape) {
        e->accept();
        finish(true);
        return;
    }
    RS_PreviewActionInterface::keyPressEvent(e);
}

bool RS_ActionModifyTrim::isCrossingTarget(const RS_Entity* e) const {
    // Any atomic entity or polyline can be clicked to trim. A selected entity is
    // both a boundary AND trimmable, exactly like AutoCAD: trimCrossing() skips
    // the target against itself when finding crossings.
    if (e == nullptr)
        return false;
    return e->isAtomic() || e->rtti() == RS2::EntityPolyline;
}

void RS_ActionModifyTrim::previewCrossing(RS_Entity* target, const RS_Vector& clickPos) {
    if (!isCrossingTarget(target))
        return;
    RS_Modification m(*m_container, m_viewport);
    LC_TrimResult r = m.trimCrossing(clickPos, target, m_cutterEntities, true);
    if (!r.result)
        return;
    highlightHover(target);
    // The trimmed/split result pieces are owned by the preview once handed over.
    if (r.trimmed1 != nullptr)
        previewEntity(r.trimmed1);
    if (r.trimmed2 != nullptr)
        previewEntity(r.trimmed2);
    if (m_showRefEntitiesOnPreview) {
        if (r.intersection1.valid)
            previewRefPoint(r.intersection1);
        if (r.intersection2.valid)
            previewRefPoint(r.intersection2);
    }
}

void RS_ActionModifyTrim::updateMouseButtonHints() {
    switch (getStatus()) {
        case ChooseLimitEntity:
            if (m_both) {
                updateMouseWidgetTRCancel(tr("Select first trim entity"));
            } else {
                updateMouseWidgetTRBack(tr("Select limiting entity"));
            }
            break;
        case ChooseTrimEntity:
            if (!m_cutterEntities.empty()) {
                updateMouseWidgetTRBack(tr("Click or drag a box to trim (Enter/Esc to finish)"));
            } else if (m_both) {
                updateMouseWidgetTRCancel(tr("Select second trim entity"));
            } else {
                updateMouseWidgetTRBack(tr("Select entity to trim"));
            }
            break;
        default:
            updateMouseWidget();
            break;
    }
}
RS2::CursorType RS_ActionModifyTrim::doGetMouseCursor([[maybe_unused]] int status){
    return RS2::SelectCursor;
}

void RS_ActionModifyTrim::previewRefTrimmedEntity(RS_Entity *trimmed, RS_Entity* original) {
    int rtti = trimmed->rtti();
    switch (rtti){
        case RS2::EntityLine:{
            RS_Vector start = original->getStartpoint();
            RS_Vector startTrimmed = trimmed->getStartpoint();
            RS_Vector end = original->getEndpoint();
            RS_Vector endTrimmed = trimmed->getEndpoint();
            bool sameStart = start == startTrimmed;
            bool sameEnd = end == endTrimmed;

            if (!sameStart) {
                end = startTrimmed;
            }
            if (!sameEnd) {
                start = endTrimmed;
            }
            previewRefLine(start, end);
            break;
        }
        case RS2::EntityArc:{
            auto* arc = dynamic_cast<RS_Arc *>(trimmed);
            if (arc == nullptr)
                break;
            RS_ArcData data = arc->getData();
            data.reversed = !data.reversed;
            previewRefArc(data);
            break;
        }
        case RS2::EntityCircle:{
            // that's really strange case - not trimmed circle??? 
//            auto* circle = dynamic_cast<RS_Circle*>(trimmed);
//            previewRefCircle(circle->getCenter(), circle->getRadius());
            break;
        }
        case RS2::EntityEllipse:{
            auto* ellipse = dynamic_cast<RS_Ellipse *>(trimmed);
            if (ellipse == nullptr)
                break;
            auto data = ellipse->getData();
            data.reversed = !data.reversed;
            previewRefEllipse(data);
            break;
        }
        case RS2::EntityParabola:{
          // Parabola has no reversed-direction toggle; just preview the
          // resulting (already-trimmed) entity.
          previewEntity(trimmed);
          break;
        }
        default:{
            previewEntity(trimmed);
            RS_DEBUG->print("RS_ActionModifyTrim::unhandled trimmed entity type");
        }
    }
}
