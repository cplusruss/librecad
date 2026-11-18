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

#include "rs_actioneditcopy.h"

#include "lc_emfexporter.h"
#include "rs_clipboard.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_modification.h"
#include "rs_preview.h"
#include "rs_units.h"

/**
 * Constructor.
 *
 * @param undo true for undo and false for redo.
 */

RS_ActionEditCopyPaste::RS_ActionEditCopyPaste(LC_ActionContext *actionContext, RS2::ActionType actionType)
    :LC_ActionPreSelectionAwareBase("Edit Copy", actionContext, actionType)
    , m_referencePoint{new RS_Vector{}}{
}

RS_ActionEditCopyPaste::~RS_ActionEditCopyPaste() = default;

void RS_ActionEditCopyPaste::init(int status) {
    LC_ActionPreSelectionAwareBase::init(status);
    *m_referencePoint = RS_Vector(false);
    if (m_actionType  == RS2::ActionEditPaste) {
        m_selectionComplete  = true;
        // TEMP DIAGNOSTIC - remove once cross-document paste is resolved.
        commandMessage(QString("paste init: clipboard=%1 container=%2 graphic=%3")
                       .arg(RS_CLIPBOARD->getGraphic() != nullptr
                                ? int(RS_CLIPBOARD->getGraphic()->count()) : -1)
                       .arg(m_container != nullptr ? int(m_container->count()) : -1)
                       .arg(m_graphic != nullptr ? 1 : 0));
    }
    if (m_selectionComplete) {
        updateMouseButtonHints();
    }
}

void RS_ActionEditCopyPaste::onSelectionCompleted([[maybe_unused]] bool singleEntity, bool fromInit) {
    switch (m_actionType) {
        case RS2::ActionEditCutQuick:
        case RS2::ActionEditCopyQuick:
        case RS2::ActionEditCopyEmf:
        case RS2::ActionEditCopyEmfBW: {
            LC_ActionPreSelectionAwareBase::onSelectionCompleted(singleEntity, fromInit);
            break;
        }
        default: {
            setSelectionComplete(isAllowTriggerOnEmptySelection(), fromInit);
            updateMouseButtonHints();
            updateSelectionWidget();
        }
    }
}

void RS_ActionEditCopyPaste::doTrigger([[maybe_unused]]bool keepSelected) {
    switch (m_actionType){
        case  RS2::ActionEditCut:
        case  RS2::ActionEditCutQuick:
        case  RS2::ActionEditCopy:
        case  RS2::ActionEditCopyQuick:{
            RS_Modification m(*m_container, m_viewport);
            m.copy(*m_referencePoint, m_actionType ==  RS2::ActionEditCut || m_actionType == RS2::ActionEditCutQuick);

            if (m_invokedWithControl){
                m_actionType = RS2::ActionEditPaste;
                m_invokedWithControl = false;
            }
            else{
                finish(false);
            }
            break;
        }
        case RS2::ActionEditCopyEmf:
        case RS2::ActionEditCopyEmfBW: {
            // Copy the selection into the internal clipboard (re-using the entity
            // cloning, layer and block handling), then render that graphic to an
            // EMF on the Windows clipboard for pasting into Visio / PowerPoint.
            // The BW variant renders everything black instead of DXF colours.
            RS_Modification m(*m_container, m_viewport);
            m.copy(*m_referencePoint, false);
            const bool blackAndWhite = (m_actionType == RS2::ActionEditCopyEmfBW);
            LC_EmfExporter::copyGraphicToClipboard(RS_CLIPBOARD->getGraphic(), blackAndWhite);
            finish(false);
            break;
        }
        case RS2::ActionEditPaste: {
            RS_Modification m(*m_container, m_viewport);
            const unsigned before = m_container->count();
            m.paste(RS_PasteData(*m_referencePoint, 1.0, 0.0, false, ""));
            // TEMP DIAGNOSTIC - remove once cross-document paste is resolved.
            commandMessage(QString("paste at (%1, %2) valid=%3: container %4 -> %5, clipboard=%6")
                           .arg(m_referencePoint->x).arg(m_referencePoint->y)
                           .arg(m_referencePoint->valid ? 1 : 0)
                           .arg(before).arg(m_container->count())
                           .arg(int(RS_CLIPBOARD->getGraphic()->count())));
            // Sticky paste: stay active so the user can keep placing copies with each
            // click; Esc or right-click finishes. Ctrl is no longer required to repeat.
            break;
        }
        default:
            break;
    }
}

void RS_ActionEditCopyPaste::onMouseMoveEventSelected(int status, LC_MouseEvent *e) {
    if (status==SetReferencePoint) {
         switch (m_actionType) {
            case RS2::ActionEditCut:
            case RS2::ActionEditCopy:{
                break;
            }
            case RS2::ActionEditPaste:{
                *m_referencePoint = e->snapPoint;
                m_preview->addAllFrom(*RS_CLIPBOARD->getGraphic(), m_viewport);
                m_preview->move(*m_referencePoint);

                if (m_graphic) {
                    RS2::Unit sourceUnit = RS_CLIPBOARD->getGraphic()->getUnit();
                    RS2::Unit targetUnit = m_graphic->getUnit();
                    double const f = RS_Units::pasteFactor(1.0, sourceUnit, targetUnit);
                    m_preview->scale(*m_referencePoint, {f, f});
                }
                break;
            }
            default:
                break;
        }
    }
    else {
        deleteSnapper();
    }
}

void RS_ActionEditCopyPaste::onMouseLeftButtonReleaseSelected([[maybe_unused]]int status, LC_MouseEvent *e) {
    m_invokedWithControl = e->isControl;
    fireCoordinateEventForSnap(e);
}

void RS_ActionEditCopyPaste::onMouseRightButtonReleaseSelected(int status, [[maybe_unused]]LC_MouseEvent *e) {
    initPrevious(status);
}

void RS_ActionEditCopyPaste::onCoordinateEvent( [[maybe_unused]]int status, [[maybe_unused]]bool isZero, const RS_Vector &pos) {
    *m_referencePoint = pos;
    trigger();
}

void RS_ActionEditCopyPaste::updateMouseButtonHintsForSelection() {
   switch (m_actionType) {
       case RS2::ActionEditCut: {
           updateMouseWidgetTRCancel(tr("Select to cut (Enter to complete)"),  MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Set point after selection")));
           break;
       }
       case RS2::ActionEditCutQuick: {
           updateMouseWidgetTRCancel(tr("Select to cut (Enter to complete)"),  MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Cut right after selection")));
           break;
       }
       case RS2::ActionEditCopy: {
           updateMouseWidgetTRCancel(tr("Select to copy (Enter to complete)"), MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Set point after selection")));
           break;
       }
       case RS2::ActionEditCopyQuick: {
           updateMouseWidgetTRCancel(tr("Select to cut (Enter to complete)"),  MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Copy right after selection")));
           break;
       }
       case RS2::ActionEditCopyEmf: {
           updateMouseWidgetTRCancel(tr("Select to copy as EMF (Enter to complete)"), MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Copy to clipboard as metafile")));
           break;
       }
       case RS2::ActionEditCopyEmfBW: {
           updateMouseWidgetTRCancel(tr("Select to copy as B/W EMF (Enter to complete)"), MOD_SHIFT_AND_CTRL(tr("Select contour"),tr("Copy to clipboard as black/white metafile")));
           break;
       }
       default:
           break;
   }
}

void RS_ActionEditCopyPaste::updateMouseButtonHintsForSelected(int status) {
    switch (status) {
        case SetReferencePoint: {
            switch (m_actionType) {
                case RS2::ActionEditCut:
                case RS2::ActionEditCopy: {
                    updateMouseWidgetTRCancel(tr("Specify reference point"), MOD_CTRL(tr("Paste Immediately")));
                    break;
                }
                case RS2::ActionEditPaste: {
                    updateMouseWidgetTRCancel(tr("Specify point to paste a copy (Esc to finish)"));
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            updateMouseWidget();
            break;
        }
    }
}

RS2::CursorType RS_ActionEditCopyPaste::doGetMouseCursorSelected([[maybe_unused]]int status){
    return RS2::CadCursor;
}
