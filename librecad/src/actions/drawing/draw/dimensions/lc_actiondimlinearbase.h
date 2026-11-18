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

#ifndef LIBRECAD_LC_ACTIONDIMLINEARBASE_H
#define LIBRECAD_LC_ACTIONDIMLINEARBASE_H

#include "rs_actiondimension.h"

class RS_Dimension;

class LC_ActionDimLinearBase : public RS_ActionDimension {
    Q_OBJECT
public:
    LC_ActionDimLinearBase(const char* name, LC_ActionContext* actionContext, RS2::EntityType entityType,
                           RS2::ActionType actionType = RS2::ActionNone);
    ~LC_ActionDimLinearBase() override;
protected:
    // Smart DIM: two picked points that are the OPEN ends of two different lines
    // meeting at a shared vertex mean "measure that angle". Returns true if it
    // handed off to the angular dim.
    bool trySwitchToAngularForPickedPoints(const RS_Vector &p1, const RS_Vector &p2);
    /**
   * Action States.
   */
    enum Status {
        SetExtPoint1 = InitialActionStatus, /**< Setting the 1st ext point.  */
        SetExtPoint2,                       /**< Setting the 2nd ext point. */
        SetDefPoint,                        /**< Setting the common def point. */
        SetText,                            /**< Setting the text label in the command line. */
        SetAngle                            /**< Setting the angle in the command line. */
    };

    enum ActionMode {
        NORMAL,
        BASELINE,
        CONTINUE
    };

    ActionMode m_actionMode = NORMAL;

    bool m_alternateDimDirection = false;

    // Smart-DIM (ActionDimLinearAuto) only: remembers the line auto-picked
    // during SetExtPoint1 so the SetDefPoint click handler can check whether
    // a subsequent click on a *different* line should switch to an angular
    // dim (using the first line as the angular's pre-set context entity).
    RS_Entity* m_smartFirstLine{nullptr};
    RS_Vector  m_smartFirstClick;

    // Delegated two-point 'dd' handoff: force an ALIGNED dim (parallel to the
    // two picked points) and never switch to an angular dim. Set via
    // RS_ActionDimLinear::presetFirstExtensionPoint().
    bool m_forceAligned{false};

    void doInitWithContextEntity(RS_Entity* contextEntity, const RS_Vector& clickPos) override;
    virtual RS_Vector getExtensionPoint1() = 0;
    virtual void setExtensionPoint1(RS_Vector p) = 0;
    virtual void setExtensionPoint2(RS_Vector p) = 0;
    virtual RS_Vector getExtensionPoint2() = 0;
    virtual void preparePreview(bool alternateMode) = 0;
    virtual double getDimAngle(bool alternateMode) = 0;
    RS_Vector getAdjacentDimDimSnapPoint(const RS_Vector& ownDimPointToCheck, double snapRange);
    RS_Vector adjustDefPointByAdjacentDims(const RS_Vector& mouse, const RS_Vector& extPoint1,
                                           const RS_Vector& extPoint2, double ownDimLineAngle, bool forPreview);
    RS_Vector adjustByAdjacentDim(RS_Vector mouse, bool forPreview, bool altDirection);
    virtual void updateMouseButtonHintForExtPoint2();
    virtual void updateMouseButtonHintForDefPoint();
    virtual RS_Entity* createDim(RS_EntityContainer* parent) = 0;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent* e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent* e) override;
    void onMouseMoveEvent(int status, LC_MouseEvent* event) override;
    void onCoordinateEvent(int status, bool isZero, const RS_Vector& pos) override;
    void updateMouseButtonHints() override;
    void doTrigger() override;

    // Called after the user picks ext2 — i.e. the moment we finally know how
    // far apart the dimensioned points are. For the very first dim on the
    // drawing (no existing RS_DimXxx entities), picks DIMSCALE such that
    // text height ≈ 33% of the measured chord, snapped down to a 1-2-5
    // nice value. That's the "fits between the arrows" target the user
    // expects. Skipped once any dim exists, so a tuned value isn't blown
    // away by re-running the action.
    void autoTuneDimScaleForFirstLinear();

    // Called from doTrigger() after dim->update(). If the freshly-placed dim's
    // text label AABB overlaps any existing dim's text AABB in the container,
    // shift the text label to the first non-overlapping candidate position
    // in this order: perpendicular outward (1×, 2×, 3× row height) → opposite
    // perpendicular (1×, 2×, 3×) → along the dim line in alternating
    // directions. If nothing fits, leave the text centered (the user can
    // still grip-move). Marks autoText=false when relocated so the chosen
    // position survives subsequent updates.
    void autoPlaceTextAvoidingOverlap(RS_Dimension* dim);
};
#endif //LIBRECAD_LC_ACTIONDIMLINEARBASE_H
