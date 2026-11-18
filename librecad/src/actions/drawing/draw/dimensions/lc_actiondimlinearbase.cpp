/****************************************************************************
**
* Abstract base class for linear dimensions

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
**********************************************************************/

#include "lc_actiondimlinearbase.h"

#include <algorithm>
#include <cmath>

#include <QRectF>

#include "lc_dimstyle.h"
#include "lc_dimstyleslist.h"
#include "lc_graphicviewport.h"
#include "lc_linemath.h"
#include "rs_constructionline.h"
#include "rs_debug.h"
#include "rs_dialogfactory.h"
#include "rs_dialogfactoryinterface.h"
#include "rs_actiondimangular.h"
#include "rs_dimaligned.h"
#include "rs_dimension.h"
#include "rs_dimlinear.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_graphic.h"
#include "rs_mtext.h"
#include "rs_polyline.h"
#include "rs_preview.h"

namespace {
// Take a raw mouse position and project it into "dim-line offset space"
// relative to the chord between ext1 and ext2:
//   * parallel-to-chord component is preserved (mouse can slide along)
//   * perpendicular-to-chord component is what controls dim-line offset
//   * pass 1: snap the offset to ANY existing dim entity's offset on the
//             same chord-perpendicular axis (so a new dim aligns with one
//             already on the drawing — chained/continued dim stacks)
//   * pass 2: snap to zero (chord) when the cursor is near, so the dim
//             defaults to "inside the feature"
//
// Effect: the dim line "magnetically" sticks both to other dims AND to the
// chord itself. Dragging away releases the magnet, so wide offsets still
// work the way they always did.
RS_Vector projectToChordWithMagnetic(const RS_Vector& mouse,
                                     const RS_Vector& ext1,
                                     const RS_Vector& ext2,
                                     RS_EntityContainer* container,
                                     LC_GraphicViewport* viewport,
                                     double snapPx) {
    const RS_Vector chord = ext2 - ext1;
    const double chordLen = chord.magnitude();
    if (chordLen < RS_TOLERANCE) return mouse;
    const RS_Vector chordUnit{chord.x / chordLen, chord.y / chordLen};
    const RS_Vector perp{-chordUnit.y, chordUnit.x};
    const RS_Vector midpoint = (ext1 + ext2) * 0.5;
    const RS_Vector delta = mouse - midpoint;
    const double along = delta.x * chordUnit.x + delta.y * chordUnit.y;
    double off       = delta.x * perp.x      + delta.y * perp.y;

    double snapGraph = 0.0;
    if (viewport != nullptr) {
        // toUcsDX converts a GUI pixel delta to drawing-unit delta at the
        // current zoom. snapPx is in screen pixels; we need it in graph
        // units to compare against the perpendicular component.
        snapGraph = viewport->toUcsDX(static_cast<int>(snapPx));
    }

    // Pass 1: snap to an existing dim's offset on our perpendicular axis,
    // so a new dim line will magnet into alignment with one already on the
    // drawing (the chained / continued-dim case). Earlier attempt at this
    // looked broken because clicking on the aligned position caught the
    // existing dim's extension line and triggered the Smart-DIM angular
    // switch above; with that guard fixed (parent-is-dim check), this
    // alignment snap is safe to use again.
    if (container != nullptr && snapGraph > 0.0) {
        double bestExistingOff  = off;
        double bestExistingDist = snapGraph;
        for (RS_Entity* e : *container) {
            if (e == nullptr || !RS2::isDimensionalEntity(e->rtti())) continue;
            auto* dim = dynamic_cast<RS_Dimension*>(e);
            if (dim == nullptr) continue;
            const RS_Vector defPoint = dim->getData().definitionPoint;
            const RS_Vector dDelta   = defPoint - midpoint;
            const double existingOff = dDelta.x * perp.x + dDelta.y * perp.y;
            const double dist        = std::abs(off - existingOff);
            if (dist < bestExistingDist) {
                bestExistingDist = dist;
                bestExistingOff  = existingOff;
            }
        }
        if (bestExistingDist < snapGraph) off = bestExistingOff;
    }

    // Pass 2: snap to chord (off = 0) for the "inside the feature" default.
    if (snapGraph > 0.0 && std::abs(off) < snapGraph) off = 0.0;

    return midpoint + chordUnit * along + perp * off;
}
} // namespace

LC_ActionDimLinearBase::LC_ActionDimLinearBase(const char *name, LC_ActionContext *actionContext,  RS2::EntityType dimType, RS2::ActionType actionType):
   RS_ActionDimension(name, actionContext, dimType, actionType){
}

LC_ActionDimLinearBase::~LC_ActionDimLinearBase() = default;

#define DEBUG_DIM_SNAP_NO

namespace {
    //list of entity types supported by current action - line, arc, circle
    const auto dimEntityTypes = EntityTypeList{RS2::EntityDimLinear, RS2::EntityDimAligned};
}

void LC_ActionDimLinearBase::doTrigger() {
    preparePreview(m_alternateDimDirection);
    auto *dim = createDim(m_container);
    setPenAndLayerToActive(dim);
    dim->update();
    // Centered text is the default (see RS_Dimension::createHorizontalTextDimensionLine,
    // autoText path). If centering puts the label on top of another dim's text,
    // relocate it perpendicular-above / -below / along the dim line.
    if (auto* dimAsDimension = dynamic_cast<RS_Dimension*>(dim)) {
        autoPlaceTextAvoidingOverlap(dimAsDimension);
    }
    undoCycleAdd(dim);
    RS_DEBUG->print("LC_ActionDimLinearBase::trigger(): dim added: %lu", dim->getId());
}

void LC_ActionDimLinearBase::doInitWithContextEntity(RS_Entity* contextEntity, const RS_Vector& clickPos) {
    auto entity = contextEntity;
    if (isPolyline(contextEntity)) {
        auto polyline = static_cast<RS_Polyline*>(contextEntity);
        entity = polyline->getNearestEntity(clickPos);
    }
    if (isLine(entity)) {
        setExtensionPoint1(entity->getStartpoint());
        setExtensionPoint2(entity->getEndpoint());
        // Mark this as an entity-pick so smart-dim (ActionDimLinearAuto) forces
        // aligned orientation in getDimAngle(). Other action types ignore it.
        m_smartFirstLine = entity;
        m_smartFirstClick = clickPos;
        setStatus(SetDefPoint);
    }
}

void LC_ActionDimLinearBase::onMouseMoveEvent(int status, LC_MouseEvent *e) {
    RS_Vector mouse = e->snapPoint;

    bool ctrlPressed = e->isControl;
    switch (status) {
        case SetExtPoint1: {
            // Smart-DIM hover preview: highlight the line/circle/arc that the
            // click would dispatch to, so the user can see what's about to be
            // dimensioned. Same Ctrl-pick-a-line affordance for the non-Auto
            // variants is preserved.
            if (m_actionType == RS2::ActionDimLinearAuto) {
                EntityTypeList autoTypes{RS2::EntityLine, RS2::EntityCircle, RS2::EntityArc};
                auto entity = catchAndDescribe(e, autoTypes, RS2::ResolveAll);
                if (entity != nullptr) {
                    highlightHover(entity);
                } else {
                    trySnapToRelZeroCoordinateEvent(e);
                }
            }
            else if (ctrlPressed) {
                auto entity = catchAndDescribe(e, RS2::EntityLine, RS2::ResolveAll);
                if (isLine(entity)) {
                    highlightHover(entity);
                }
            }
            else {
                trySnapToRelZeroCoordinateEvent(e);
            }
            break;
        }
        case SetExtPoint2: {
            const RS_Vector &extPoint1 = getExtensionPoint1();
            if (extPoint1.valid){
                mouse = getSnapAngleAwarePoint(e, extPoint1, mouse, true);

                m_dimensionData->definitionPoint = mouse;
                setExtensionPoint2(mouse);

                if (m_showRefEntitiesOnPreview) {
                    previewRefLine(extPoint1, mouse);
                    previewRefSelectablePoint(mouse);
                }
                if (m_previewShowsFullDimension) {
                    preparePreview(ctrlPressed);
                    m_alternateDimDirection = ctrlPressed;
                    RS_Entity *dim = createDim(m_preview.get());
                    dim->update();
                    previewEntity(dim);
                }
                else if (m_showRefEntitiesOnPreview) {
                    previewRefLine(extPoint1, mouse);
                }
                else{
                    previewLine(extPoint1, mouse);
                }
            }
            break;
        }
        case SetDefPoint:{
            const RS_Vector &extPoint1 = getExtensionPoint1();
            const RS_Vector &extPoint2 = getExtensionPoint2();
            if (extPoint1.valid && extPoint2.valid){
                // Dim-line placement uses the raw mouse position — no entity
                // snap, no adjacent-dim snap. Snap was causing the dim line
                // to jump to nearby geometry instead of going where the user
                // moved the cursor. Picking the dim-line offset is a visual
                // placement decision, not a precision-snap step.
                //
                // AutoCAD-style magnetic-inside: project the cursor into the
                // chord's perpendicular axis and snap the perpendicular
                // offset to zero when the cursor is near the chord. So the
                // dim defaults to "inside the feature" (text between the
                // extension lines), and the user can drag outside if they
                // want offset.
                mouse = projectToChordWithMagnetic(
                    e->graphPoint, extPoint1, extPoint2,
                    m_container, m_viewport, /*snapPx=*/12.0);
                m_alternateDimDirection = ctrlPressed;
                m_dimensionData->definitionPoint = mouse;
                preparePreview(ctrlPressed);
                RS_Entity* dim = createDim(m_preview.get());
                dim->update();
                previewEntity(dim);
#ifdef DEBUG_DIM_SNAP
                RS_Vector p1;
                RS_Vector p2;
                dim->getDimPoints(p1, p2);

                addReferencePointToPreview(p1);
                addReferencePointToPreview(p2);
#endif
                break;
            }
        }
        default:
            break;
    }
}

void LC_ActionDimLinearBase::onMouseLeftButtonRelease(int status, LC_MouseEvent *e) {
    RS_Vector snap = e->snapPoint;
    switch (status) {
        case SetExtPoint1: {
            // Fresh dim cycle: clear the smart-pick line from any previous
            // trigger so a two-point pick this cycle doesn't get treated as
            // an entity pick (the angle path reads this flag to force aligned).
            m_smartFirstLine = nullptr;
            m_smartFirstClick = {};
            m_forceAligned = false;   // per-cycle; parallel-gap/delegated dims set it
            // AutoCAD-style smart DIM: when run as ActionDimLinearAuto, the
            // first click inspects what's under the cursor and dispatches:
            //   • line     → use its endpoints as the two ext-line origins
            //                and jump straight to placing the dim line
            //   • circle   → switch to the diameter-dim action
            //   • arc      → switch to the radial-dim action
            //   • empty    → fall through to the normal two-click flow
            if (m_actionType == RS2::ActionDimLinearAuto) {
                EntityTypeList autoTypes{RS2::EntityLine, RS2::EntityCircle, RS2::EntityArc};
                auto entity = catchAndDescribe(e, autoTypes, RS2::ResolveAll);
                if (entity != nullptr) {
                    if (isLine(entity)) {
                        // If the click snapped onto one of the line's endpoints the
                        // user is picking a POINT (e.g. a rectangle corner) for a
                        // two-point linear dim, not selecting the whole edge. Fall
                        // through to the normal two-click flow -- otherwise the
                        // second corner catches the adjoining edge and we'd wrongly
                        // hand off to the angular dim.
                        constexpr double vertexTol = 1.0e-8;
                        const bool snappedToVertex =
                            snap.distanceTo(entity->getStartpoint()) < vertexTol ||
                            snap.distanceTo(entity->getEndpoint()) < vertexTol;
                        if (!snappedToVertex) {
                            setExtensionPoint1(entity->getStartpoint());
                            setExtensionPoint2(entity->getEndpoint());
                            // Remember the line so SetDefPoint can detect a
                            // second-line click and dispatch to angular.
                            m_smartFirstLine  = entity;
                            m_smartFirstClick = snap;
                            setStatus(SetDefPoint);
                            break;
                        }
                    }
                    if (isCircle(entity)) {
                        switchToAction(RS2::ActionDimDiametric);
                        return;
                    }
                    if (isArc(entity)) {
                        switchToAction(RS2::ActionDimRadial);
                        return;
                    }
                }
            }
            if (e->isControl) {
                auto entity = catchAndDescribe(e, RS2::EntityLine, RS2::ResolveAll);
                if (isLine(entity)) {
                    setExtensionPoint1(entity->getStartpoint());
                    setExtensionPoint2(entity->getEndpoint());
                    setStatus(SetDefPoint);
                }
                else {
                    fireCoordinateEvent(snap);
                }
            }
            else {
                fireCoordinateEvent(snap);
            }
            break;
        }
        case SetExtPoint2: {
            m_alternateDimDirection = e->isControl;
            snap = getSnapAngleAwarePoint(e, getExtensionPoint1(), snap);
            fireCoordinateEvent(snap);
            break;
        }
        case SetDefPoint: {
            // Smart DIM: in Auto mode, if this click lands on a different
            // line than the first one, the user meant an angular dim between
            // them, not a linear placement. Save the first line as the new
            // action's context entity and switch — RS_ActionDimAngular's
            // doInitWithContextEntity will pre-populate line1, so the user
            // only needs to click line2 again and then place.
            //
            // BUT only when the caught line is a free user line — not when
            // it's a CHILD of an existing dimension (extension line, dim
            // line, leader, etc.). RS2::ResolveAll resolves into container
            // entities, so clicking on an existing dim catches one of its
            // lines and used to fire this branch — which silently switched
            // away from linear-dim placement when the user was just trying
            // to align with an existing dim. Skip the switch in that case
            // so the click commits the linear dim normally.
            if (m_actionType == RS2::ActionDimLinearAuto && m_smartFirstLine != nullptr && !m_forceAligned) {
                auto entity = catchAndDescribe(e, RS2::EntityLine, RS2::ResolveAll);
                if (entity != nullptr && isLine(entity) && entity != m_smartFirstLine) {
                    RS_Entity* parent = entity->getParent();
                    const bool insideDim = (parent != nullptr) &&
                                           RS2::isDimensionalEntity(parent->rtti());
                    if (!insideDim) {
                        // Parallel lines → dimension the perpendicular gap
                        // between them (aligned dim across the two lines).
                        // Non-parallel lines fall through to the angular dim.
                        const double a1 = m_smartFirstLine->getStartpoint()
                                              .angleTo(m_smartFirstLine->getEndpoint());
                        const double a2 = entity->getStartpoint()
                                              .angleTo(entity->getEndpoint());
                        const double d  = std::fmod(std::fabs(a1 - a2), M_PI);
                        constexpr double parallelTol = 0.0175;  // ~1 degree
                        const bool parallel = d < parallelTol || (M_PI - d) < parallelTol;
                        if (parallel) {
                            // Point on line 2 under the click, then its
                            // perpendicular foot on line 1 (onEntity=false →
                            // project onto the infinite lines).
                            const RS_Vector pOn2 = entity->getNearestPointOnEntity(
                                e->graphPoint, false);
                            const RS_Vector pOn1 = m_smartFirstLine->getNearestPointOnEntity(
                                pOn2, false);
                            if (pOn1.valid && pOn2.valid &&
                                LC_LineMath::isMeaningfulDistance(pOn1, pOn2)) {
                                setExtensionPoint1(pOn1);
                                setExtensionPoint2(pOn2);
                                m_forceAligned    = true;   // aligned across the gap
                                m_smartFirstLine  = nullptr;
                                m_smartFirstClick = {};
                                setStatus(SetDefPoint);
                                return;
                            }
                        }
                        m_actionContext->saveContextMenuActionContext(
                            m_smartFirstLine, m_smartFirstClick, /*clearEntitySelection=*/false);
                        // Stash line2 before switching — switchToAction inits
                        // the new action synchronously (consuming line1 from
                        // context) and we lose access to e once we return.
                        RS_Entity* secondLine = entity;
                        const RS_Vector secondClick = e->graphPoint;
                        switchToAction(RS2::ActionDimAngular);
                        // Feed the second click into the angular action so
                        // the user's NEXT click commits dim-arc placement
                        // (SetPos) — without this, the user would have to
                        // click line2 a third time to register it.
                        if (auto* ang = dynamic_cast<RS_ActionDimAngular*>(
                                m_actionContext->getCurrentAction())) {
                            ang->consumeSecondLineClick(secondLine, secondClick);
                        }
                        return;
                    }
                }
            }
            // Match the preview path: raw mouse coords, no snap, no adjacent-dim
            // alignment, but with the "magnetic inside" projection so a click
            // near the chord commits to offset = 0 just like the preview did.
            snap = projectToChordWithMagnetic(
                e->graphPoint, getExtensionPoint1(), getExtensionPoint2(),
                m_container, m_viewport, /*snapPx=*/12.0);
            m_alternateDimDirection = e->isControl;
            fireCoordinateEvent(snap);
            break;
        }
        default:
            fireCoordinateEvent(snap);
            break;
    }
}

void LC_ActionDimLinearBase::onMouseRightButtonRelease(int status, [[maybe_unused]] LC_MouseEvent *e) {
    deletePreview();
    initPrevious(status);
}

namespace {
    constexpr double kVertexTol = 1.0e-8;

    bool lineTouches(const RS_Entity* e, const RS_Vector& p) {
        return e->getStartpoint().distanceTo(p) < kVertexTol
            || e->getEndpoint().distanceTo(p) < kVertexTol;
    }

    // All visible lines (recursing into containers) with an endpoint at p.
    void collectLinesAt(const RS_EntityContainer* c, const RS_Vector& p,
                        std::vector<RS_Entity*>& out) {
        for (RS_Entity* e : *c) {
            if (e == nullptr || !e->isVisible()) {
                continue;
            }
            if (e->rtti() == RS2::EntityLine) {
                if (lineTouches(e, p)) {
                    out.push_back(e);
                }
            }
            else if (e->isContainer()) {
                collectLinesAt(static_cast<RS_EntityContainer*>(e), p, out);
            }
        }
    }

    bool linesShareVertex(const RS_Entity* a, const RS_Entity* b) {
        return lineTouches(b, a->getStartpoint()) || lineTouches(b, a->getEndpoint());
    }
}

bool LC_ActionDimLinearBase::trySwitchToAngularForPickedPoints(const RS_Vector &p1,
                                                               const RS_Vector &p2) {
    if (m_container == nullptr) {
        return false;
    }
    std::vector<RS_Entity*> at1;
    std::vector<RS_Entity*> at2;
    collectLinesAt(m_container, p1, at1);
    collectLinesAt(m_container, p2, at2);

    // Only when each point is an OPEN end (exactly one line). A rectangle corner
    // has two lines, so corner-to-corner picks stay a linear dim.
    if (at1.size() != 1 || at2.size() != 1) {
        return false;
    }
    RS_Entity* l1 = at1.front();
    RS_Entity* l2 = at2.front();
    if (l1 == l2 || !linesShareVertex(l1, l2)) {
        return false;   // same edge, or not meeting: keep it linear
    }

    m_actionContext->saveContextMenuActionContext(l1, p1, /*clearEntitySelection=*/false);
    switchToAction(RS2::ActionDimAngular);
    if (auto* ang = dynamic_cast<RS_ActionDimAngular*>(m_actionContext->getCurrentAction())) {
        ang->consumeSecondLineClick(l2, p2);
    }
    return true;
}

void LC_ActionDimLinearBase::onCoordinateEvent(int status, [[maybe_unused]] bool isZero, const RS_Vector &pos) {
    switch (status) {
        case SetExtPoint1: {
            setExtensionPoint1(pos);
            moveRelativeZero(pos);
            setStatus(SetExtPoint2);
            break;
        }
        case SetExtPoint2: {
            setExtensionPoint2(pos);
            // Smart DIM: two points that are the OPEN ends of two different
            // lines meeting at a shared vertex = "measure that angle".
            if (m_actionType == RS2::ActionDimLinearAuto
                && trySwitchToAngularForPickedPoints(getExtensionPoint1(), pos)) {
                return;
            }
            moveRelativeZero(pos);
            // We finally know the measured length — pick a DIMSCALE for the
            // very first dim that puts text + arrows comfortably inside the
            // feature.
            autoTuneDimScaleForFirstLinear();
            setStatus(SetDefPoint);
            break;
        }
        case SetDefPoint: {
            m_dimensionData->definitionPoint = pos;
            trigger();
            reset();
            setStatus(SetExtPoint1);
            break;
        }
        default:
            break;
    }
}

RS_Vector LC_ActionDimLinearBase::getAdjacentDimDimSnapPoint(const RS_Vector &ownDimPointToCheck, double snapRange){
    auto result = RS_Vector(false);
    auto dimCandidate = RS_Snapper::catchEntity(ownDimPointToCheck, dimEntityTypes, RS2::ResolveNone);
    if (dimCandidate != nullptr){
        int rtti = dimCandidate->rtti();
        bool dimFound = true;
        RS_Vector candidateDim1;
        RS_Vector candidateDim2;
        switch (rtti){
            case RS2::EntityDimAligned:{
                auto* startAligned = dynamic_cast<RS_DimAligned *>(dimCandidate);
                startAligned->getDimPoints(candidateDim1, candidateDim2);
                break;
            }
            case RS2::EntityDimLinear: {
                auto *startLin = dynamic_cast<RS_DimLinear *>(dimCandidate);
                startLin->getDimPoints(candidateDim1, candidateDim2);
                break;
            }
            default: {
                dimFound = false;
            }
        }
        if (dimFound){
            if (candidateDim1.distanceTo(ownDimPointToCheck) < snapRange){
                result = candidateDim1;
            } else if (candidateDim2.distanceTo(ownDimPointToCheck) < snapRange){
                result = candidateDim2;
            }
        }
    }
    return result;
}

RS_Vector LC_ActionDimLinearBase::adjustDefPointByAdjacentDims(const RS_Vector &mouse, const RS_Vector &extPoint1, const RS_Vector &extPoint2, double ownDimLineAngle, bool forPreview){
    RS_Vector result = mouse;
    RS_Vector dirDim = RS_Vector::polar(100.0, ownDimLineAngle);
    // construction line for dimension line
    RS_ConstructionLine dimLine(nullptr, RS_ConstructionLineData(mouse,mouse + dirDim));

    // tangent line from extensionPoint2
    RS_Vector dirTan = RS_Vector::polar(100, M_PI_2 + ownDimLineAngle);
    RS_ConstructionLine dimTangentLine(nullptr, RS_ConstructionLineData(extPoint2,
                                                                        extPoint2 + dirTan));

    // check whether dim points may be aligned with neighbors
    RS_Vector ownDimP2 = dimLine.getNearestPointOnEntity(extPoint2);
    double snapRange = getSnapRange();

    // first, check for end point (closest to mouse)
    RS_Vector endSnapPoint = getAdjacentDimDimSnapPoint(ownDimP2, snapRange);
    if (endSnapPoint.valid){
        result = dimTangentLine.getNearestPointOnEntity(endSnapPoint);
        if (forPreview && m_showRefEntitiesOnPreview){
            previewRefPoint(endSnapPoint);
        }
    }
    else {
        // checking for first point (start point of this dim)
        RS_Vector ownDimP1 = dimLine.getNearestPointOnEntity(extPoint1);
        RS_Vector startSnapPoint = getAdjacentDimDimSnapPoint(ownDimP1, snapRange);
        if (startSnapPoint.valid){
            result = dimTangentLine.getNearestPointOnEntity(startSnapPoint);
            if (forPreview && m_showRefEntitiesOnPreview){
                previewRefPoint(startSnapPoint);
            }
        }
    }

#ifdef DEBUG_DIM_SNAP
    if (forPreview){
            addReferenceLineToPreview(dimTangentLine.getStartpoint(), dimTangentLine.getEndpoint());
            addReferencePointToPreview(dimLine.getStartpoint());
            addReferencePointToPreview(dimLine.getEndpoint());
            addReferenceLineToPreview(dimLine.getStartpoint(), dimLine.getEndpoint());
            RS_Vector ownDimP1 = dimLine.getNearestPointOnEntity(edata->extensionPoint1); // intentional duplicate
            addReferencePointToPreview(ownDimP1);
            addReferencePointToPreview(ownDimP2);
        }
#endif
    return result;
}

RS_Vector LC_ActionDimLinearBase::adjustByAdjacentDim(RS_Vector mouse, bool forPreview, bool altDirection){
    return adjustDefPointByAdjacentDims(mouse, getExtensionPoint1(), getExtensionPoint2(), getDimAngle(altDirection), forPreview);
}

void LC_ActionDimLinearBase::updateMouseButtonHintForExtPoint2() {
    updateMouseWidgetTRBack(tr("Specify second extension line origin"), MOD_SHIFT_ANGLE_SNAP);
}

void LC_ActionDimLinearBase::updateMouseButtonHintForDefPoint() {
    updateMouseWidgetTRBack(tr("Specify dimension line location"), MOD_SHIFT_LC(tr("Snap to Adjacent Dim")));
}

void LC_ActionDimLinearBase::updateMouseButtonHints(){
    switch (getStatus()) {
        case SetExtPoint1:
            updateMouseWidgetTRCancel(tr("Specify first extension line origin"),MOD_SHIFT_AND_CTRL( MSG_REL_ZERO, "Select Line for origin points"));
            break;
        case SetExtPoint2:
            updateMouseButtonHintForExtPoint2();
            break;
        case SetDefPoint:
            updateMouseButtonHintForDefPoint();
            break;
        case SetText:
            updateMouseWidget(tr("Enter dimension text:"));
            break;
        case SetAngle:
            updateMouseWidget(tr("Enter dimension line angle:"));
            break;
        default:
            updateMouseWidget();
            break;
    }
}

void LC_ActionDimLinearBase::autoTuneDimScaleForFirstLinear() {
    if (m_container == nullptr || m_graphic == nullptr) return;

    // Only the very first dim gets auto-tuned. Once any RS_DimXxx exists,
    // the user (or a previous auto-tune) has already chosen a scale and we
    // leave it alone.
    for (RS_Entity* e : *m_container) {
        if (e != nullptr && RS2::isDimensionalEntity(e->rtti())) return;
    }

    const double measured = getExtensionPoint1().distanceTo(getExtensionPoint2());
    if (!(measured > 0.0) || !std::isfinite(measured)) return;

    const double dimtxt = m_graphic->getVariableDouble(QStringLiteral("$DIMTXT"), 2.5);
    if (!(dimtxt > 0.0)) return;

    // Target text height = 33% of the measured length. AutoCAD's "fits
    // between the arrows" sweet spot: text + 2 arrows ≈ measured, with
    // arrows ≈ text in default styles. Slightly aggressive (would push text
    // outside if features are tighter) but the auto-flip-arrows code at
    // rs_dimension.cpp:846 handles that case gracefully.
    constexpr double kTextFractionOfMeasured = 0.33;
    const double raw = (measured * kTextFractionOfMeasured) / dimtxt;
    if (!(raw > 0.0) || !std::isfinite(raw)) return;

    // Snap DOWN to the 1-2-5 series — biases smaller so the picked scale
    // never overshoots the measured length.
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double n   = raw / mag;
    const double nice = (n < 2.0) ? 1.0 : (n < 5.0) ? 2.0 : 5.0;
    const double scale = std::clamp(nice * mag, 0.0001, 100000.0);

    // Write to every dim style + the header variable — same pattern as
    // RS_ActionDimension::autoGuessDimScaleIfFirstDim, so a load/save
    // round-trip preserves the value.
    LC_DimStylesList* styles = m_graphic->getDimStyleList();
    if (styles != nullptr) {
        const QList<LC_DimStyle*>* list = styles->getStylesList();
        if (list != nullptr) {
            for (LC_DimStyle* s : *list) {
                if (s == nullptr || s->scaling() == nullptr) continue;
                s->scaling()->setScale(scale);
                s->scaling()->setFlag(LC_DimStyle::Scaling::$DIMSCALE);
            }
        }
        styles->setModified(true);
    }
    m_graphic->addVariable(QStringLiteral("$DIMSCALE"), scale, 40);
    m_graphic->setModified(true);

    // Tell the user — handy for tuning the heuristic, and makes the
    // automatic behaviour discoverable.
    if (RS_DIALOGFACTORY != nullptr) {
        RS_DIALOGFACTORY->commandMessage(
            tr("Auto-tuned DIMSCALE to %1 for a measured length of %2 "
               "(text ≈ 33%% of the feature). Use 'dimscale' to override.")
                .arg(scale,    0, 'g', 4)
                .arg(measured, 0, 'g', 4));
    }
}

namespace {
// Walk a dim's child entities and union the AABBs of every RS_MText child.
// Returns invalid QRectF if there's no text (text suppressed). Coordinates
// are world-space; entity getMin/getMax returns the post-rotation AABB so
// rotated labels are handled conservatively (slightly over-large bbox, which
// is harmless for collision rejection).
QRectF dimensionTextRect(RS_Dimension* dim) {
    if (dim == nullptr) return {};
    QRectF rect;
    for (RS_Entity* child : *dim) {
        if (child == nullptr || child->rtti() != RS2::EntityMText) continue;
        const RS_Vector mn = child->getMin();
        const RS_Vector mx = child->getMax();
        if (!mn.valid || !mx.valid) continue;
        QRectF r(QPointF(mn.x, mn.y), QPointF(mx.x, mx.y));
        r = r.normalized();
        rect = rect.isValid() ? rect.united(r) : r;
    }
    return rect;
}

bool intersectsAny(const QRectF& candidate, const QList<QRectF>& obstacles) {
    for (const QRectF& o : obstacles) {
        if (candidate.intersects(o)) return true;
    }
    return false;
}

// Extract the dim line endpoints in WCS for the variants we autoplace.
// Returns false if the dim type isn't supported (autoplace is then a no-op).
bool getDimLineEndpoints(RS_Dimension* dim, RS_Vector& p1, RS_Vector& p2) {
    if (auto* lin = dynamic_cast<RS_DimLinear*>(dim)) {
        lin->getDimPoints(p1, p2);
        return p1.valid && p2.valid;
    }
    if (auto* al = dynamic_cast<RS_DimAligned*>(dim)) {
        al->getDimPoints(p1, p2);
        return p1.valid && p2.valid;
    }
    return false;
}
} // namespace

void LC_ActionDimLinearBase::autoPlaceTextAvoidingOverlap(RS_Dimension* dim) {
    if (dim == nullptr || m_container == nullptr) return;

    // 1. Snapshot obstacle rects: every OTHER dim's text bbox in the drawing.
    //    Pre-existing dims only — we never move them.
    QList<QRectF> obstacles;
    for (RS_Entity* e : *m_container) {
        if (e == nullptr || e == dim) continue;
        if (!RS2::isDimensionalEntity(e->rtti())) continue;
        auto* other = dynamic_cast<RS_Dimension*>(e);
        if (other == nullptr) continue;
        const QRectF r = dimensionTextRect(other);
        if (r.isValid()) obstacles.append(r);
    }
    if (obstacles.isEmpty()) return;

    // 2. Centered position clear? Done — nothing to do.
    const QRectF originalRect = dimensionTextRect(dim);
    if (!originalRect.isValid()) return;
    const double textH = originalRect.height();
    const double textW = originalRect.width();
    // Small padding so abutting boxes don't count as overlap, and so the
    // relocated label keeps visible breathing room from its neighbor.
    const double pad = std::max(textH, textW) * 0.10;
    if (!intersectsAny(originalRect.adjusted(-pad, -pad, pad, pad), obstacles)) return;

    // 3. Need dim-line direction to know what "above" and "along" mean.
    RS_Vector p1;
    RS_Vector p2;
    if (!getDimLineEndpoints(dim, p1, p2)) return;
    const double chordLen = p1.distanceTo(p2);
    if (chordLen < RS_TOLERANCE) return;
    const RS_Vector unitChord{(p2.x - p1.x) / chordLen, (p2.y - p1.y) / chordLen};
    const RS_Vector outward = dim->getTextOutwardDirection(p1, p2);
    if (!outward.valid) return;

    // 4. Build candidate offsets (perpendicular and along-line deltas applied
    //    to the original centered text-rect position). Ladder per spec:
    //    above 1-3 rows -> below 1-3 rows -> along +/- 1-4 steps. The
    //    translated rect is the test - RS_MText rotation is invariant under
    //    translation so the bbox shifts cleanly, no re-update needed for the
    //    probe.
    const double stepPerp  = textH + pad * 2.0;
    const double stepAlong = textW + pad * 2.0;

    QList<RS_Vector> deltas;
    for (int n = 1; n <= 3; ++n) deltas.append(outward * ( stepPerp  * n));   // above
    for (int n = 1; n <= 3; ++n) deltas.append(outward * (-stepPerp  * n));   // below
    for (int n = 1; n <= 4; ++n) {                                            // along, +/-
        deltas.append(unitChord * ( stepAlong * n));
        deltas.append(unitChord * (-stepAlong * n));
    }

    RS_Vector chosenDelta;
    bool found = false;
    for (const RS_Vector& d : deltas) {
        const QRectF trial = originalRect.translated(d.x, d.y)
                                          .adjusted(-pad, -pad, pad, pad);
        if (intersectsAny(trial, obstacles)) continue;
        chosenDelta = d;
        found = true;
        break;
    }
    if (!found) return;  // Drawing is crowded; leave the user to grip-move.

    // 5. Commit: write the new text anchor and disable autoText so the chosen
    //    location survives subsequent dim updates (style changes, edits, etc.).
    const RS_Vector newMiddle = dim->getMiddleOfText() + chosenDelta;
    dim->setMiddleOfText(newMiddle);
    dim->setAutoText(false);
    dim->update();
}
