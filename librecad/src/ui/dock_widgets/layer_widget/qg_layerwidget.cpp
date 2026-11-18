/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2015 A. Stebich (librecad@mail.lordofbikes.de)
** Copyright (C) 2011 Rallaz (rallazz@gmail.com)
** Copyright (C) 2010-2011 R. van Twisk (librecad@rvt.dds.nl)
**
**
** This file is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
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

#include "qg_layerwidget.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCursor>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QObject>
#include <QScrollBar>
#include <QTableView>
#include <QToolButton>

#include "lc_actiongroupmanager.h"
#include "lc_flexlayout.h"
#include "lc_theme.h"
#include "qc_applicationwindow.h"
#include "qc_mdiwindow.h"
#include "qg_actionhandler.h"
#include "qg_colorbox.h"
#include "qg_linetypebox.h"
#include "qg_widthbox.h"
#include "rs_debug.h"
#include "rs_document.h"
#include "rs_entitycontainer.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"
#include "rs_layer.h"
#include "rs_layerlist.h"
#include "rs_pen.h"
#include "rs_settings.h"

// Human-readable name for a linetype, sourced once from the authoritative
// QG_LineTypeBox item list so the column label never drifts from the editor.
static QString layerLineTypeName(RS2::LineType t) {
    static const QMap<int, QString> names = []() {
        QMap<int, QString> m;
        QG_LineTypeBox box(false, false, nullptr);
        for (int i = 0; i < box.count(); ++i) {
            m.insert(box.itemData(i).toInt(), box.itemText(i));
        }
        return m;
    }();
    return names.value(static_cast<int>(t), QString::number(static_cast<int>(t)));
}

QG_LayerModel::QG_LayerModel(QObject * parent) : ::QAbstractTableModel(parent) {
    m_iconLayerVisible = QIcon(":/icons/visible.lci");
    //layerHidden = QIcon(":/icons/invisible.lci");
    m_iconLayerHidden = QIcon(":/icons/not_visible.lci");
    m_iconLayerDefreeze = QIcon(":/icons/unlocked.lci");
    m_iconLayerFreeze = QIcon(":/icons/locked.lci");
    m_iconLayerPrint = QIcon(":/icons/print.lci");
    m_iconLayerNoPrint = QIcon(":/icons/noprint.lci");
    m_iconLayerConstruction = QIcon(":/icons/construction_layer.lci");
    m_iconLayerNoConstruction = QIcon(":/icons/noconstruction.lci");
}

int QG_LayerModel::rowCount ( const QModelIndex & /*parent*/ ) const {
    return m_listLayer.size();
}

QModelIndex QG_LayerModel::parent ( const QModelIndex & /*index*/ ) const {
    return QModelIndex();
}

QModelIndex QG_LayerModel::index ( int row, int column, const QModelIndex & /*parent*/ ) const {
    if ( row >= m_listLayer.size() || row < 0)
        return QModelIndex();
    return createIndex ( row, column);
}

void QG_LayerModel::setLayerList(RS_LayerList* ll) {
    /* since 4.6 the recommended way is to use begin/endResetModel()
     * TNick <nicu.tofan@gmail.com>
     */
    beginResetModel();
    m_listLayer.clear();
    m_layerListRef = ll;
    if (ll == nullptr) {
        endResetModel();
        return;
    }
    for (unsigned i=0; i < ll->count(); ++i) {
        m_listLayer.append(ll->at(i));
    }
    setActiveLayer(ll->getActive());
    std::sort( m_listLayer.begin(), m_listLayer.end(), [](const RS_Layer *s1, const RS_Layer *s2)-> bool{
        return s1->getName() < s2->getName();
    } );

    //called to force redraw
    endResetModel();
}

RS_Layer *QG_LayerModel::getLayer(int row) const {
    if ( row >= m_listLayer.size() || row < 0) {
        return nullptr;
    }
    return m_listLayer.at(row);
}

QModelIndex QG_LayerModel::getIndex (RS_Layer * lay) const {
    int row = m_listLayer.indexOf(lay);
    if (row<0) {
        return {};
    }
    return createIndex (row, NAME);
}
QVariant QG_LayerModel::data ( const QModelIndex & index, int role ) const{
    if (!index.isValid() || index.row() >= m_listLayer.size())
        return QVariant();

    RS_Layer *layer{m_listLayer.at(index.row())};
    int col{index.column()};

    switch (role) {
        case Qt::DecorationRole:
            switch (col) {
                case VISIBLE:
                    if (!layer->isFrozen()) {
                        return m_iconLayerVisible;
                    }
                    return m_iconLayerHidden;

                case LOCKED:
                    if (!layer->isLocked()) {
                        return m_iconLayerDefreeze;
                    }
                    return m_iconLayerFreeze;

                case PRINT:
                    if (!layer->isPrint()) {
                        return m_iconLayerNoPrint;
                    }
                    return m_iconLayerPrint;

                case CONSTRUCTION:
                    if (!layer->isConstruction()) {
                        return m_iconLayerNoConstruction;
                    }
                    return m_iconLayerConstruction;

                default:
                    break;
            }
            break;

        case Qt::EditRole:      // pre-fills the inline rename editor
        case Qt::DisplayRole:
            if (NAME == col) {
                return layer->getName();
            }
            if (WIDTH == col) {
                const int w = static_cast<int>(layer->getPen().getWidth());
                if (w < 0) {
                    return QStringLiteral("-");   // ByLayer/ByBlock/Default
                }
                return QString::asprintf("%.2f", w / 100.0);   // enum units = mm*100
            }
            if (LINETYPE == col) {
                return layerLineTypeName(layer->getPen().getLineType());
            }
            break;

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
        case Qt::BackgroundRole:
#else
    case Qt::BackgroundColorRole:
#endif
            if (COLOR_SAMPLE == col) {
                return layer->getPen().getColor().toQColor();
            }
            break;

        case Qt::FontRole:
            if (NAME == col) {
                if (m_activeLayer && m_activeLayer == layer) {
                    QFont font;
                    font.setBold(true);
                    return font;
                }
            }
            break;
        default:
            break;
    }

    return QVariant();
}

bool QG_LayerModel::setData(const QModelIndex & index, const QVariant & value, int role) {
    if (!index.isValid() || role != Qt::EditRole || index.column() != NAME
        || m_layerListRef == nullptr) {
        return false;
    }
    RS_Layer* lay = getLayer(index.row());
    if (lay == nullptr) {
        return false;
    }
    const QString newName = value.toString().trimmed();
    const QString oldName = lay->getName();
    if (newName.isEmpty() || newName == oldName) {
        return false;
    }
    if (oldName == "0") {
        return false;   // the default layer keeps its name
    }
    RS_Layer* clash = m_layerListRef->find(newName);
    if (clash != nullptr && clash != lay) {
        return false;   // a layer with that name already exists
    }
    // Same clone -> edit() commit the layer dialog uses; entities reference the
    // layer by pointer, so the rename needs no entity fixup.
    RS_Layer* clone = lay->clone();
    clone->setName(newName);
    m_layerListRef->edit(lay, *clone);
    delete clone;
    emit dataChanged(index, index);
    return true;
}

void QG_LayerWidget::addToolbarButton(LC_FlexLayout* layButtons, RS2::ActionType actionType) {
    QAction* action = m_actionGroupManager->getActionByType(actionType);
    if (action != nullptr) {
        auto button = new QToolButton(this);
        button->setDefaultAction(action);
        layButtons->addWidget(button);
    }
}

/**
 * Constructor.
 */
QG_LayerWidget::QG_LayerWidget(LC_ActionGroupManager* agm, QG_ActionHandler *ah, QWidget *parent, const char *name, Qt::WindowFlags f)
    : LC_GraphicViewAwareWidget(parent, name, f){
    m_actionHandler = ah;
    m_actionGroupManager = agm;
    m_layerList = nullptr;
    m_showByBlock = false;
    m_lastLayer = nullptr;

    m_layerModel = new QG_LayerModel(this);
    m_layerView = new QTableView(this);
    m_layerView->setModel(m_layerModel);
    m_layerView->setShowGrid(true);
    m_layerView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_layerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_layerView->setFocusPolicy(Qt::NoFocus);
    // No zebra striping on data grids (project rule).
    m_layerView->setAlternatingRowColors(false);
    // Compact 16px rows per project rule. The vertical header is hidden but
    // its defaultSectionSize is still what drives row height in QTableView.
    // Icons are shrunk to fit (was 24px → clipped at 16px row height).
    m_layerView->verticalHeader()->setDefaultSectionSize(16);
    m_layerView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_layerView->setIconSize(QSize(14, 14));
    // layerView->setMinimumHeight(140);
    m_layerView->setMinimumHeight(60);
    QHeaderView *pHeader {m_layerView->horizontalHeader()};
    pHeader->setMinimumSectionSize( QG_LayerModel::ICONWIDTH + 4);
    pHeader->setStretchLastSection(true);
    pHeader->hide();
    m_layerView->setColumnWidth(QG_LayerModel::VISIBLE, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::VISIBLE, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::LOCKED, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::PRINT, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::CONSTRUCTION, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::COLOR_SAMPLE, QG_LayerModel::ICONWIDTH);
    m_layerView->setColumnWidth(QG_LayerModel::WIDTH, 44);
    m_layerView->setColumnWidth(QG_LayerModel::LINETYPE, 88);
    m_layerView->verticalHeader()->hide();

#ifndef DONT_FORCE_WIDGETS_CSS
    // gridline-color makes the table grid black instead of the default
    // platform gray; the QHeaderView rules carry it into the (hidden but
    // still-rendered) header line too.
    if (LC_Theme::isClassic())
        m_layerView->setStyleSheet(
            "QWidget {background-color: white;}"
            "QScrollBar{ background-color: none }"
            "QTableView { gridline-color: black; }"
        );
#endif

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);

    auto *layButtons = new LC_FlexLayout(0,5,5);

    addToolbarButton(layButtons, RS2::ActionLayersDefreezeAll);
    addToolbarButton(layButtons, RS2::ActionLayersFreezeAll);
    addToolbarButton(layButtons, RS2::ActionLayersUnlockAll);
    addToolbarButton(layButtons, RS2::ActionLayersLockAll);
    addToolbarButton(layButtons, RS2::ActionLayersAdd);
    addToolbarButton(layButtons, RS2::ActionLayersRemove);
    addToolbarButton(layButtons, RS2::ActionLayersEdit);

    // lineEdit to filter layer list with RegEx
    m_matchLayerName = new QLineEdit(this);
    m_matchLayerName->setReadOnly(false);
    m_matchLayerName->setPlaceholderText(tr("Filter"));
    m_matchLayerName->setClearButtonEnabled(true);
    m_matchLayerName->setToolTip(tr("Looking for matching layer names"));
    connect(m_matchLayerName, &QLineEdit::textChanged, this, &QG_LayerWidget::slotUpdateLayerList);

    lay->addWidget(m_matchLayerName);
    lay->addLayout(layButtons);
    lay->addWidget(m_layerView);
    this->setLayout(lay);

    connect(m_layerView, &QTableView::clicked, this, &QG_LayerWidget::slotActivated);
    connect(m_layerView, &QTableView::doubleClicked, this, &QG_LayerWidget::slotLayerDoubleClicked);
    connect(m_layerView->selectionModel(), &QItemSelectionModel::selectionChanged,
             this, &QG_LayerWidget::slotSelectionChanged);

    updateWidgetSettings();
}

/**
 * Sets the layerlist this layer widget should show.
 *
 * @param showByBlock true: show the layer with the name "ByBlock" if
 *                    it exists.
 *                    false: don't show special layer "ByBlock"
 */
void QG_LayerWidget::setLayerList(RS_LayerList* layerList, bool showByBlock) {
    if (m_layerList != nullptr) {
        m_layerList->removeListener(this);
    }

    m_layerList = layerList;
    m_showByBlock = showByBlock;

    if (layerList != nullptr) {
        m_layerList->addListener(this);
    }
    update();
}

void QG_LayerWidget::updateFiltering(){
    if (! m_matchLayerName->text().isEmpty()) {
        slotUpdateLayerList();
    }
}

void QG_LayerWidget::layerAdded(RS_Layer* layer){
    update();   // 1st apply the new layer to the view
    updateFiltering();
    activateLayer(layer);
    update();   // update again, if new layer is last row, the height was wrong
}

void QG_LayerWidget::layerEdited([[maybe_unused]]RS_Layer *rs_layer){
    update();
    updateFiltering();
}

void QG_LayerWidget::layerRemoved([[maybe_unused]]RS_Layer *rs_layer){
    update();
    updateFiltering();
    update();
    activateLayer(m_layerList->at(0));
}

/**
 * @brief getActiveName
 * @return the name of the active layer
 */
QString QG_LayerWidget::getActiveName() const{
    if (m_layerList != nullptr) {
        RS_Layer *activeLayer = m_layerList->getActive();
        if (activeLayer != nullptr) {
            return activeLayer->getName();
        }
    }
    return QString();
}

/**
 * Updates the layer box from the layers in the graphic.
 */
void QG_LayerWidget::update() {
    RS_DEBUG->print("QG_LayerWidget::update() begin");

    if (!m_layerView) {
        RS_DEBUG->print(RS_Debug::D_ERROR, "QG_LayerWidget::update: nullptr layerView");
        return;
    }
    int yPos = m_layerView->verticalScrollBar()->value();
    m_layerView->resizeRowsToContents();
    m_layerView->verticalScrollBar()->setValue(yPos);

    m_layerModel->setLayerList(m_layerList); // allow a null layerList; this clears the widget

    if (m_layerList == nullptr) {
        RS_DEBUG->print(RS_Debug::D_NOTICE, "QG_LayerWidget::update: nullptr layerList");
        return;
    }

    RS_DEBUG->print("QG_LayerWidget::update: reactivating current layer");

    RS_Layer* activeLayer = m_layerList->getActive();
    if (activeLayer == nullptr) {
        RS_DEBUG->print(RS_Debug::D_NOTICE, "QG_LayerWidget::update: nullptr activeLayer");
        m_layerModel->setActiveLayer(nullptr);
        return;
    }

    if (m_lastLayer == nullptr) {
        RS_DEBUG->print(RS_Debug::D_NOTICE, "QG_LayerWidget::update: nullptr lastLayer");
        m_lastLayer = activeLayer;
    }

    restoreSelections();

    RS_DEBUG->print("QG_LayerWidget::update(): OK");
}

void QG_LayerWidget::restoreSelections() {
    QItemSelectionModel* selectionModel = m_layerView->selectionModel();
    for (auto* layer: *m_layerList) {
        if (!layer) continue;
        if (!layer->isVisibleInLayerList()) continue;
        if (!layer->isSelectedInLayerList()) continue;

        QModelIndex idx = m_layerModel->getIndex(layer);
        QItemSelection selection(idx, idx);
        selectionModel->select(selection, QItemSelectionModel::Select);
    }
}

/**
 * Activates the given layer and makes it the active
 * layer in the layerlist.
 */
void QG_LayerWidget::activateLayer(RS_Layer* layer, bool updateScroll) {
    RS_DEBUG->print("QG_LayerWidget::activateLayer() begin");

    if (!layer || !m_layerList) {
        RS_DEBUG->print(RS_Debug::D_ERROR, "QG_LayerWidget::activateLayer: nullptr layer or layerList");
        return;
    }

    m_layerList->activate(layer);

    if (!m_layerModel) {
        RS_DEBUG->print(RS_Debug::D_ERROR, "QG_LayerWidget::activateLayer: nullptr layerModel");
        return;
    }
    QModelIndex idx = m_layerModel->getIndex(layer);

    if (!idx.model() || !m_layerView) {
        RS_DEBUG->print(RS_Debug::D_ERROR, "QG_LayerWidget::activateLayer: invalid layer or nullptr layerView");
        return;
    }

    // remember selected status of the layer
    bool selected = layer->isSelectedInLayerList();

    m_layerView->setCurrentIndex(idx);
    m_layerModel->setActiveLayer(layer);
    m_layerView->viewport()->update();

    // restore selected status of the layer
    QItemSelectionModel::SelectionFlag selFlag;
    if (selected) {
        selFlag = QItemSelectionModel::Select;
    } else {
        selFlag = QItemSelectionModel::Deselect;
    }
    layer->selectedInLayerList(selected);
    m_layerView->selectionModel()->select(QItemSelection(idx, idx), selFlag);

    if (!updateScroll) {
        int yPos = m_layerView->verticalScrollBar()->value();
        m_layerView->verticalScrollBar()->setValue(yPos);
    }

    //update active layer name in mainwindow status bar
    QC_ApplicationWindow::getAppWindow()->slotUpdateActiveLayer();

    RS_DEBUG->print("QG_LayerWidget::activateLayer() end");
}

/**
 * Called when the user activates (highlights) a layer.
 */
void QG_LayerWidget::slotActivated(QModelIndex layerIdx /*const QString& layerName*/) {
    if (!layerIdx.isValid() || m_layerList==nullptr) {
        return;
    }

    RS_Layer* lay = m_layerModel->getLayer(layerIdx.row());
    if (lay == nullptr)
        return;

    if (layerIdx.column() == QG_LayerModel::NAME) {
        m_layerList->activate(lay, true);
        return;
    }

    switch (layerIdx.column()) {
        case QG_LayerModel::VISIBLE:
            m_actionHandler->setCurrentAction(RS2::ActionLayersToggleView, lay);
            break;
        case QG_LayerModel::LOCKED:
            m_actionHandler->setCurrentAction(RS2::ActionLayersToggleLock, lay);
            // m_actionHandler->toggleLock(lay);
            break;
        case QG_LayerModel::PRINT:
            m_actionHandler->setCurrentAction(RS2::ActionLayersTogglePrint, lay);
            // m_actionHandler->togglePrint(lay);
            break;
        case QG_LayerModel::CONSTRUCTION:
            m_actionHandler->setCurrentAction(RS2::ActionLayersToggleConstruction, lay);
            // m_actionHandler->toggleConstruction(lay);
            break;
        case QG_LayerModel::COLOR_SAMPLE:
            editLayerColor(lay);
            break;
        case QG_LayerModel::WIDTH:
            editLayerWidth(lay);
            break;
        case QG_LayerModel::LINETYPE:
            editLayerLineType(lay);
            break;
        default:
            break;
    }
}

void QG_LayerWidget::slotLayerDoubleClicked(QModelIndex layerIdx) {
    if (!layerIdx.isValid() || m_layerList == nullptr) {
        return;
    }
    // Only the name column drives the move: the icon/color/width/linetype cells
    // have their own single-click behaviour and must not also move the selection.
    if (layerIdx.column() != QG_LayerModel::NAME) {
        return;
    }
    RS_Layer* lay = m_layerModel->getLayer(layerIdx.row());
    if (lay == nullptr) {
        return;
    }
    auto& appWin = QC_ApplicationWindow::getAppWindow();
    QC_MDIWindow* w = (appWin != nullptr) ? appWin->getCurrentMDIWindow() : nullptr;
    RS_Document* doc = (w != nullptr) ? w->getDocument() : nullptr;
    if (doc != nullptr && doc->countSelected() > 0) {
        appWin->applyLayerToSelection(lay);   // move the selection onto this layer
    }
    else {
        // Nothing selected: edit the name in place (a single click already
        // activated the layer, so activating again here would be a no-op).
        m_layerView->edit(layerIdx);
    }
}

void QG_LayerWidget::editLayerColor(RS_Layer* lay) {
    // Cursor-anchored swatch menu built from the authoritative colour combo:
    // one click applies and dismisses. The combo's "Custom" entry (no colour in
    // its item data) falls back to the full picker.
    QG_ColorBox probe(false, false, nullptr);
    QMenu menu(this);
    for (int i = 0; i < probe.count(); ++i) {
        QAction* a = menu.addAction(probe.itemIcon(i), probe.itemText(i));
        a->setData(probe.itemData(i));
    }
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen == nullptr) {
        return;
    }
    RS_Pen pen = lay->getPen();
    QColor c = chosen->data().value<QColor>();
    if (!c.isValid()) {
        c = QColorDialog::getColor(pen.getColor().toQColor(), this);
        if (!c.isValid()) {
            return;
        }
    }
    pen.setColor(RS_Color(c));
    commitLayerPen(lay, pen);
}

void QG_LayerWidget::editLayerWidth(RS_Layer* lay) {
    // Build a cursor-anchored menu from the authoritative width combo list.
    QG_WidthBox probe(false, false, nullptr);
    QMenu menu(this);
    for (int i = 0; i < probe.count(); ++i) {
        QAction* a = menu.addAction(probe.itemIcon(i), probe.itemText(i));
        a->setData(probe.itemData(i));
    }
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen != nullptr) {
        RS_Pen pen = lay->getPen();
        pen.setWidth(static_cast<RS2::LineWidth>(chosen->data().toInt()));
        commitLayerPen(lay, pen);
    }
}

void QG_LayerWidget::editLayerLineType(RS_Layer* lay) {
    QG_LineTypeBox probe(false, false, nullptr);
    QMenu menu(this);
    for (int i = 0; i < probe.count(); ++i) {
        QAction* a = menu.addAction(probe.itemIcon(i), probe.itemText(i));
        a->setData(probe.itemData(i));
    }
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen != nullptr) {
        RS_Pen pen = lay->getPen();
        pen.setLineType(static_cast<RS2::LineType>(chosen->data().toInt()));
        commitLayerPen(lay, pen);
    }
}

void QG_LayerWidget::commitLayerPen(RS_Layer* lay, const RS_Pen& pen) {
    if (lay == nullptr || m_layerList == nullptr) {
        return;
    }
    // Mirror RS_ActionLayersEdit: edit via a clone, refresh ByLayer entities,
    // then redraw. RS_LayerList::edit does *lay = clone; fireEdit(lay).
    RS_Layer* clone = lay->clone();
    clone->setPen(pen);
    m_layerList->edit(lay, *clone);
    delete clone;

    if (m_graphicView != nullptr) {
        RS_EntityContainer* container = m_graphicView->getContainer();
        RS_Graphic* graphic = (container != nullptr) ? container->getGraphic() : nullptr;
        if (graphic != nullptr) {
            for (auto e : *graphic) {
                RS_Layer* l = e->getLayer();
                if (l != nullptr && l->getName() == lay->getName()) {
                    e->update();
                }
            }
        }
        m_graphicView->redraw(RS2::RedrawDrawing);
    }
}


/**
 * Called on layers selection/deselection
 */
void QG_LayerWidget::slotSelectionChanged(
    const QItemSelection &selected,
    const QItemSelection &deselected){
    QModelIndex index;
    QItemSelectionModel *selectionModel {m_layerView->selectionModel()};

    foreach (index, selected.indexes()) {
        auto layer = m_layerModel->getLayer(index.row());
        if (layer) {
            layer->selectedInLayerList(true);
            selectionModel->select(QItemSelection(index, index), QItemSelectionModel::Select);
        }
    }

    foreach (index, deselected.indexes()) {
        auto layer = m_layerModel->getLayer(index.row());
        if (layer && layer->isVisibleInLayerList()) {
            layer->selectedInLayerList(false);
            selectionModel->select(QItemSelection(index, index), QItemSelectionModel::Deselect);
        }
    }
}


/**
 * Called when reg-expresion matchLayerName->text changed
 */
void QG_LayerWidget::slotUpdateLayerList() {
    QRegularExpression rx = QRegularExpression::fromWildcard(m_matchLayerName->text());

    for (unsigned i=0; i<m_layerList->count() ; i++) {
        QString s=m_layerModel->getLayer(i)->getName();
        if (m_matchLayerName->text().isEmpty() || s.indexOf(rx) == 0) {
            m_layerView->showRow(i);
            m_layerModel->getLayer(i)->visibleInLayerList(true);
        } else {
            m_layerView->hideRow(i);
            m_layerModel->getLayer(i)->visibleInLayerList(false);
        }
    }

    restoreSelections();
}

void QG_LayerWidget::addMenuItem(QMenu* contextMenu, RS2::ActionType actionType) {
    auto action = m_actionGroupManager->getActionByType(actionType);
    if (action != nullptr) {
        contextMenu->QWidget::addAction(action);
    }
}

/**
 * Shows a context menu for the layer widget. Launched with a right click.
 */
void QG_LayerWidget::contextMenuEvent(QContextMenuEvent *e) {
    auto menu = new QMenu(this);
    addMenuItem(menu, RS2::ActionLayersDefreezeAll);
    addMenuItem(menu, RS2::ActionLayersFreezeAll);
    addMenuItem(menu, RS2::ActionLayersUnlockAll);
    addMenuItem(menu, RS2::ActionLayersLockAll);
    menu->addSeparator();
    // Actions for selected layers or, if nothing is selected, for active layer:
    addMenuItem(menu, RS2::ActionLayersToggleView);
    addMenuItem(menu, RS2::ActionLayersToggleLock);
    addMenuItem(menu, RS2::ActionLayersTogglePrint);
    addMenuItem(menu, RS2::ActionLayersToggleConstruction);
    addMenuItem(menu, RS2::ActionLayersRemove);
    menu->addSeparator();
    // Single layer actions:
    addMenuItem(menu, RS2::ActionLayersAdd);
    addMenuItem(menu, RS2::ActionLayersEdit);
    menu->addSeparator();
    addMenuItem(menu, RS2::ActionLayersExportSelected);
    addMenuItem(menu, RS2::ActionLayersExportVisible);
    menu->exec(QCursor::pos());
    e->accept();
}

/**
 * Escape releases focus.
 */
void QG_LayerWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {

    case Qt::Key_Escape: {
        emit escape();
        break;
    }
    default:
        QWidget::keyPressEvent(e);
        break;
    }
}

void QG_LayerWidget::activateLayer(int row){
    auto layer = m_layerModel->getLayer(row);
    if (layer) {
        m_layerList->activate(layer, true);
    }
    else
        qWarning("activateLayer: row %d doesn't exist", row);
}

void QG_LayerWidget::updateWidgetSettings(){
    LC_GROUP("Widgets"); {
        bool flatIcons = LC_GET_BOOL("DockWidgetsFlatIcons", true);
        int iconSize = LC_GET_INT("DockWidgetsIconSize", 16);

        QSize size(iconSize, iconSize);

        QList<QToolButton *> widgets = this->findChildren<QToolButton *>();
        foreach(QToolButton *w, widgets) {
            w->setAutoRaise(flatIcons);
            w->setIconSize(size);
        }
    }
    LC_GROUP_END();
}

void QG_LayerWidget::setGraphicView(RS_GraphicView *gview){
    m_graphicView = gview;
    if (gview == nullptr) {
        setLayerList(nullptr, false);
    }
    else {
        auto doc = gview->getContainer();
        bool showByBlock = doc->rtti() == RS2::EntityBlock;
        if (showByBlock) {
            setLayerList(gview->getGraphic(true)->getLayerList(), false);
        }
        else {
            auto layerList = doc->getGraphic()->getLayerList();
            setLayerList(layerList, showByBlock);
        }
    }
}
