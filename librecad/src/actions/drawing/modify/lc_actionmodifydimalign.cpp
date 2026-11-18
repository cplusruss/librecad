/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2026 LibreCAD.org

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
#include "lc_actionmodifydimalign.h"

#include <cmath>

#include "rs_dimaligned.h"
#include "rs_dimension.h"
#include "rs_dimlinear.h"
#include "rs_entitycontainer.h"

LC_ActionModifyDimAlign::LC_ActionModifyDimAlign(LC_ActionContext *actionContext)
    : LC_ActionPreSelectionAwareBase("ModifyDimAlign", actionContext, RS2::ActionDimAlign) {
}

double LC_ActionModifyDimAlign::dimLineAngle(RS_Entity *e) {
    if (e == nullptr) return std::nan("");
    switch (e->rtti()) {
        case RS2::EntityDimLinear:
            return static_cast<RS_DimLinear*>(e)->getAngle();
        case RS2::EntityDimAligned: {
            auto *d = static_cast<RS_DimAligned*>(e);
            return d->getExtensionPoint1().angleTo(d->getExtensionPoint2());
        }
        default:
            return std::nan("");
    }
}

LC_ActionModifyDimAlign::Bucket LC_ActionModifyDimAlign::bucketOf(double angle) {
    if (std::isnan(angle)) return Bucket::Other;
    constexpr double tol = 0.01;
    double a = std::fmod(std::fmod(angle, M_PI) + M_PI, M_PI);
    if (a < tol || (M_PI - a) < tol) return Bucket::Horizontal;
    if (std::abs(a - M_PI_2) < tol) return Bucket::Vertical;
    return Bucket::Other;
}

void LC_ActionModifyDimAlign::init(int status) {
    m_master = nullptr;
    LC_ActionPreSelectionAwareBase::init(status);
}

void LC_ActionModifyDimAlign::onSelectionCompleted(
    [[maybe_unused]] bool singleEntity, bool fromInit) {
    setSelectionComplete(isAllowTriggerOnEmptySelection(), fromInit);
    updateMouseButtonHints();
    updateSelectionWidget();
    if (m_selectionComplete) {
        setStatus(PickMaster);
    }
}

void LC_ActionModifyDimAlign::onMouseMoveEventSelected(int status, LC_MouseEvent *e) {
    if (status != PickMaster || e == nullptr) return;
    RS_Entity *under = catchEntity(e->snapPoint);
    if (under != nullptr && under->isSelected()
        && bucketOf(dimLineAngle(under)) != Bucket::Other) {
        highlightHover(under);
    }
}

void LC_ActionModifyDimAlign::onMouseLeftButtonReleaseSelected(int status, LC_MouseEvent *e) {
    if (status != PickMaster || e == nullptr) return;
    RS_Entity *picked = catchEntity(e->snapPoint);
    if (picked == nullptr) {
        commandMessage(tr("No entity under cursor. Click one of the selected dimensions."));
        return;
    }
    if (!picked->isSelected()) {
        commandMessage(tr("Master must be one of the dimensions you selected before 'da'."));
        return;
    }
    if (bucketOf(dimLineAngle(picked)) == Bucket::Other) {
        commandMessage(tr("Picked dim isn't horizontal or vertical (linear/aligned only)."));
        return;
    }
    m_master = picked;
    trigger();
}

void LC_ActionModifyDimAlign::onMouseRightButtonReleaseSelected(int status, [[maybe_unused]] LC_MouseEvent *e) {
    deletePreview();
    initPrevious(status);
}

void LC_ActionModifyDimAlign::doTrigger(bool keepSelected) {
    if (m_master == nullptr) return;

    const Bucket masterBucket = bucketOf(dimLineAngle(m_master));
    if (masterBucket == Bucket::Other) return;

    auto *masterDim = dynamic_cast<RS_Dimension*>(m_master);
    if (masterDim == nullptr) return;

    const RS_Vector masterDp = masterDim->getDefinitionPoint();
    const bool alignX = (masterBucket == Bucket::Vertical);   // vertical dim line ⇒ share X
    const double target = alignX ? masterDp.x : masterDp.y;
    constexpr double coordTol = 1e-9;

    undoCycleStart();

    int aligned = 0;
    int skipped = 0;

    for (auto *e : m_selectedEntities) {
        if (e == nullptr || e == m_master) continue;
        if (bucketOf(dimLineAngle(e)) != masterBucket) {
            ++skipped;
            continue;
        }
        auto *dim = dynamic_cast<RS_Dimension*>(e);
        if (dim == nullptr) { ++skipped; continue; }

        const RS_Vector dp = dim->getDefinitionPoint();
        const double cur = alignX ? dp.x : dp.y;
        if (std::abs(cur - target) < coordTol) continue;     // already aligned

        auto *clone = dynamic_cast<RS_Dimension*>(e->clone());
        if (clone == nullptr) { ++skipped; continue; }

        RS_Vector newDp = clone->getDefinitionPoint();
        if (alignX) {
            newDp.x = target;
        } else {
            newDp.y = target;
        }
        clone->setDefinitionPoint(newDp);
        clone->setSelected(keepSelected);
        clone->updateDim(true);

        m_container->addEntity(clone);
        undoableAdd(clone);
        undoableDeleteEntity(e);
        ++aligned;
    }

    undoCycleEnd();

    QString msg = tr("Dimension Align: %1 dim(s) aligned to master's %2")
                      .arg(aligned)
                      .arg(alignX ? tr("X") : tr("Y"));
    if (skipped > 0) msg.append(tr(", %1 skipped").arg(skipped));
    commandMessage(msg);

    m_master = nullptr;
    m_selectedEntities.clear();
    m_selectionComplete = false;
    finish(false);
}

void LC_ActionModifyDimAlign::updateMouseButtonHintsForSelection() {
    updateMouseWidgetTRCancel(
        tr("Select linear / aligned dims (Enter to confirm, then pick master)"));
}

void LC_ActionModifyDimAlign::updateMouseButtonHintsForSelected(int status) {
    switch (status) {
        case PickMaster:
            updateMouseWidgetTRBack(
                tr("Pick reference dimension - others snap to its offset"));
            break;
        default:
            updateMouseWidget();
    }
}

RS2::CursorType LC_ActionModifyDimAlign::doGetMouseCursorSelected([[maybe_unused]] int status) {
    return RS2::CadCursor;
}
