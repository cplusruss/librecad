/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2011 Dongxu Li ( dongxuli2011@gmail.com )
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**

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

** This copyright notice MUST APPEAR in all copies of the script!
**
**********************************************************************/

#ifndef RS_ACTIONMODIFYOFFSET_H
#define RS_ACTIONMODIFYOFFSET_H


#include "lc_actionmodifybase.h"

struct RS_OffsetData;

/**
 * This action class create entity by offset
 *
 * @author Dongxu Li
 */
class RS_ActionModifyOffset : public LC_ActionModifyBase {
    Q_OBJECT
public:
    RS_ActionModifyOffset(LC_ActionContext *actionContext);
    ~RS_ActionModifyOffset() override;
    double getDistance();
    void setDistance(double distance);
    bool isFixedDistance() {return m_distanceIsFixed;};
    void setDistanceFixed(bool value);
protected:
    /**
     * Action States. AutoCAD OFFSET order: distance first, then repeated
     * select-object / pick-side pairs until Esc.
     */
    enum Status {
        SetDistance,      /**< Entering the offset distance. */
        SetSide           /**< Picking which side to offset toward. */
    };

    bool m_distanceIsFixed = true;
    /** False until the distance prompt is answered; gates entity selection. */
    bool m_distanceConfirmed = false;
    std::unique_ptr<RS_OffsetData> m_offsetData;

    LC_ActionOptionsWidget* createOptionsWidget() override;
    void init(int status) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool doProcessCommand(int status, const QString& c) override;
    QStringList getAvailableCommands() override;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent *pEvent) override;
    void onMouseLeftButtonReleaseSelected(int status, LC_MouseEvent *pEvent) override;
    void onMouseRightButtonReleaseSelected(int status, LC_MouseEvent *pEvent) override;
    void proceedSelectedEntity(LC_MouseEvent* e) override;
    void updateMouseButtonHintsForSelection() override;
    void updateMouseButtonHintsForSelected(int status) override;
    bool isAllowTriggerOnEmptySelection() override;
    LC_ModifyOperationFlags *getModifyOperationFlags() override;
    void doTrigger(bool keepSelected) override;
    void onMouseMoveEventSelected(int status, LC_MouseEvent *e) override;
    bool doUpdateDistanceByInteractiveInput(const QString& tag, double distance) override;

private:
    /** Distance from the picked entity to coord, for the through-point mode. */
    double distanceToPickedEntity(const RS_Vector& coord) const;
    void confirmDistance(double distance);
    void restartObjectSelection();
};
#endif
