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

#ifndef LC_ACTIONMODIFYDIMALIGN_H
#define LC_ACTIONMODIFYDIMALIGN_H

#include "lc_actionpreselectionawarebase.h"

class LC_ActionModifyDimAlign : public LC_ActionPreSelectionAwareBase {
    Q_OBJECT
public:
    explicit LC_ActionModifyDimAlign(LC_ActionContext *actionContext);

protected:
    enum Status { PickMaster = 0 };

    enum class Bucket { Horizontal, Vertical, Other };

    void init(int status) override;
    void updateMouseButtonHintsForSelection() override;
    void updateMouseButtonHintsForSelected(int status) override;
    bool isAllowTriggerOnEmptySelection() override { return false; }

    void onSelectionCompleted(bool singleEntity, bool fromInit) override;
    void onMouseLeftButtonReleaseSelected(int status, LC_MouseEvent *e) override;
    void onMouseRightButtonReleaseSelected(int status, LC_MouseEvent *e) override;
    void onMouseMoveEventSelected(int status, LC_MouseEvent *e) override;
    void doTrigger(bool keepSelected) override;
    RS2::CursorType doGetMouseCursorSelected(int status) override;

private:
    RS_Entity *m_master {nullptr};

    static double dimLineAngle(RS_Entity *e);
    static Bucket bucketOf(double angle);
};

#endif // LC_ACTIONMODIFYDIMALIGN_H
