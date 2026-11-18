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

#ifndef RS_ACTIONMODIFYTRIM_H
#define RS_ACTIONMODIFYTRIM_H

#include <vector>

#include "lc_overlayboxaction.h"
#include "rs_vector.h"

class RS_AtomicEntity;
/**
 * This action class can handle user events to trim entities.
 *
 * @author Andrew Mustun
 */
class RS_ActionModifyTrim:public LC_OverlayBoxAction {
    Q_OBJECT
public:
    RS_ActionModifyTrim(LC_ActionContext *actionContext,bool both = false);
    ~RS_ActionModifyTrim() override;
    void init(int status) override;
    void finish(bool updateTB) override;
protected:
    /**
     * Action States.
     */
    enum Status {
        ChooseLimitEntity = InitialActionStatus, /**< Choosing the limiting entity. */
        ChooseTrimEntity /**< Choosing the entity to trim. */
    };

    RS_AtomicEntity *m_trimEntity = nullptr;
    RS_Entity *m_limitEntity = nullptr;
    // Cutter-set mode target to trim/split (atomic entity or polyline).
    RS_Entity *m_crossTarget = nullptr;
    // Cutter-set mode box-drag ("lasso"): first corner captured on press, and
    // whether the drag has grown past the click threshold into a window select.
    RS_Vector m_boxCorner1{false};
    RS_Vector m_boxCorner2{false};
    bool m_boxMode = false;
    // AutoCAD-style classic trim: when entities are pre-selected before TRIM is
    // started, they are taken as the cutter set; clicked targets are trimmed
    // against whichever cutter has the nearest valid intersection.
    std::vector<RS_Entity*> m_cutterEntities;
    struct TrimActionData;
    std::unique_ptr<TrimActionData> m_actionData;
    bool m_both = false;

    void doInitWithContextEntity(RS_Entity* contextEntity, const RS_Vector& clickPos) override;
    void previewRefTrimmedEntity(RS_Entity *trimmed, RS_Entity *original);
    RS2::CursorType doGetMouseCursor(int status) override;
    void updateMouseButtonHints() override;
    void onMouseLeftButtonPress(int status, LC_MouseEvent *e) override;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseMoveEvent(int status, LC_MouseEvent *event) override;
    void doTrigger() override;
    // Cutter-set mode: trim every entity caught by the drag box, each cut once at
    // the span nearest the box, all in a single undo step.
    void trimWindow(const RS_Vector& corner1, const RS_Vector& corner2);
    void previewTrim(RS_Entity* entityToTrimCandidate, RS_Entity* limitingEntity, RS_Vector trimCoordinates,
                     RS_Vector limitCoordinates, bool& trimInvalid);
    // True if the entity may be clicked as a trim/split target in cutter-set
    // mode: any atomic entity or polyline (a selected entity is also trimmable).
    bool isCrossingTarget(const RS_Entity* e) const;
    // Cutter-set mode preview: trim/split the hovered target against the whole
    // cutter set and show the resulting pieces.
    void previewCrossing(RS_Entity* target, const RS_Vector& clickPos);

public:
    void keyPressEvent(QKeyEvent* e) override;
};
#endif
