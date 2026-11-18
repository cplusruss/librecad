/*******************************************************************************
*
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2025 LibreCAD.org
 Copyright (C) 2025 sand1024

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

#include "lc_defaultactioncontext.h"

#include "lc_actionoptionsmanager.h"
#include "lc_qtstatusbarmanager.h"
#include "qc_applicationwindow.h"
#include "qg_actionhandler.h"
#include "qg_commandwidget.h"
#include "qg_coordinatewidget.h"
#include "qg_mousewidget.h"
#include "qg_pentoolbar.h"
#include "qg_selectionwidget.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_graphicview.h"

LC_DefaultActionContext::LC_DefaultActionContext(QG_ActionHandler* actionHandler):m_actionHandler{actionHandler} {
}

void LC_DefaultActionContext::addOptionsWidget(LC_ActionOptionsWidget *widget){
    m_actionOptionsManager->addOptionsWidget(widget);
}

void LC_DefaultActionContext::removeOptionsWidget(LC_ActionOptionsWidget *widget){
    m_actionOptionsManager->removeOptionsWidget(widget);
}

void LC_DefaultActionContext::requestSnapDistOptions(double *dist, bool on){
    m_actionOptionsManager->requestSnapDistOptions(dist, on);
}

void LC_DefaultActionContext::requestSnapMiddleOptions(int *middlePoints, bool on){
    m_actionOptionsManager->requestSnapMiddleOptions(middlePoints, on);
}

void LC_DefaultActionContext::hideSnapOptions(){
    m_actionOptionsManager->hideSnapOptions();
}

void LC_DefaultActionContext::updateSelectionWidget(int countSelected, double selectedLength){
    if (m_selectionWidget != nullptr) {
        m_selectionWidget->setNumber(countSelected);
        m_selectionWidget->setTotalLength(selectedLength);
    }
    // Only recompute the toolbar layer combo when the selection count actually
    // changes. This method is called from hot paths (every click/trigger), and
    // an unconditional scan here was making interaction sluggish on large drawings.
    const bool countChanged = (countSelected != m_selectionCount);
    m_selectionCount = countSelected;
    if (!countChanged) {
        return;
    }

    // Reflect the selection's layer in the toolbar layer combo. deep=false: the
    // combo cares about the layers of the top-level selected entities only, not
    // every nested block child (which would be an expensive full traversal).
    auto& appWin = QC_ApplicationWindow::getAppWindow();
    QG_PenToolBar* penToolBar = (appWin != nullptr) ? appWin->getPenToolBar() : nullptr;
    if (penToolBar != nullptr) {
        RS_Layer* commonLayer = nullptr;
        bool multiple = false;
        if (countSelected > 0) {
            RS_GraphicView* gv = getGraphicView();
            RS_EntityContainer* container = (gv != nullptr) ? gv->getContainer() : nullptr;
            if (container != nullptr) {
                std::vector<RS_Entity*> selected;
                container->collectSelected(selected, false);
                bool first = true;
                for (RS_Entity* e : selected) {
                    RS_Layer* l = e->getLayer();
                    if (first) {
                        commonLayer = l;
                        first = false;
                    }
                    else if (l != commonLayer) {
                        multiple = true;
                        commonLayer = nullptr;
                        break;
                    }
                }
            }
        }
        penToolBar->updateForSelection(countSelected, commonLayer, multiple);
    }
}

void LC_DefaultActionContext::updateMouseWidget(const QString &left, const QString &right, const LC_ModifiersInfo &modifiers){
    if (m_mouseWidget != nullptr) {
        m_mouseWidget->setHelp(left, right, modifiers);
    }

    if (m_commandWidget != nullptr) {
        m_commandWidget->setCommand(left);
    }

    if (m_statusBarManager != nullptr){
        m_statusBarManager->setActionHelp(left, right, modifiers);
    }
}

void LC_DefaultActionContext::commandMessage(const QString &message){
    if (m_commandWidget != nullptr) {
        m_commandWidget->appendHistory(message);
    }
}

void LC_DefaultActionContext::commandPrompt(const QString &message){
    if (m_commandWidget != nullptr) {
        m_commandWidget->setCommand(message);
    }
}

void LC_DefaultActionContext::updateCoordinateWidget(const RS_Vector &abs, const RS_Vector &rel, bool updateFormat){
    if (m_coordinateWidget != nullptr) {
        m_coordinateWidget->setCoordinates(abs, rel, updateFormat);
    }
}

void LC_DefaultActionContext::setDocumentAndView(RS_Document *document, RS_GraphicView *view){
    LC_ActionContext::setDocumentAndView(document, view);
    if (m_coordinateWidget != nullptr) {
        m_coordinateWidget->setGraphicView(view);
    }
}

void LC_DefaultActionContext::setSnapMode(const RS_SnapMode& mode) {
    m_actionHandler->setSnaps(mode);
}

void LC_DefaultActionContext::setCurrentAction(RS2::ActionType action, void* data) {
    m_actionHandler->setCurrentAction(action, data);
}

RS_ActionInterface* LC_DefaultActionContext::getCurrentAction() {
    return m_actionHandler->getCurrentAction();
}

void LC_DefaultActionContext::deleteActionHandler() {
    delete m_actionHandler;
}
