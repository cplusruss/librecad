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

#ifndef LC_ACTIONLAYERMANAGER_H
#define LC_ACTIONLAYERMANAGER_H

#include "rs_actioninterface.h"

/**
 * AutoCAD LAYER. Surfaces LibreCAD's Layers dock widget - raises it if it
 * exists but is hidden, focuses it so subsequent keyboard nav goes to the
 * layer list. Fires on the command-line aliases "layer" / "la".
 */
class LC_ActionLayerManager : public RS_ActionInterface {
    Q_OBJECT
public:
    explicit LC_ActionLayerManager(LC_ActionContext* actionContext);
    void trigger() override;
    void init(int status) override;
};

#endif // LC_ACTIONLAYERMANAGER_H
