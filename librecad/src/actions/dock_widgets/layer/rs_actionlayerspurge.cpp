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

#include "rs_actionlayerspurge.h"

#include <set>

#include "rs_block.h"
#include "rs_blocklist.h"
#include "rs_debug.h"
#include "rs_entitycontainer.h"
#include "rs_graphic.h"
#include "rs_layer.h"
#include "rs_layerlist.h"

namespace {
// Collect the names of every layer referenced by an entity, recursing into
// containers (polylines, inserts, block definitions).
void collectUsedLayers(RS_EntityContainer* c, std::set<QString>& used) {
    if (c == nullptr) {
        return;
    }
    for (RS_Entity* e : *c) {
        if (e == nullptr) {
            continue;
        }
        RS_Layer* l = e->getLayer();
        if (l != nullptr) {
            used.insert(l->getName());
        }
        if (e->isContainer()) {
            collectUsedLayers(static_cast<RS_EntityContainer*>(e), used);
        }
    }
}
}

RS_ActionLayersPurge::RS_ActionLayersPurge(LC_ActionContext *actionContext)
    : RS_ActionInterface("Purge Unused Layers", actionContext, RS2::ActionLayersPurge) {
}

void RS_ActionLayersPurge::trigger() {
    if (m_graphic == nullptr) {
        finish(false);
        return;
    }
    RS_LayerList* layerList = m_graphic->getLayerList();

    std::set<QString> used;
    collectUsedLayers(m_graphic, used);
    RS_BlockList* blocks = m_graphic->getBlockList();
    if (blocks != nullptr) {
        for (int i = 0; i < blocks->count(); i++) {
            collectUsedLayers(blocks->at(i), used);
        }
    }

    RS_Layer* active = m_graphic->getActiveLayer();
    std::vector<RS_Layer*> toRemove;
    for (unsigned i = 0; i < layerList->count(); i++) {
        RS_Layer* l = layerList->at(i);
        if (l == nullptr || l == active) {
            continue;
        }
        if (l->getName() == "0") {          // keep the default layer
            continue;
        }
        if (used.find(l->getName()) == used.end()) {
            toRemove.push_back(l);
        }
    }

    for (RS_Layer* l : toRemove) {
        m_graphic->removeLayer(l);          // undo-aware; only unused layers reach here
    }
    commandMessage(tr("Purge: removed %1 unused layer(s)").arg((int)toRemove.size()));
    if (!toRemove.empty()) {
        redrawDrawing();
    }
    finish(false);
}

void RS_ActionLayersPurge::init(int status) {
    RS_ActionInterface::init(status);
    trigger();
}
