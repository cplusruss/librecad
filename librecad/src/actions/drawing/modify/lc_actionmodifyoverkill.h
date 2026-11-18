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

#ifndef LC_ACTIONMODIFYOVERKILL_H
#define LC_ACTIONMODIFYOVERKILL_H

#include "lc_actionpreselectionawarebase.h"

/**
 * OVERKILL: removes duplicate lines from the current selection — lines that lie
 * on top of each other (coincident endpoints, either direction). One
 * representative per duplicate group is kept; the rest are deleted (undoable).
 */
class LC_ActionModifyOverkill : public LC_ActionPreSelectionAwareBase {
    Q_OBJECT
public:
    LC_ActionModifyOverkill(LC_ActionContext *actionContext);
protected:
    RS2::CursorType doGetMouseCursorSelected(int status) override;
    void updateMouseButtonHintsForSelection() override;
    void doTrigger(bool keepSelected) override;
    bool doProcessCommand(int status, const QString &command) override;
private:
    double m_fuzz;   // coincidence tolerance, persisted between uses
};

#endif // LC_ACTIONMODIFYOVERKILL_H
