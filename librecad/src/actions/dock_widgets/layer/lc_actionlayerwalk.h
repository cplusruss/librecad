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

#ifndef LC_ACTIONLAYERWALK_H
#define LC_ACTIONLAYERWALK_H

#include "rs_actioninterface.h"

/**
 * AutoCAD LAYWALK. Opens a modal list of all layers; clicking a name shows
 * only that layer (freezing every other). Closing restores the original
 * visibility map so the dialog itself is non-destructive.
 *
 * Differences from AutoCAD:
 *   * No "Filter" pane / saved layer states - that's a follow-up.
 *   * No multi-select (Ctrl-click to show several at once) - also follow-up.
 *   * "Apply" / "Cancel" replaced with a single Close button; closing keeps
 *     whatever you have showing. AutoCAD's Cancel restores; here you just
 *     run LAYON afterwards if you want everything back.
 */
class LC_ActionLayerWalk : public RS_ActionInterface {
    Q_OBJECT
public:
    explicit LC_ActionLayerWalk(LC_ActionContext* actionContext);
    void trigger() override;
    void init(int status) override;
};

#endif // LC_ACTIONLAYERWALK_H
