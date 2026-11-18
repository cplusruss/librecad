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

#ifndef QG_PENTOOLBAR_H
#define QG_PENTOOLBAR_H
#include <QToolBar>

#include "lc_graphicviewaware.h"
#include "rs_layerlistlistener.h"
#include "rs_pen.h"

class RS_LayerList;
class QG_ColorBox;
class QG_WidthBox;
class QG_LineTypeBox;
class QG_LayerBox;

/**
 * A toolbar that offers all widgets for choosing a pen.
 */
class QG_PenToolBar: public QToolBar, public LC_GraphicViewAware, public RS_LayerListListener {
    Q_OBJECT
public:
	QG_PenToolBar( const QString & title, QWidget * parent = 0 );
    ~QG_PenToolBar() override;
    void updateByLayer(RS_Layer* l);

    RS_Pen getPen() const;

    // Methods from RS_LayerListListener Interface:
    void layerActivated(RS_Layer*) override;
    void layerEdited(RS_Layer*) override;
    void layerAdded(RS_Layer*) override;
    void layerRemoved(RS_Layer*) override;
    void layerListModified(bool) override;

    void setLayerColor(RS_Color color, bool b);
    void setLayerWidth(RS2::LineWidth width, bool b);
    void setLayerLineType(RS2::LineType lineType, bool b);

    void setColor(RS_Color color);
    void setWidth(RS2::LineWidth width);
    void setLineType(RS2::LineType lineType);
    void emitPenChanged();

    // Reflects the current selection in the layer combo: the shared layer when
    // all selected entities share one, "(Multiple Layers)" when they differ, or
    // the active layer again when nothing is selected.
    void updateForSelection(int selectionCount, RS_Layer* commonLayer, bool multiple);

    void setGraphicView(RS_GraphicView* gview) override;
public slots:
    void slotColorChanged(const RS_Color& color);
    void slotWidthChanged(RS2::LineWidth w);
    void slotLineTypeChanged(RS2::LineType w);
    void slotLayerBoxChanged(RS_Layer* layer);
signals:
    void penChanged(RS_Pen);
    // Emitted only on genuine user interaction with a combo (not programmatic/layer
    // updates), so the changed attribute can be applied to the current selection.
    void penAttributeChangedForSelection(RS_Pen pen, bool color, bool width, bool lineType);
    // Emitted on genuine user layer-combo interaction, so the current selection can
    // be moved onto the chosen layer.
    void layerChangedForSelection(RS_Layer* layer);
private:
    void setLayerList(RS_LayerList* ll);
    void refreshLayerBox();
    void clearMultipleLayersItem();
    void selectActiveLayerInBox();
    RS_LayerList* m_layerList{nullptr};
    // True while the combos are updated programmatically (layer switch, state sync) so
    // the user-edit slots don't push the change onto selected entities.
    bool m_suppressSelectionApply{false};
    // True while a temporary "(Multiple Layers)" item occupies index 0 of the layer combo.
    bool m_multipleLayersShown{false};
	std::unique_ptr<RS_Pen> m_currentPen;
	std::unique_ptr<QG_LayerBox> m_layerBox;
	std::unique_ptr<QG_ColorBox> m_colorBox;
	std::unique_ptr<QG_WidthBox> m_widthBox;
	std::unique_ptr<QG_LineTypeBox> m_lineTypeBox;
};

#endif
