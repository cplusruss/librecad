/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** AutoCAD-style PURGE: removes layers that no entity uses.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
**********************************************************************/

#ifndef RS_ACTIONLAYERSPURGE_H
#define RS_ACTIONLAYERSPURGE_H

#include "rs_actioninterface.h"

/**
 * Deletes all unused (entity-free) layers, like AutoCAD's PURGE.
 * Keeps layer "0" and the active layer.
 */
class RS_ActionLayersPurge : public RS_ActionInterface {
    Q_OBJECT
public:
    RS_ActionLayersPurge(LC_ActionContext *actionContext);

    void init(int status) override;
    void trigger() override;
};

#endif
