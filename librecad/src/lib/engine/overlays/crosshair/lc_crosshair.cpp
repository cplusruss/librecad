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

#include "lc_crosshair.h"

#include "lc_graphicviewport.h"
#include "rs_painter.h"

LC_Crosshair::LC_Crosshair(const RS_Vector& coord,
                           int shapeType,
                           int linesType,
                           const RS_Pen& linesPen,
                           int pointSize,
                           int pointType)
     :wcsPos(coord){
     this->indicatorShape = shapeType;
     this->linesShape = linesType;
     this->linesPen = linesPen;
     this->pointSize = pointSize;
     this->pointType = pointType;
}

double LC_Crosshair::drawIndicator(RS_Painter* painter, const RS_Vector& uiPos)
{
    double offset = 0.0;

    // AutoCAD-style: when the snapper told us WHAT kind of snap fired
    // (Endpoint, Center, etc.), draw a glyph specific to that snap type
    // instead of the generic user-set shape. snapTypeHint == -2 (legacy
    // sentinel) and snapTypeHint == -1 (FREE — cursor wasn't snapped to
    // anything) both fall through to the static indicatorShape behaviour.
    if (snapTypeHint >= 0 && indicatorShape != NoShape) {
        const double autoOffset = drawAutoSnapMarker(painter, uiPos, snapTypeHint);
        if (autoOffset > 0.0) return autoOffset;
        // 0.0 means the type has no custom glyph — drop to the static shape.
    }

      switch (indicatorShape) {
          case Circle: {
              offset = 4.0;
              painter->drawCircleUIDirect(uiPos, offset);
              break;
          }
          case Point:{
              int screenPDSize = painter->determinePointScreenSize(pointSize);
              offset = screenPDSize;
              painter->drawPointEntityUI(uiPos, pointType, screenPDSize);
              break;
          }
          case Square: {
              double a = 6.0;
              offset = a;
              RS_Vector p1 = uiPos + RS_Vector(-a, a);
              RS_Vector p2 = uiPos + RS_Vector(a, a);
              RS_Vector p3 = uiPos + RS_Vector(a, -a);
              RS_Vector p4 = uiPos + RS_Vector(-a, -a);

              painter->drawLineUISimple(p1,p2);
              painter->drawLineUISimple(p2,p3);
              painter->drawLineUISimple(p3,p4);
              painter->drawLineUISimple(p4,p1);
              break;
          }
          case Gap:{
              offset = 5.0;
              break;
          }
          default:
              break;
    }
    return offset;
}

double LC_Crosshair::drawAutoSnapMarker(RS_Painter* painter, const RS_Vector& uiPos, int type)
{
    // Marker half-size. Sized AutoCAD-style so the box is visible at a glance
    // ("giant" marker per user request). Returned as offset so the
    // crosshair-line gap clears the glyph cleanly.
    constexpr double a = 14.0;

    // Snap-type integers mirror RS_Snapper's internal SnapType enum:
    //   0 GRID, 1 ENTITY (on-entity / nearest), 2 ENDPOINT,
    //   3 INTERSECTION, 4 MIDDLE, 5 DISTANCE, 6 CENTER,
    //   7-9 ANGLE variants
    switch (type) {
        case 2: {  // ENDPOINT — hollow square
            RS_Vector p1 = uiPos + RS_Vector(-a,  a);
            RS_Vector p2 = uiPos + RS_Vector( a,  a);
            RS_Vector p3 = uiPos + RS_Vector( a, -a);
            RS_Vector p4 = uiPos + RS_Vector(-a, -a);
            painter->drawLineUISimple(p1, p2);
            painter->drawLineUISimple(p2, p3);
            painter->drawLineUISimple(p3, p4);
            painter->drawLineUISimple(p4, p1);
            return a;
        }
        case 4: {  // MIDDLE — equilateral triangle, point up
            RS_Vector apex   = uiPos + RS_Vector(0.0, -a);
            RS_Vector left   = uiPos + RS_Vector(-a,   a * 0.7);
            RS_Vector right  = uiPos + RS_Vector( a,   a * 0.7);
            painter->drawLineUISimple(apex,  left);
            painter->drawLineUISimple(left,  right);
            painter->drawLineUISimple(right, apex);
            return a;
        }
        case 6: {  // CENTER — small circle
            painter->drawCircleUIDirect(uiPos, a * 0.7);
            return a * 0.7;
        }
        case 3: {  // INTERSECTION — X
            RS_Vector tl = uiPos + RS_Vector(-a,  a);
            RS_Vector tr = uiPos + RS_Vector( a,  a);
            RS_Vector bl = uiPos + RS_Vector(-a, -a);
            RS_Vector br = uiPos + RS_Vector( a, -a);
            painter->drawLineUISimple(tl, br);
            painter->drawLineUISimple(tr, bl);
            return a;
        }
        case 1: {  // ENTITY (on-entity / nearest) — hourglass / bowtie
            RS_Vector tl = uiPos + RS_Vector(-a,  a);
            RS_Vector tr = uiPos + RS_Vector( a,  a);
            RS_Vector bl = uiPos + RS_Vector(-a, -a);
            RS_Vector br = uiPos + RS_Vector( a, -a);
            painter->drawLineUISimple(tl, tr);  // top
            painter->drawLineUISimple(tr, bl);  // diagonal
            painter->drawLineUISimple(bl, br);  // bottom
            painter->drawLineUISimple(br, tl);  // diagonal
            return a;
        }
        case 0: {  // GRID — plus
            painter->drawLineUISimple(uiPos + RS_Vector(-a, 0),  uiPos + RS_Vector(a, 0));
            painter->drawLineUISimple(uiPos + RS_Vector(0, -a),  uiPos + RS_Vector(0, a));
            return a * 0.85;
        }
        case 5: {  // DISTANCE — square with a tick mark above
            RS_Vector p1 = uiPos + RS_Vector(-a,  a);
            RS_Vector p2 = uiPos + RS_Vector( a,  a);
            RS_Vector p3 = uiPos + RS_Vector( a, -a);
            RS_Vector p4 = uiPos + RS_Vector(-a, -a);
            painter->drawLineUISimple(p1, p2);
            painter->drawLineUISimple(p2, p3);
            painter->drawLineUISimple(p3, p4);
            painter->drawLineUISimple(p4, p1);
            painter->drawLineUISimple(uiPos + RS_Vector(0, -a),
                                     uiPos + RS_Vector(0, -a * 1.8));
            return a;
        }
        case 7: case 8: case 9: {  // ANGLE / ANGLE_REL / ANGLE_ON_ENTITY — wedge
            // Small open arc — quarter circle hint.
            const double r = a * 0.9;
            RS_Vector right  = uiPos + RS_Vector(r, 0);
            RS_Vector top    = uiPos + RS_Vector(0, -r);
            painter->drawLineUISimple(uiPos, right);
            painter->drawLineUISimple(uiPos, top);
            painter->drawCircleUIDirect(uiPos, r);
            return r;
        }
        default:
            return 0.0;  // unknown type → caller falls back to static shape
    }
}

void LC_Crosshair::draw(RS_Painter *painter) {
    RS_Vector uiCoord = painter->toGui(wcsPos);

    if (indicatorShape != NoShape) {
        painter->setPen(shapePen);
    }

    double offset = drawIndicator(painter, uiCoord);

    LC_GraphicViewport* viewport = painter->getViewPort();

    int width = viewport->getWidth();
    int height = viewport->getHeight();

    painter->setPen(linesPen);

    switch (linesShape){
        case Spiderweb:{
            RS_Vector p1(0, 0);
            RS_Vector p2(0, height);
            RS_Vector p3(width, 0);
            RS_Vector p4(width, height);
            drawCrosshairLines(painter, uiCoord, offset, p1, p2, p3, p4);
            break;
        }
        case Adaptive:{
            if (viewport->isGridIsometric()){
                RS2::IsoGridViewType chType= viewport->getIsoViewType();
                RS_Vector direction1;
                RS_Vector direction2(0.,1.);
                double l=width+height;
                switch(chType){
                case RS2::IsoRight:
                    direction1=RS_Vector(M_PI*5./6.)*l;
                    direction2*=l;
                    break;
                case RS2::IsoLeft:
                    direction1=RS_Vector(M_PI*1./6.)*l;
                    direction2*=l;
                    break;
                default:
                    direction1=RS_Vector(M_PI*1./6.)*l;
                    direction2=RS_Vector(M_PI*5./6.)*l;
                }
                RS_Vector p1 = uiCoord - direction1;
                RS_Vector p2 = uiCoord + direction1;
                RS_Vector p3 = uiCoord - direction2;
                RS_Vector p4 = uiCoord + direction2;
                drawCrosshairLines(painter, uiCoord, offset, p1, p2, p3, p4);
                break;
            }
            // implicit fall-through to draw cartesian orthogonal crosshair
            [[fallthrough]];
        }
        case Crosshair:{
            RS_Vector p1(0, uiCoord.y);
            RS_Vector p2(uiCoord.x, 0);
            RS_Vector p3(uiCoord.x, height);
            RS_Vector p4(width, uiCoord.y);
            drawCrosshairLines(painter, uiCoord, offset, p1, p2, p3, p4);
            break;
        }
        case Infinite:{
            // Full-viewport horizontal + vertical lines passing through the cursor
            // with no gap around the indicator — AutoCAD-style infinite crosshair.
            RS_Vector hLeft(0, uiCoord.y);
            RS_Vector hRight(width, uiCoord.y);
            RS_Vector vTop(uiCoord.x, 0);
            RS_Vector vBottom(uiCoord.x, height);
            painter->drawLineUISimple(hLeft, hRight);
            painter->drawLineUISimple(vTop, vBottom);
            break;
        }
        default:
            break;
    }
}

void LC_Crosshair::setLinesPen(const RS_Pen &pen) {
    linesPen = pen;
}

void LC_Crosshair::setPointType(int type) {
    pointType = type;
}

void LC_Crosshair::setPointSize(int size) {
    pointSize = size;
}

void LC_Crosshair::drawCrosshairLines(RS_Painter *painter, const RS_Vector &guiCoord, double offset,
                                      const RS_Vector &p1, const RS_Vector &p2, const RS_Vector &p3, const RS_Vector &p4) const {
    RS_Vector offset1 = RS_Vector::polar(offset, guiCoord.angleTo(p1));
    RS_Vector offset2 = RS_Vector::polar(offset, guiCoord.angleTo(p2));
    RS_Vector offset3 = RS_Vector::polar(offset, guiCoord.angleTo(p3));
    RS_Vector offset4 = RS_Vector::polar(offset, guiCoord.angleTo(p4));

    painter->drawLineUISimple(guiCoord + offset1, p1);
    painter->drawLineUISimple(guiCoord + offset2 , p2);
    painter->drawLineUISimple(guiCoord + offset3, p3);
    painter->drawLineUISimple(guiCoord + offset4, p4);
}
