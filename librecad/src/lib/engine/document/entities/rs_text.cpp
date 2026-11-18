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

#include<iostream>
#include<cmath>

#include <QChar>

#include "rs_text.h"

#include "rs_arc.h"
#include "rs_debug.h"
#include "rs_font.h"
#include "rs_fontlist.h"
#include "rs_insert.h"
#include "rs_line.h"
#include "rs_math.h"
#include "rs_mtext.h"
#include "rs_painter.h"
#include "rs_pen.h"
#include "rs_polyline.h"

namespace {

// A fresh, invisible-pen polyline parented to the text. Strokes are emitted as
// RS_Polyline (a container that deep-clones itself) rather than loose lines, so the
// text still clones safely for highlight/preview/undo.
RS_Polyline* newFlatPoly(RS_EntityContainer* parent) {
    auto* poly = new RS_Polyline(parent, RS_PolylineData());
    poly->setPen(RS_Pen(RS2::FlagInvalid));
    poly->setLayer(nullptr);
    return poly;
}

// Append an arc, tessellated to ~10 deg facets, as vertices of poly. When
// includeStart is set the arc's start point is added first (otherwise it is assumed
// already present as the previous segment's end).
void addArcVertices(RS_Polyline* poly, RS_Arc* arc, bool includeStart) {
    const double a1 = arc->getAngle1();
    const RS_Vector c = arc->getCenter();
    const double r = arc->getRadius();
    const bool rev = arc->isReversed();
    const double aLen = arc->getAngleLength();
    int n = static_cast<int>(std::ceil(aLen / (M_PI / 18.0)));
    if (n < 2) {
        n = 2;
    }
    if (includeStart) {
        poly->addVertex(arc->getStartpoint(), 0.0);
    }
    for (int i = 1; i <= n; i++) {
        const double t = aLen * i / n;
        const double a = rev ? a1 - t : a1 + t;
        const RS_Vector p = (i == n) ? arc->getEndpoint() : c + RS_Vector::polar(r, a);
        poly->addVertex(p, 0.0);
    }
}

// Flatten src's geometry into straight RS_Polyline strokes appended to out (each
// parented to parent). Arcs (standalone or as polyline segments) become line facets;
// containers (glyph inserts, composed letters) are recursed into.
void appendFlattenedStrokes(std::vector<RS_Entity*>& out, RS_EntityContainer* parent, RS_Entity* src) {
    if (src == nullptr) {
        return;
    }
    switch (src->rtti()) {
        case RS2::EntityLine: {
            auto* poly = newFlatPoly(parent);
            poly->addVertex(src->getStartpoint(), 0.0);
            poly->addVertex(src->getEndpoint(), 0.0);
            out.push_back(poly);
            break;
        }
        case RS2::EntityArc: {
            auto* poly = newFlatPoly(parent);
            addArcVertices(poly, static_cast<RS_Arc*>(src), true);
            out.push_back(poly);
            break;
        }
        case RS2::EntityPolyline: {
            auto* poly = newFlatPoly(parent);
            bool first = true;
            for (RS_Entity* seg : *static_cast<RS_EntityContainer*>(src)) {
                if (seg == nullptr) {
                    continue;
                }
                if (seg->rtti() == RS2::EntityArc) {
                    addArcVertices(poly, static_cast<RS_Arc*>(seg), first);
                } else {
                    if (first) {
                        poly->addVertex(seg->getStartpoint(), 0.0);
                    }
                    poly->addVertex(seg->getEndpoint(), 0.0);
                }
                first = false;
            }
            out.push_back(poly);
            break;
        }
        default: {
            if (src->isContainer()) {
                for (RS_Entity* child : *static_cast<RS_EntityContainer*>(src)) {
                    appendFlattenedStrokes(out, parent, child);
                }
            }
            break;
        }
    }
}

/**
 * Resolve the UAX#9 base direction for an RS_Text update pass. Honors an
 * explicit @c LeftToRight / @c RightToLeft setting when set; for @c ByContent
 * scans the string for the first strong-directional character and uses that
 * (UAX#9 P-rules), defaulting to LTR when there is no strong character.
 */
Qt::LayoutDirection
resolveTextBaseDirection(const QString &text,
                         RS_TextData::DrawingDirection setting) {
  if (setting == RS_TextData::LeftToRight)
    return Qt::LeftToRight;
  if (setting == RS_TextData::RightToLeft)
    return Qt::RightToLeft;
  for (int i = 0; i < text.size(); ++i) {
    const QChar::Direction d = text.at(i).direction();
    if (d == QChar::DirL)
      return Qt::LeftToRight;
    if (d == QChar::DirR || d == QChar::DirAL)
      return Qt::RightToLeft;
  }
  return Qt::LeftToRight;
}
} // namespace

class RS_Font;

RS_TextData::RS_TextData(const RS_Vector& _insertionPoint,
                         const RS_Vector& _secondPoint,
                         double _height,
                         double _widthRel,
                         VAlign _valign,
                         HAlign _halign,
                         TextGeneration _textGeneration,
                         const QString& _text,
                         const QString& _style,
                         double _angle,
                         RS2::UpdateMode _updateMode):
	insertionPoint(_insertionPoint)
  ,secondPoint(_secondPoint)
  ,height(_height)
  ,widthRel(_widthRel)
  ,valign(_valign)
  ,halign(_halign)
  ,textGeneration(_textGeneration)
  ,text(_text)
  ,style(_style)
  ,angle(_angle)
  ,updateMode(_updateMode){
}

std::ostream& operator << (std::ostream& os, const RS_TextData& td) {
    os << "("
       <<td.insertionPoint<<','
       <<td.secondPoint<<','
       <<td.height<<','
       <<td.widthRel<<','
       <<td.valign<<','
       <<td.halign<<','
       <<td.textGeneration<<','
       <<td.text.toLatin1().data() <<','
       <<td.style.toLatin1().data()<<','
       <<td.angle<<','
       <<td.updateMode<<','
       <<")";
    return os;
}

/**
 * Constructor.
 */
RS_Text::RS_Text(RS_EntityContainer* parent,
                 const RS_TextData& d)
        : RS_EntityContainer(parent), data(d) {
    usedTextHeight = 0.0;
    usedTextWidth = 0.0;
    setText(data.text);
}

RS_Entity* RS_Text::clone() const{
    auto t = new RS_Text(*this);
    t->setOwner(isOwner());
    t->detach();
    return t;
}

/**
 * Sets a new text. The entities representing the
 * text are updated.
 */
void RS_Text::setText(const QString& t) {
    data.text = t;

    // handle some special flags embedded in the text:
    if (data.text.left(4)=="\\A0;") {
        data.text = data.text.mid(4);
        data.valign = RS_TextData::VABottom;
    } else if (data.text.left(4)=="\\A1;") {
        data.text = data.text.mid(4);
        data.valign = RS_TextData::VAMiddle;
    } else if (data.text.left(4)=="\\A2;") {
        data.text = data.text.mid(4);
        data.valign = RS_TextData::VATop;
    }
    if (data.updateMode==RS2::Update) {
        update();
        //calculateBorders();
    }
}

void RS_Text::setDrawingDirection(RS_TextData::DrawingDirection direction) {
  if (data.drawingDirection == direction)
    return;
  data.drawingDirection = direction;
  if (data.updateMode == RS2::Update) {
    update();
  }
}

/**
 * Gets the alignment as an int.
 *
 * @return  1: top left ... 9: bottom right
 */
//RLZ: bad function, this is MText style align
int RS_Text::getAlignment() {
    if (data.valign==RS_TextData::VATop) {
        if (data.halign==RS_TextData::HALeft) {
            return 1;
        } else if (data.halign==RS_TextData::HACenter) {
            return 2;
        } else if (data.halign==RS_TextData::HARight) {
            return 3;
        }
    } else if (data.valign==RS_TextData::VAMiddle) {
        if (data.halign==RS_TextData::HALeft) {
            return 4;
        } else if (data.halign==RS_TextData::HACenter) {
            return 5;
        } else if (data.halign==RS_TextData::HARight) {
            return 6;
        }
    } else if (data.valign==RS_TextData::VABaseline) {
        if (data.halign==RS_TextData::HALeft) {
            return 7;
        } else if (data.halign==RS_TextData::HACenter) {
            return 8;
        } else if (data.halign==RS_TextData::HARight) {
            return 9;
        }
    } else if (data.valign==RS_TextData::VABottom) {
        if (data.halign==RS_TextData::HALeft) {
            return 10;
        } else if (data.halign==RS_TextData::HACenter) {
            return 11;
        } else if (data.halign==RS_TextData::HARight) {
            return 12;
        }
    }
    if (data.halign==RS_TextData::HAFit) {
        return 13;
    } else if (data.halign==RS_TextData::HAAligned) {
        return 14;
    } else if (data.halign==RS_TextData::HAMiddle) {
        return 15;
    }
    return 1;
}

/**
 * Sets the alignment from an int.
 *
 * @param a 1: top left ... 9: bottom right
 */
//RLZ: bad function, this is MText style align
void RS_Text::setAlignment(int a) {
    switch (a%3) {
        default:
        case 1:
            data.halign = RS_TextData::HALeft;
            break;
        case 2:
            data.halign = RS_TextData::HACenter;
            break;
        case 0:
            data.halign = RS_TextData::HARight;
            break;
    }

    switch ((int)ceil(a/3.0)) {
        default:
        case 1:
            data.valign = RS_TextData::VATop;
            break;
        case 2:
            data.valign = RS_TextData::VAMiddle;
            break;
        case 3:
            data.valign = RS_TextData::VABaseline;
            break;
        case 4:
            data.valign = RS_TextData::VABottom;
            break;
    }
    if (a > 12) {
        data.valign = RS_TextData::VABaseline;
        if (a == 13) {
            data.halign = RS_TextData::HAFit;
        } else if (a == 14) {
            data.halign = RS_TextData::HAAligned;
        } else if (a == 15) {
            data.halign = RS_TextData::HAMiddle;
        }
    }
    if (data.updateMode == RS2::Update) {
      update();
    }
}

/**
 * @return Number of lines in this text entity.
 */
int RS_Text::getNumberOfLines() {
    int c=1;
    for (int i=0; i<(int)data.text.length(); ++i) {
        if (data.text.at(i).unicode()==0x0A) {
            c++;
        }
    }
    return c;
}

/**
 * Updates the Inserts (letters) of this text. Called when the
 * text or it's data, position, alignment, .. changes.
 * This method also updates the usedTextWidth / usedTextHeight property.
 */
void RS_Text::update() {

    RS_DEBUG->print("RS_Text::update");

    clear();

    if (isUndone()) {
        return;
    }

    usedTextWidth = 0.0;
    usedTextHeight = 0.0;

    RS_Font* font = RS_FONTLIST->requestFont(data.style);

    if (font==nullptr) {
        return;
    }

    RS_Vector letterPos = RS_Vector(0.0, -9.0);
    RS_Vector letterSpace = RS_Vector(font->getLetterSpacing(), 0.0);
    RS_Vector space = RS_Vector(font->getWordSpacing(), 0.0);

    // First every text line is created with
    //   alignment: top left
    //   angle: 0
    //   height: 9.0
    // Rotation, scaling and centering is done later

    // Visual ordering depends on the drawingDirection setting:
    //   * RightToLeft: pure positional reversal — matches AutoCAD semantics
    //     and the editor mirror. UAX#9 alone leaves EN digits direction-
    //     immune, so widget and canvas would diverge for "1234" otherwise.
    //   * ByContent (and the other settings): UAX#9 with first-strong base
    //     detection so embedded strong-RTL runs (Hebrew/Arabic) still
    //     display in correct visual order.
    std::vector<int> visual;
    if (data.drawingDirection == RS_TextData::RightToLeft) {
      visual.resize(data.text.size());
      for (int i = 0; i < data.text.size(); ++i) {
        visual[i] = data.text.size() - 1 - i;
      }
    } else {
      const Qt::LayoutDirection baseDir =
          resolveTextBaseDirection(data.text, data.drawingDirection);
      visual = RS_MText::computeBidiVisualOrder(data.text, baseDir);
    }

    for (int logIdx : visual) {
      const QChar ch = data.text.at(logIdx);
      // Space:
      if (ch.unicode() == 0x20) {
        letterPos += space;
      } else {
        // One Letter:
        QString letterText = QString(ch);
        if (font->findLetter(letterText) == nullptr) {
          RS_DEBUG->print("RS_Text::update: missing font for letter( %s ), "
                          "replaced it with QChar(0xfffd)",
                          qPrintable(letterText));
          letterText = QChar(0xfffd);
        }
        RS_DEBUG->print("RS_Text::update: insert a "
                        "letter at pos: %f/%f",
                        letterPos.x, letterPos.y);

        RS_InsertData d(letterText, letterPos, RS_Vector(1.0, 1.0), 0.0, 1, 1,
                        RS_Vector(0.0, 0.0), font->getLetterList(),
                        RS2::NoUpdate);

        auto *letter = new RS_Insert(this, d);
        RS_Vector letterWidth;
        letter->setPen(RS_Pen(RS2::FlagInvalid));
        letter->setLayer(nullptr);
        letter->update();
        letter->forcedCalculateBorders();

        letterWidth = RS_Vector(letter->getMax().x - letterPos.x, 0.0);
        if (letterWidth.x < 0) {
          letterWidth.x = -letterSpace.x;
        }
        addEntity(letter);

        // next letter position:
        letterPos += letterWidth;
        letterPos += letterSpace;
      }
    }

    if (!getAutoUpdateBorders()) {
        //only update borders when needed
        forcedCalculateBorders();
    }
    RS_Vector textSize = getSize();

    RS_DEBUG->print("RS_Text::updateAddLine: width 2: %f", textSize.x);

    // Vertical Align:
    double vSize = 9.0;
    //HAAligned, HAFit, HAMiddle require VABaseline
    if (data.halign == RS_TextData::HAAligned
            || data.halign == RS_TextData::HAFit
            || data.halign == RS_TextData::HAMiddle) {
        data.valign = RS_TextData::VABaseline;
    }
    RS_Vector offset(0.0, 0.0);
    switch (data.valign) {
        case RS_TextData::VAMiddle: {
            offset.move(RS_Vector(0.0, vSize/2.0));
            break;
        }
        case RS_TextData::VABottom: {
            offset.move(RS_Vector(0.0, vSize+3));
            break;
        }
        case RS_TextData::VABaseline: {
            offset.move(RS_Vector(0.0, vSize));
            break;
        }
        default:
            break;
    }

    // Horizontal Align:
    switch (data.halign) {
        case RS_TextData::HAMiddle:{
            offset.move(RS_Vector(-textSize.x/2.0, -(vSize + textSize.y/2.0 + getMin().y) ));
            break;
        }
        case RS_TextData::HACenter: {
            RS_DEBUG->print("RS_Text::updateAddLine: move by: %f", -textSize.x / 2.0);
            offset.move(RS_Vector(-textSize.x / 2.0, 0.0));
            break;
        }
        case RS_TextData::HARight: {
            offset.move(RS_Vector(-textSize.x, 0.0));
            break;
        }
        default:
            break;
    }

    if (data.halign!=RS_TextData::HAAligned && data.halign!=RS_TextData::HAFit){
        data.secondPoint = RS_Vector(offset.x, offset.y - vSize);
    }
    RS_EntityContainer::move(offset);


    // Scale:
    if (data.halign==RS_TextData::HAAligned){
        double dist = data.insertionPoint.distanceTo(data.secondPoint)/textSize.x;
        data.height = vSize*dist;
        RS_EntityContainer::scale(RS_Vector(0.0,0.0),
                                  RS_Vector(dist, dist));
    } else if (data.halign==RS_TextData::HAFit){
        double dist = data.insertionPoint.distanceTo(data.secondPoint)/textSize.x;
        double sy = data.height/9.0;
        // A non-uniform scale turns glyph arcs into ellipses (RS_Arc can't represent
        // that), so flatten the curved strokes to line segments first.
        if (std::abs(dist - sy) > RS_TOLERANCE) {
            flattenGlyphArcs();
        }
        RS_EntityContainer::scale(RS_Vector(0.0,0.0), RS_Vector(dist, sy));
    } else {
        double sx = data.height*data.widthRel/9.0;
        double sy = data.height/9.0;
        // Width factor != 1 gives a non-uniform scale that would distort glyph arcs
        // into overlapping circles; flatten curved strokes to lines first so the
        // stretch only affects straight segments.
        if (std::abs(sx - sy) > RS_TOLERANCE) {
            flattenGlyphArcs();
        }
        RS_EntityContainer::scale(RS_Vector(0.0,0.0), RS_Vector(sx, sy));
        data.secondPoint.scale(RS_Vector(0.0,0.0), RS_Vector(sx, sy));
    }

    forcedCalculateBorders();

    // Update actual text size (before rotating, after scaling!):
    usedTextWidth = getSize().x;
    usedTextHeight = data.height;

    // Rotate:
    if (data.halign==RS_TextData::HAAligned || data.halign==RS_TextData::HAFit){
        double angle = data.insertionPoint.angleTo(data.secondPoint);
        data.angle =  angle;
    } else {
        data.secondPoint.rotate(RS_Vector(0.0,0.0), data.angle);
        data.secondPoint.move(data.insertionPoint);
    }
    RS_EntityContainer::rotate(RS_Vector(0.0,0.0), data.angle);

    // Move to insertion point:
    RS_EntityContainer::move(data.insertionPoint);

    updateBaselinePoints();

    forcedCalculateBorders();

    RS_DEBUG->print("RS_Text::update: OK");
}

void RS_Text::updateBaselinePoints() {
    RS_Vector shiftVector = RS_Vector::polar(usedTextHeight / 2, data.angle + M_PI_2);
    baselineStartPoint = data.secondPoint + shiftVector;
    baselineEndPoint = baselineStartPoint + RS_Vector::polar(usedTextWidth, data.angle);
}

/**
 * Replaces the glyph letters (still at unit scale, arcs intact) with flattened
 * line-segment geometry. Called just before a non-uniform scale so glyph curves
 * stretch correctly instead of breaking into circles/loops. Only invoked for
 * stretched text (width factor != 1 or a fit that isn't square).
 */
void RS_Text::flattenGlyphArcs() {
    // Build flattened stroke polylines from the current letters (still at unit scale,
    // arcs intact) before clearing, then swap them in.
    std::vector<RS_Entity*> strokes;
    for (RS_Entity* letter : *this) {
        appendFlattenedStrokes(strokes, this, letter);
    }
    clear();
    for (RS_Entity* s : strokes) {
        addEntity(s);
    }
}

RS_Vector RS_Text::getNearestEndpoint(const RS_Vector& coord, double* dist)const {
    if (dist != nullptr) {
        *dist = data.insertionPoint.distanceTo(coord);
    }
    return data.insertionPoint;
}

RS_VectorSolutions RS_Text::getRefPoints() const{
    RS_VectorSolutions ret({data.insertionPoint, data.secondPoint});
    return ret;
}

RS_Vector RS_Text::getNearestRef(const RS_Vector &coord, double *dist) const {
    return RS_Entity::getNearestRef(coord, dist);
}

RS_Vector RS_Text::getNearestSelectedRef(const RS_Vector &coord, double *dist) const {
    return RS_Entity::getNearestSelectedRef(coord, dist);
}

void RS_Text::moveRef([[maybe_unused]]const RS_Vector &ref, const RS_Vector &offset) {
    move(offset);
}

void RS_Text::moveSelectedRef([[maybe_unused]]const RS_Vector &ref, const RS_Vector &offset) {
//    RS_EntityContainer::moveSelectedRef(ref, offset);
    move(offset);
}

void RS_Text::move(const RS_Vector& offset) {
    RS_EntityContainer::move(offset);
    data.insertionPoint.move(offset);
    data.secondPoint.move(offset);
    updateBaselinePoints();
    forcedCalculateBorders();
//    update();
}

void RS_Text::rotate(const RS_Vector& center, double angle) {
    RS_Vector angleVector(angle);
    RS_EntityContainer::rotate(center, angleVector);
    data.insertionPoint.rotate(center, angleVector);
    data.secondPoint.rotate(center, angleVector);
    data.angle = RS_Math::correctAngle(data.angle+angle);
    updateBaselinePoints();
//    update();
}
void RS_Text::rotate(const RS_Vector& center, const RS_Vector& angleVector) {
    RS_EntityContainer::rotate(center, angleVector);
    data.insertionPoint.rotate(center, angleVector);
    data.secondPoint.rotate(center, angleVector);
    data.angle = RS_Math::correctAngle(data.angle+angleVector.angle());
    updateBaselinePoints();
//    update();
}

void RS_Text::scale(const RS_Vector& center, const RS_Vector& factor) {
    data.insertionPoint.scale(center, factor);
    data.secondPoint.scale(center, factor);
    data.height*=factor.x;
    update();
}

void RS_Text::mirror(const RS_Vector& axisPoint1, const RS_Vector& axisPoint2) {
    bool readable = RS_Math::isAngleReadable(data.angle);

    RS_Vector vec = RS_Vector::polar(1.0, data.angle);
    vec.mirror(RS_Vector(0.0,0.0), axisPoint2-axisPoint1);
    data.angle = vec.angle();

    bool corr = false;
    data.angle = RS_Math::makeAngleReadable(data.angle, readable, &corr);

    if (corr) {
        data.insertionPoint.mirror(axisPoint1, axisPoint2);
        data.secondPoint.mirror(axisPoint1, axisPoint2);
        if (data.halign==RS_TextData::HALeft) {
            data.halign=RS_TextData::HARight;
        } else if (data.halign==RS_TextData::HARight) {
            data.halign=RS_TextData::HALeft;
        } else if (data.halign==RS_TextData::HAFit || data.halign==RS_TextData::HAAligned) {
            RS_Vector tmp = data.insertionPoint;
            data.insertionPoint = data.secondPoint;
            data.secondPoint = tmp;
        }
    } else {
        RS_Vector minP{getMin().x, getMax().y};
        minP = minP.mirror(axisPoint1, axisPoint2);
        double mirrAngle = axisPoint1.angleTo(axisPoint2)*2.0;
        data.insertionPoint.move(minP - getMin());
        data.secondPoint.move(minP - getMin());
        data.insertionPoint.rotate(minP, mirrAngle);
        data.secondPoint.rotate(minP, mirrAngle);
    }
    update();
}

bool RS_Text::hasEndpointsWithinWindow(const RS_Vector& /*v1*/, const RS_Vector& /*v2*/) const
{
    return false;
}

/**
 * Implementations must stretch the given range of the entity
 * by the given offset.
 */
void RS_Text::stretch(const RS_Vector& firstCorner, const RS_Vector& secondCorner, const RS_Vector& offset) {
    if (getMin().isInWindow(firstCorner, secondCorner) &&
        getMax().isInWindow(firstCorner, secondCorner)) {
        move(offset);
    }
}

/**
 * Dumps the point's data to stdout.
 */
std::ostream& operator << (std::ostream& os, const RS_Text& p) {
    os << " Text: " << p.getData() << "\n";
    return os;
}

RS_Entity *RS_Text::cloneProxy() const {
   return new RS_Line(nullptr, baselineStartPoint, baselineEndPoint);
}

void RS_Text::drawDraft(RS_Painter *painter) {
    painter->drawLineWCS(baselineStartPoint, baselineEndPoint);
}

void RS_Text::draw(RS_Painter* painter){
    bool drawAsDraft = painter->isTextLineNotRenderable(getHeight());
    if (drawAsDraft){
        drawDraft(painter);
        return;
    }

    for(RS_Entity* e: *this){
       painter->drawAsChild(e);
    }
}
