/***************************************************************************
 * This file is part of the LibreCAD project, a 2D CAD program
 *
 * Copyright (C) 2026 LibreCAD.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * **********************************************************************
 */

#include "lc_emfexporter.h"

#ifdef Q_OS_WIN

#include "lc_emfpaintengine.h"
#include "lc_graphicviewport.h"
#include "lc_printviewportrenderer.h"
#include "rs_graphic.h"
#include "rs_painter.h"
#include "rs_units.h"

#include <QColor>
#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
// Transfers ownership of `emf` to the clipboard on success. Frees it on failure.
bool placeOnClipboard(HENHMETAFILE emf) {
    if (emf == nullptr) {
        return false;
    }
    if (!OpenClipboard(nullptr)) {
        DeleteEnhMetaFile(emf);
        return false;
    }
    EmptyClipboard();
    HANDLE handle = SetClipboardData(CF_ENHMETAFILE, emf);
    CloseClipboard();
    if (handle == nullptr) {
        DeleteEnhMetaFile(emf);   // we still owned it
        return false;
    }
    return true;   // clipboard now owns the metafile
}
}

bool LC_EmfExporter::copyGraphicToClipboard(RS_Graphic* graphic, bool blackAndWhite) {
    if (graphic == nullptr) {
        return false;
    }

    graphic->calculateBorders();
    const RS_Vector vmin = graphic->getMin();
    const RS_Vector vmax = graphic->getMax();
    if (!vmin.valid || !vmax.valid) {
        return false;
    }

    const double widthUnits = vmax.x - vmin.x;
    const double heightUnits = vmax.y - vmin.y;
    if (widthUnits <= RS_TOLERANCE && heightUnits <= RS_TOLERANCE) {
        return false;
    }

    double mmPerUnit = RS_Units::convert(1.0, graphic->getUnit(), RS2::Millimeter);
    if (mmPerUnit <= 0.0) {
        mmPerUnit = 1.0;
    }
    const double contentWmm = std::max(widthUnits * mmPerUnit, 0.1);
    const double contentHmm = std::max(heightUnits * mmPerUnit, 0.1);

    // Normalize the pasted physical size: scale so the larger dimension is about
    // TARGET_LARGER_MM. This makes a selection paste at a consistent, slide-friendly
    // size instead of its (often tiny) true millimetre extent.
    constexpr double TARGET_LARGER_MM = 250.0;
    const double scale = TARGET_LARGER_MM / std::max(contentWmm, contentHmm);
    const double frameWmm = contentWmm * scale;
    const double frameHmm = contentHmm * scale;

    // A *proportional* margin keeps the canvas aspect ratio equal to the content
    // aspect ratio, so zoomAuto (which preserves aspect) fills the canvas evenly and
    // leaves only a thin, uniform border instead of a large gap on one axis.
    constexpr double MARGIN_FRACTION = 0.02;   // 2% of each side
    const double totalWmm = frameWmm * (1.0 + 2.0 * MARGIN_FRACTION);
    const double totalHmm = frameHmm * (1.0 + 2.0 * MARGIN_FRACTION);

    LC_EmfPaintDevice device(totalWmm, totalHmm);
    const int widthPx = device.widthPx();
    const int heightPx = device.heightPx();
    const int borderPxX = static_cast<int>(std::lround(widthPx * MARGIN_FRACTION));
    const int borderPxY = static_cast<int>(std::lround(heightPx * MARGIN_FRACTION));

    {
        RS_Painter painter(&device);
        painter.setBackground(Qt::white);

        LC_GraphicViewport viewport;
        viewport.setSize(widthPx, heightPx);
        viewport.setBorders(borderPxX, borderPxY, borderPxX, borderPxY);
        viewport.setContainer(graphic);
        viewport.zoomAuto(false, true);   // keep aspect ratio -> even, tight border
        viewport.loadSettings();

        LC_PrintViewportRenderer renderer(&viewport, &painter);
        renderer.setBackground(Qt::white);
        renderer.loadSettings();
        // The renderer re-applies its drawing mode to the painter in setupPainter(),
        // so the B/W mode must be set on the renderer (not the painter) to stick.
        if (blackAndWhite) {
            renderer.setDrawingMode(RS2::ModeBW);   // force all entities to black
        }
        renderer.render();

        painter.end();
    }

    HENHMETAFILE emf = static_cast<HENHMETAFILE>(device.takeMetafile());
    return placeOnClipboard(emf);
}

#else  // !Q_OS_WIN

bool LC_EmfExporter::copyGraphicToClipboard(RS_Graphic*) {
    return false;
}

#endif // Q_OS_WIN
