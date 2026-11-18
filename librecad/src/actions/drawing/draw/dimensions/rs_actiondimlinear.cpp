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

#include "rs_actiondimlinear.h"

#include <cmath>

#include "lc_linemath.h"
#include "rs_constructionline.h"
#include "rs_dimlinear.h"
#include "rs_dimension.h"

/**
 * Constructor.
 *
 * @param angle Initial angle in rad.
 * @param fixedAngle true: The user can't change the angle.
 *                   false: The user can change the angle in a option widget.
 */
RS_ActionDimLinear::RS_ActionDimLinear(LC_ActionContext *actionContext,double angle,bool _fixedAngle, RS2::ActionType type)
    :LC_ActionDimLinearBase("Draw linear dimensions", actionContext, RS2::EntityDimLinear, type),
     m_edata(std::make_unique<RS_DimLinearData>(RS_Vector(0., 0.), RS_Vector(0., 0.), angle, 0.)),
     m_fixedAngle(_fixedAngle), m_lastStatus(SetExtPoint1){
    setUcsAngleDegrees(angle);
    updateOptions();
    reset();
}

RS_ActionDimLinear::~RS_ActionDimLinear() = default;

void RS_ActionDimLinear::init(int status) {
    LC_ActionDimLinearBase::init(status);
    // Delegated two-point handoff (see presetFirstExtensionPoint): consume the
    // seeded first extension point by replaying it through the normal
    // coordinate path, which sets ext point 1 and advances to "specify second
    // extension point" — no duplication of the ext-point-1 side effects.
    if (status >= InitialActionStatus && m_presetExtPoint1.valid) {
        RS_Vector p = m_presetExtPoint1;
        m_presetExtPoint1 = RS_Vector(false);
        m_smartFirstLine = nullptr;
        m_smartFirstClick = {};
        fireCoordinateEvent(p);
        updateMouseButtonHints();
    }
}

void RS_ActionDimLinear::presetFirstExtensionPoint(const RS_Vector& p) {
    m_presetExtPoint1 = p;
    // A delegated two-point dim (from the smart 'dd') is an ALIGNED dim: the dim
    // line runs parallel to the two picked points (true distance), never snapped
    // to H/V and never switched to an angular dim.
    m_forceAligned = true;
}

void RS_ActionDimLinear::reset(){
    RS_ActionDimension::reset();

    double angleDeg = m_fixedAngle ? m_ucsBasisAngleDegrees : 0; // keep selected angle

    *m_edata = {{}, {}, toWorldAngleFromUCSBasisDegrees(angleDeg), toWorldAngle(0.0)};
    m_forceAligned = false;   // per-cycle flag; don't leak into the next dim
}

void RS_ActionDimLinear::preparePreview(bool alternateMode){
    double angle = getDimAngle(alternateMode);
    RS_Vector dirV = RS_Vector::polar(100., angle + M_PI_2);
    RS_ConstructionLine cl(nullptr,RS_ConstructionLineData(m_edata->extensionPoint2,m_edata->extensionPoint2 + dirV));
    m_dimensionData->definitionPoint = cl.getNearestPointOnEntity(m_dimensionData->definitionPoint);
}

RS_Entity *RS_ActionDimLinear::createDim(RS_EntityContainer* parent){
    m_edata->angle = getDimAngle(m_alternateDimDirection);
    m_edata->oblique =  toWorldAngle(0.0);
    auto *dim = new RS_DimLinear(parent, *m_dimensionData, *m_edata);
    return dim;
}

double RS_ActionDimLinear::getUcsAngleDegrees() const{
    return m_ucsBasisAngleDegrees;
}

void RS_ActionDimLinear::setUcsAngleDegrees(double ucsRelAngleDegrees){
    m_ucsBasisAngleDegrees = ucsRelAngleDegrees;
}

bool RS_ActionDimLinear::hasFixedAngle() const{
    return m_fixedAngle;
}

void RS_ActionDimLinear::onCoordinateEvent(int status, bool isZero, const RS_Vector &pos) {
    switch (status){
        case SetAngle:{
            if (isZero){
                setUcsAngleDegrees(0);
                updateOptions();
                setStatus(m_lastStatus);
                return;
            }
            break;
        }
        default:
            break;
    }
    LC_ActionDimLinearBase::onCoordinateEvent(status, isZero, pos);
}

bool RS_ActionDimLinear::doProcessCommand(int status, const QString &c) {
    bool accept = false;
    switch (status) {
        case SetText: {
            setText(c);
            updateOptions();
            enableCoordinateInput();
            setStatus(m_lastStatus);
            accept = true;
            break;
        }
        case SetAngle: {
            double ucsBasisAngleDeg;
            bool ok = parseToUCSBasisAngle(c, ucsBasisAngleDeg);
            if (ok){
                accept = true;
                m_ucsBasisAngleDegrees = ucsBasisAngleDeg;
            } else {
                commandMessage(tr("Not a valid expression"));
            }
            updateOptions();
            setStatus(m_lastStatus);
            break;
        }
        default:
            m_lastStatus = (Status) getStatus();
            deletePreview();
            if (checkCommand("text", c)){
                disableCoordinateInput();
                setStatus(SetText);
                accept = true;
            } else if (!m_fixedAngle && (checkCommand("angle", c))){
                setStatus(SetAngle);
                accept = true;
            }
            break;
    }
    return accept;
}

QStringList RS_ActionDimLinear::getAvailableCommands(){
    QStringList cmd;

    switch (getStatus()) {
        case SetExtPoint1:
        case SetExtPoint2:
        case SetDefPoint: {
            cmd += command("text");
            if (!m_fixedAngle){
                cmd += command("angle");
            }
            break;
        }
        default:
            break;
    }
    return cmd;
}

RS_Vector RS_ActionDimLinear::getExtensionPoint1(){
    return m_edata->extensionPoint1;
}

RS_Vector RS_ActionDimLinear::getExtensionPoint2(){
    return m_edata->extensionPoint2;
}

double RS_ActionDimLinear::getDimAngle(bool alternateMode){
// Todo - sand - option for dim behaviour?
// This is good place for optional behavior for Hor/Vert dims.
// Implementation below created dims orthogonal to X and Y axises. 0 in UCS Abs values, ignore basis there??
// however, if translate angles using toWorldAngleFromUCSBasis(), dims will be orthogonal to angles basis.
// not sure whether this is necessary so far, yet still - an option may be added to the options widget.

    switch (m_actionType){
        case RS2::ActionDimLinearHor:{
            double angle = alternateMode ? M_PI_2: 0;
            return toWorldAngle(angle);
        }
        case RS2::ActionDimLinearVer:{
            double angle = alternateMode ? 0: M_PI_2;
            return toWorldAngle(angle);
        }
        case RS2::ActionDimLinearAuto:{
            // AutoCAD-style auto-pick: after 2 extension points are set, the
            // cursor location decides H / V / aligned. Called from
            // preparePreview() and createDim(); m_dimensionData->definitionPoint
            // holds the raw mouse coords at that moment (before projection).
            const RS_Vector& p1 = m_edata->extensionPoint1;
            const RS_Vector& p2 = m_edata->extensionPoint2;
            const RS_Vector  m  = m_dimensionData->definitionPoint;
            if (!p1.valid || !p2.valid || !m.valid) {
                return toWorldAngle(0.0);
            }
            const RS_Vector seg = p2 - p1;
            if (seg.magnitude() < RS_TOLERANCE) {
                return toWorldAngle(0.0);
            }

            // Aligned path:
            //  - entity pick: the user clicked a line (m_smartFirstLine set), so
            //    dimension THAT line — always aligned.
            //  - delegated two-point 'dd' (m_forceAligned): the two picked
            //    points get an ALIGNED dim, parallel to them, true distance —
            //    never H/V-snapped, never angular.
            // Cursor only chooses the dim-line offset, not orientation. Plain
            // d/dim two-point picks leave both null and fall through to the
            // cursor-direction H/V/aligned logic below.
            if (m_smartFirstLine != nullptr || m_forceAligned) {
                return std::atan2(seg.y, seg.x);
            }

            // Axis-aligned segments — bypass the cursor-direction logic.
            // A horizontal dim of a vertical segment (or vice-versa) measures
            // zero, which produces a degenerate dim with a giant misplaced
            // arrow when DIMSCALE is large. So lock the dim type to whatever
            // matches the segment when the segment is clearly axis-aligned
            // (within ~3° of horizontal / vertical).
            const double absX = std::fabs(seg.x);
            const double absY = std::fabs(seg.y);
            constexpr double axisAlignedRatio = 0.05;  // ~tan(3°)
            if (absY < absX * axisAlignedRatio) {
                return toWorldAngle(0.0);              // horizontal segment → horizontal dim
            }
            if (absX < absY * axisAlignedRatio) {
                return toWorldAngle(M_PI_2);           // vertical   segment → vertical   dim
            }

            // Diagonal segment: wedge around the perpendicular bisector → aligned,
            // otherwise H or V based on which screen axis the cursor offset dominates.
            const double alpha = std::atan2(seg.y, seg.x);
            const RS_Vector mid = (p1 + p2) * 0.5;
            const RS_Vector v = m - mid;
            const double cosA = std::cos(alpha);
            const double sinA = std::sin(alpha);
            const double vAlong = v.x * cosA + v.y * sinA;
            const double vPerp  = -v.x * sinA + v.y * cosA;
            if (std::fabs(vPerp) > std::fabs(vAlong) * 2.5) {
                return alpha;
            }
            if (std::fabs(v.x) > std::fabs(v.y)) {
                return toWorldAngle(M_PI_2);
            }
            return toWorldAngle(0.0);
        }
        default:{
            double angleDegrees = m_ucsBasisAngleDegrees;
            if (alternateMode) {
                if (!LC_LineMath::isMeaningfulAngle(m_ucsBasisAngleDegrees)) {
                    angleDegrees = 90.0;
                }
                else if (!LC_LineMath::isMeaningfulAngle(m_ucsBasisAngleDegrees - 90.0)) {
                    angleDegrees = 0.0;
                }
            }
            return toWorldAngleFromUCSBasisDegrees(angleDegrees);
        }
    }
}

bool RS_ActionDimLinear::checkMaySwitchDimDirection() {
    bool maySwitchDirection = m_actionType == RS2::ActionDimLinearHor || m_actionType == RS2::ActionDimLinearVer;
    if (!maySwitchDirection) {
        maySwitchDirection  = !LC_LineMath::isMeaningfulAngle(m_ucsBasisAngleDegrees) || !LC_LineMath::isMeaningfulAngle(m_ucsBasisAngleDegrees - M_PI_2);
    }
    return maySwitchDirection;
}

void RS_ActionDimLinear::updateMouseButtonHintForExtPoint2() {
    bool maySwitchDirection = checkMaySwitchDimDirection();
    if (maySwitchDirection) {
        updateMouseWidgetTRBack(tr("Specify second extension line origin"), MOD_SHIFT_AND_CTRL( MSG_REL_ZERO, tr("Switch Direction")));
    }
    else {
        LC_ActionDimLinearBase::updateMouseButtonHintForExtPoint2();
    }
}

void RS_ActionDimLinear::updateMouseButtonHintForDefPoint() {
    bool maySwitchDirection = checkMaySwitchDimDirection();
    if (maySwitchDirection) {
        updateMouseWidgetTRBack(tr("Specify dimension line location"), MOD_SHIFT_AND_CTRL(tr("Snap to Adjacent Dim"), tr("Switch Direction")));
    }
    else {
        LC_ActionDimLinearBase::updateMouseButtonHintForDefPoint();
    }
}

void RS_ActionDimLinear::setExtensionPoint1(RS_Vector p){
    m_edata->extensionPoint1 = p;
}

void RS_ActionDimLinear::setExtensionPoint2(RS_Vector p){
    m_edata->extensionPoint2 = p;
}
