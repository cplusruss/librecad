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
#include "qg_selectionwidget.h"

#include <QTimer>

#include "rs_entitycontainer.h"
#include "rs_graphicview.h"

/*
 *  Constructs a QG_SelectionWidget as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
QG_SelectionWidget::QG_SelectionWidget(QWidget* parent, const char* name, Qt::WindowFlags fl)
    : QWidget(parent, fl){
    setObjectName(name);
    setupUi(this);

    lEntities->setText("0");

    m_auxDataMode = false;

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect( m_timer, &QTimer::timeout, this, &QG_SelectionWidget::removeAuxData);
}

/*
 *  Destroys the object and frees any allocated resources
 */
QG_SelectionWidget::~QG_SelectionWidget(){
    // no need to delete child widgets, Qt does it all for us
    delete m_timer;
}

QToolButton* QG_SelectionWidget::getActionsButton() {
    return tbSelectionActions;
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void QG_SelectionWidget::languageChange(){
    retranslateUi(this);
}

void QG_SelectionWidget::setNumber(int n){
    if (m_auxDataMode)    {
        m_entitiesText.setNum(n);
    }
    else /* if (!auxDataMode) */{
        QString str;
        str.setNum(n);
        lEntities->setText(str);
    }
}

void QG_SelectionWidget::setTotalLength(double l) {
    QString str;
    str.setNum(l, 'g', 6);
    lTotalLength->setText(str);
}


void QG_SelectionWidget::flashAuxData( const QString& header, 
                                       const QString& data, 
                                       const unsigned int& timeout, 
                                       const bool& flash){
    if (flash){
        if (!m_auxDataMode)
        {
            m_auxDataMode = true;

            m_labelLengthMin = lLabelLength->minimumSize();
            m_labelLengthMax = lLabelLength->maximumSize();
            lLabelLength->setMinimumSize(0, 0);
            lLabelLength->setMaximumSize(0, 0);

            m_totalLengthMin = lTotalLength->minimumSize();
            m_totalLengthMax = lTotalLength->maximumSize();
            lTotalLength->setMinimumSize(0, 0);
            lTotalLength->setMaximumSize(0, 0);

            m_labelMinWidth = lLabel->minimumWidth();
            m_labelText = lLabel->text();
            lLabel->setMinimumWidth(235);

            m_entitiesMinWidth = lEntities->minimumWidth();
            m_entitiesText = lEntities->text();
            lEntities->setMinimumWidth(235);
        }

        lLabel->setText(header);

        lEntities->setText(data);

        m_timer->setInterval(timeout);
        m_timer->start();
    }
    else {
        if (m_auxDataMode) {
            m_timer->stop();
            removeAuxData();
        }
    }
}


void QG_SelectionWidget::removeAuxData(){
    m_auxDataMode = false;

    lLabelLength->setMinimumSize(m_labelLengthMin);
    lLabelLength->setMaximumSize(m_labelLengthMax);
    lTotalLength->setMinimumSize(m_totalLengthMin);
    lTotalLength->setMaximumSize(m_totalLengthMax);

    lLabel->setMinimumWidth(m_labelMinWidth);
    lLabel->setText(m_labelText);

    lEntities->setMinimumWidth(m_entitiesMinWidth);
    lEntities->setText(m_entitiesText);
}

void QG_SelectionWidget::setGraphicView(RS_GraphicView* gview) {
    if (gview == nullptr) {
        removeAuxData();
        setNumber(0);
        setTotalLength(0);
    }
    else {
        RS_EntityContainer* container = gview->getContainer();
        const RS_EntityContainer::LC_SelectionInfo &info = container->getSelectionInfo();
        setNumber(info.count);
        setTotalLength(info.length);
    }
}
