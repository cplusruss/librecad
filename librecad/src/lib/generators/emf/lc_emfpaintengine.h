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

// Windows-only: a minimal QPaintEngine/QPaintDevice pair that records QPainter
// output into a GDI Enhanced Metafile (EMF). LibreCAD's RS_Painter pre-maps all
// geometry to device coordinates with an identity world transform, so the engine
// simply flattens incoming paths/shapes to polygons and emits GDI poly records.
// The resulting HENHMETAFILE is suitable for the Windows clipboard (CF_ENHMETAFILE),
// which Visio / PowerPoint paste as editable vector graphics.

#ifndef LC_EMFPAINTENGINE_H
#define LC_EMFPAINTENGINE_H

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QPaintDevice>
#include <QPaintEngine>
#include <QPen>
#include <QBrush>
#include <QTransform>

class LC_EmfPaintEngine;

/**
 * A QPaintDevice whose paint engine records into a Windows EMF.
 *
 * Logical resolution is fixed at 100 device units per millimetre (i.e. one device
 * unit == 0.01 mm), so device coordinates map 1:1 onto the metafile's HIMETRIC
 * frame. That gives 0.01 mm geometric precision and true physical sizing when the
 * metafile is pasted into Office applications.
 */
class LC_EmfPaintDevice : public QPaintDevice {
public:
    // widthMM/heightMM: physical size of the drawing extent in millimetres.
    LC_EmfPaintDevice(double widthMM, double heightMM);
    ~LC_EmfPaintDevice() override;

    QPaintEngine* paintEngine() const override;

    // Hand over the recorded metafile (HENHMETAFILE as void*). Valid only after the
    // QPainter using this device has been ended. Ownership transfers to the caller.
    void* takeMetafile();

    int widthPx() const { return m_widthPx; }
    int heightPx() const { return m_heightPx; }
    double widthMM() const { return m_widthMM; }
    double heightMM() const { return m_heightMM; }

    static constexpr int UNITS_PER_MM = 100;   // 1 device unit == 0.01 mm
    static constexpr int LOGICAL_DPI = 2540;   // 2540 dpi == 100 units / mm

protected:
    int metric(PaintDeviceMetric metric) const override;

private:
    int m_widthPx;
    int m_heightPx;
    double m_widthMM;
    double m_heightMM;
    LC_EmfPaintEngine* m_engine;
};

/**
 * Translates QPainter primitives into GDI EMF records.
 */
class LC_EmfPaintEngine : public QPaintEngine {
public:
    explicit LC_EmfPaintEngine(LC_EmfPaintDevice* device);
    ~LC_EmfPaintEngine() override;

    bool begin(QPaintDevice* pdev) override;
    bool end() override;
    void updateState(const QPaintEngineState& state) override;

    void drawPath(const QPainterPath& path) override;
    void drawPolygon(const QPointF* points, int count, PolygonDrawMode mode) override;
    void drawPolygon(const QPoint* points, int count, PolygonDrawMode mode) override;
    void drawEllipse(const QRectF& rect) override;
    void drawLines(const QLineF* lines, int count) override;
    void drawLines(const QLine* lines, int count) override;
    void drawRects(const QRectF* rects, int count) override;
    void drawRects(const QRect* rects, int count) override;
    void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
    void drawImage(const QRectF& r, const QImage& image, const QRectF& sr,
                   Qt::ImageConversionFlags flags) override;
    void drawTextItem(const QPointF& p, const QTextItem& textItem) override;

    Type type() const override { return QPaintEngine::User; }

    // Hand over the recorded metafile (HENHMETAFILE as void*); engine keeps nothing.
    void* takeMetafile();

private:
    void emitPolygons(const QList<QPolygonF>& polys, bool fillIt, bool strokeIt,
                      Qt::FillRule rule);
    void selectPen();      // installs a GDI pen matching m_pen, returns previous
    void selectBrush();    // installs a GDI brush matching m_brush
    void syncFromPainter();

    LC_EmfPaintDevice* m_device;
    void* m_hdc;   // HDC of the metafile DC
    void* m_emf;   // HENHMETAFILE produced by end()

    QPen m_pen;
    QBrush m_brush;
    QTransform m_transform;
};

#endif // Q_OS_WIN
#endif // LC_EMFPAINTENGINE_H
