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

#include "lc_actionmodifyoverkill.h"

#include "rs_entitycontainer.h"
#include "rs_line.h"
#include "rs_math.h"
#include "rs_modification.h"
#include "rs_settings.h"

namespace {
    // Two lines are duplicates if their endpoints coincide (within fuzz) in
    // either direction.
    bool sameLine(RS_Entity* a, RS_Entity* b, double fuzz) {
        const RS_Vector a1 = a->getStartpoint();
        const RS_Vector a2 = a->getEndpoint();
        const RS_Vector b1 = b->getStartpoint();
        const RS_Vector b2 = b->getEndpoint();
        return (a1.distanceTo(b1) < fuzz && a2.distanceTo(b2) < fuzz)
            || (a1.distanceTo(b2) < fuzz && a2.distanceTo(b1) < fuzz);
    }
}

LC_ActionModifyOverkill::LC_ActionModifyOverkill(LC_ActionContext *actionContext)
    : LC_ActionPreSelectionAwareBase("Overkill", actionContext, RS2::ActionModifyOverkill) {
    // Remember the last fuzz distance. Default 1e-6: catches lines visually on
    // top of each other without merging genuinely distinct ones.
    bool ok;
    double f = RS_Math::eval(LC_GET_ONE_STR("Modify", "OverkillFuzz", "0.000001"), &ok);
    m_fuzz = (ok && f > 0.0) ? f : 1.0e-6;
}

void LC_ActionModifyOverkill::doTrigger([[maybe_unused]] bool keepSelected) {
    // A selection is optional. With nothing selected, work on everything
    // currently turned on - visible and unlocked, the same "is this editable
    // right now" rule the other modify actions use.
    std::vector<RS_Entity*> candidates;
    if (m_selectedEntities.empty()) {
        for (RS_Entity* e : *m_container) {
            if (e != nullptr && e->isVisible() && !e->isLocked()) {
                candidates.push_back(e);
            }
        }
    } else {
        candidates = m_selectedEntities;
    }

    std::vector<RS_Entity*> kept;      // one representative line per group
    std::vector<RS_Entity*> toRemove;  // redundant duplicates to delete
    for (RS_Entity* e : candidates) {
        if (e == nullptr || e->rtti() != RS2::EntityLine) {
            continue;   // OVERKILL here targets duplicate lines only
        }
        bool duplicate = false;
        for (RS_Entity* k : kept) {
            if (sameLine(e, k, m_fuzz)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            toRemove.push_back(e);
        } else {
            kept.push_back(e);
        }
    }

    if (toRemove.empty()) {
        commandMessage(tr("Overkill: no duplicate lines found."));
    } else {
        RS_Modification m(*m_container, m_viewport);
        m.remove(toRemove);
        commandMessage(tr("Overkill: removed %1 duplicate line(s).").arg(toRemove.size()));
    }
    redrawDrawing();
}

void LC_ActionModifyOverkill::updateMouseButtonHintsForSelection() {
    updateMouseWidgetTRCancel(
        tr("Select lines, or Enter for whole drawing (fuzz <%1>)")
            .arg(m_fuzz),
        MOD_SHIFT_LC(tr("Select contour")));
}

bool LC_ActionModifyOverkill::doProcessCommand(int status, const QString &command) {
    bool ok;
    double f = RS_Math::eval(command, &ok);
    if (ok && f > 0.0) {
        m_fuzz = f;
        LC_SET_ONE("Modify", "OverkillFuzz", QString::number(m_fuzz, 'g', 12));
        commandMessage(tr("Overkill fuzz distance set to %1").arg(m_fuzz));
        updateMouseButtonHints();
        return true;
    }
    return LC_ActionPreSelectionAwareBase::doProcessCommand(status, command);
}

RS2::CursorType LC_ActionModifyOverkill::doGetMouseCursorSelected([[maybe_unused]] int status) {
    return RS2::DelCursor;
}
