/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2024 LibreCAD.org
 Copyright (C) 2024 sand1024

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

#include "qg_pentoolbar.h"

#include <QSignalBlocker>

#include "qg_colorbox.h"
#include "qg_widthbox.h"
#include "qg_linetypebox.h"
#include "qg_layerbox.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_layer.h"
#include "rs_layerlist.h"
#include "rs_pen.h"

namespace {
    // Sets a bool true for the duration of the current scope. Used to mark combo
    // updates as programmatic so the user-edit slots don't apply them to the selection.
    struct ScopedFlag {
        bool& m_flag;
        explicit ScopedFlag(bool& flag) : m_flag(flag) { m_flag = true; }
        ~ScopedFlag() { m_flag = false; }
    };
}

/**
 * Constructor.
 */
QG_PenToolBar::QG_PenToolBar( const QString & title, QWidget * parent )
		: QToolBar(title, parent)
		, m_currentPen(new RS_Pen{})
		, m_layerBox(new QG_LayerBox{this})
		, m_colorBox(new QG_ColorBox{true, false, this, "colorbox"})
		, m_widthBox(new QG_WidthBox{true, false, this, "widthbox"})
		, m_lineTypeBox(new QG_LineTypeBox{true, false, this, "lineTypebox"})
{
    m_layerBox->setToolTip(tr("Active layer (moves selection when set)"));
    m_layerBox->setMinimumContentsLength(12);
    connect(m_layerBox.get(), &QG_LayerBox::layerChanged, this, &QG_PenToolBar::slotLayerBoxChanged);

    m_colorBox->setToolTip(tr("Line color"));
    connect(m_colorBox.get(), &QG_ColorBox::colorChanged, this, &QG_PenToolBar::slotColorChanged);

    m_widthBox->setToolTip(tr("Line width"));
    connect(m_widthBox.get(), &QG_WidthBox::widthChanged,this, &QG_PenToolBar::slotWidthChanged);

    m_lineTypeBox->setToolTip(tr("Line type"));
    connect(m_lineTypeBox.get(), &QG_LineTypeBox::lineTypeChanged,this,&QG_PenToolBar::slotLineTypeChanged);

    // Width box defaults to its first item (By Layer), which resolves to layer 0's wide pen.
    m_widthBox->setWidth(RS2::Width00);

    m_currentPen->setColor(m_colorBox->getColor());
    m_currentPen->setWidth(m_widthBox->getWidth());
    m_currentPen->setLineType(m_lineTypeBox->getLineType());

    addWidget(m_layerBox.get());
    addWidget(m_colorBox.get());
    addWidget(m_widthBox.get());
    addWidget(m_lineTypeBox.get());
}

/**
 * Destructor
 */
QG_PenToolBar::~QG_PenToolBar() = default;

void QG_PenToolBar::updateByLayer(RS_Layer* l) {
    if (l==nullptr) return;
    ScopedFlag guard(m_suppressSelectionApply);
    m_colorBox->setLayerColor(l->getPen().getColor());
    m_widthBox->setLayerWidth(l->getPen().getWidth());
    m_lineTypeBox->setLayerLineType(l->getPen().getLineType());
}

/**
 * Called by the layer list if this object was added as a listener
 * to a layer list.
 */
void QG_PenToolBar::layerActivated(RS_Layer* l) {
    updateByLayer(l);
    if (m_layerBox != nullptr && l != nullptr) {
        ScopedFlag guard(m_suppressSelectionApply);
        QString name = l->getName();
        m_layerBox->setLayer(name);
    }
}

void QG_PenToolBar::layerAdded(RS_Layer*) {
    refreshLayerBox();
}

void QG_PenToolBar::layerRemoved(RS_Layer*) {
    refreshLayerBox();
}

void QG_PenToolBar::layerListModified(bool) {
    refreshLayerBox();
}

// Rebuilds the layer combo from the current list and selects the active layer,
// without pushing anything onto the selection (programmatic update).
void QG_PenToolBar::refreshLayerBox() {
    if (m_layerBox == nullptr || m_layerList == nullptr) {
        return;
    }
    ScopedFlag guard(m_suppressSelectionApply);
    m_multipleLayersShown = false;   // init() clears items, dropping any temp entry
    m_layerBox->init(*m_layerList, false, false);
    RS_Layer* active = m_layerList->getActive();
    if (active != nullptr) {
        QString name = active->getName();
        m_layerBox->setLayer(name);
    }
}

void QG_PenToolBar::clearMultipleLayersItem() {
    if (m_multipleLayersShown && m_layerBox != nullptr) {
        if (m_layerBox->count() > 0 && m_layerBox->itemText(0) == tr("(Multiple Layers)")) {
            m_layerBox->removeItem(0);
        }
        m_multipleLayersShown = false;
    }
}

void QG_PenToolBar::selectActiveLayerInBox() {
    if (m_layerList == nullptr || m_layerBox == nullptr) {
        return;
    }
    RS_Layer* active = m_layerList->getActive();
    if (active != nullptr) {
        int idx = m_layerBox->findText(active->getName());
        if (idx >= 0) {
            m_layerBox->setCurrentIndex(idx);
        }
    }
}

void QG_PenToolBar::updateForSelection(int selectionCount, RS_Layer* commonLayer, bool multiple) {
    if (m_layerBox == nullptr) {
        return;
    }
    ScopedFlag guard(m_suppressSelectionApply);
    QSignalBlocker block(m_layerBox.get());
    clearMultipleLayersItem();
    if (selectionCount <= 0) {
        selectActiveLayerInBox();               // nothing selected -> back to active layer
    }
    else if (multiple) {
        m_layerBox->insertItem(0, tr("(Multiple Layers)"));
        m_multipleLayersShown = true;
        m_layerBox->setCurrentIndex(0);
    }
    else if (commonLayer != nullptr) {
        int idx = m_layerBox->findText(commonLayer->getName());
        if (idx >= 0) {
            m_layerBox->setCurrentIndex(idx);
        }
        else {
            selectActiveLayerInBox();
        }
    }
    else {
        selectActiveLayerInBox();
    }
}

// User picked a layer in the combo: make it active and move any selection onto it.
void QG_PenToolBar::slotLayerBoxChanged(RS_Layer* layer) {
    if (m_suppressSelectionApply || layer == nullptr) {
        return;
    }
    if (m_layerList != nullptr) {
        m_layerList->activate(layer, true);
    }
    emit layerChangedForSelection(layer);
}

void QG_PenToolBar::setLayerColor(RS_Color color, bool updateSelection){
    ScopedFlag guard(m_suppressSelectionApply);
    m_colorBox->setLayerColor(color);
    if (updateSelection){
        m_colorBox->setCurrentIndex(0);
    }
    m_currentPen->setColor(color);
    emitPenChanged();
}

void QG_PenToolBar::setColor(RS_Color color){
    ScopedFlag guard(m_suppressSelectionApply);
    m_colorBox->setColor(color);
}

void QG_PenToolBar::setLayerLineType(RS2::LineType lineType, bool updateSelection){
    ScopedFlag guard(m_suppressSelectionApply);
    m_lineTypeBox->setLayerLineType(lineType);
    if (updateSelection){
        m_lineTypeBox->setCurrentIndex(0);
    }
    m_currentPen->setLineType(lineType);
    emitPenChanged();
}

void QG_PenToolBar::setLineType(RS2::LineType lineType){
    ScopedFlag guard(m_suppressSelectionApply);
    m_lineTypeBox->setLineType(lineType);
}

void QG_PenToolBar::setLayerWidth(RS2::LineWidth width, bool updateSelection){
    ScopedFlag guard(m_suppressSelectionApply);
    m_widthBox->setLayerWidth(width);
    if (updateSelection){
        m_widthBox->setCurrentIndex(0);
    }
    m_currentPen->setWidth(width);
    emitPenChanged();
}

void QG_PenToolBar::setWidth(RS2::LineWidth width){
    ScopedFlag guard(m_suppressSelectionApply);
    m_widthBox->setWidth(width);
}

void QG_PenToolBar::emitPenChanged(){
    emit penChanged(*m_currentPen);
}

void QG_PenToolBar::setLayerList(RS_LayerList* ll) {
    if (m_layerList != nullptr) {
        m_layerList->removeListener(this);
    }
    m_layerList = ll;
    if (ll != nullptr) {
        m_layerList->addListener(this);
        refreshLayerBox();
        auto activeLayer = m_layerList->getActive();
        if (activeLayer != nullptr) {
            updateByLayer(activeLayer);
        }
    }
}

void QG_PenToolBar::setGraphicView(RS_GraphicView* gv) {
    if (gv == nullptr) {
        setLayerList(nullptr);
    }
    else {
        auto graphic = gv->getGraphic();
        if (graphic == nullptr) {
            setLayerList(nullptr);
        }
        else {
            auto layerList = graphic->getLayerList();
            setLayerList(layerList);
        }
    }
}

RS_Pen QG_PenToolBar::getPen() const {
    return *m_currentPen;
}

/**
 * Called by the layer list (if this object was previously
 * added as a listener to a layer list).
 */
void QG_PenToolBar::layerEdited(RS_Layer* l) {
    updateByLayer(l);
    refreshLayerBox();   // a rename comes through here; rebuild the combo items
}

/**
 * Called when the color was changed by the user.
 */
void QG_PenToolBar::slotColorChanged(const RS_Color& color) {
    m_currentPen->setColor(color);

    emit penChanged(*m_currentPen);
    if (!m_suppressSelectionApply) {
        emit penAttributeChangedForSelection(*m_currentPen, true, false, false);
    }
}

/**
 * Called when the width was changed by the user.
 */
void QG_PenToolBar::slotWidthChanged(RS2::LineWidth w) {
    m_currentPen->setWidth(w);

    emit penChanged(*m_currentPen);
    if (!m_suppressSelectionApply) {
        emit penAttributeChangedForSelection(*m_currentPen, false, true, false);
    }
}

/**
 * Called when the linetype was changed by the user.
 */
void QG_PenToolBar::slotLineTypeChanged(RS2::LineType w) {
    m_currentPen->setLineType(w);

    emit penChanged(*m_currentPen);
    if (!m_suppressSelectionApply) {
        emit penAttributeChangedForSelection(*m_currentPen, false, false, true);
    }
}
