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

#include "lc_emfpaintengine.h"

#ifdef Q_OS_WIN

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QTextItem>
#include <QVarLengthArray>
#include <vector>
#include <string>
#include <climits>
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
inline LONG toGdi(double v) { return static_cast<LONG>(std::lround(v)); }
inline COLORREF toColorRef(const QColor& c) { return RGB(c.red(), c.green(), c.blue()); }

// All supported QPainter features are advertised so QPainter routes high-level
// drawing through our overrides (with logical coords + current transform) instead
// of rasterising. Unsupported effects degrade to solid colour but stay vector.
constexpr QPaintEngine::PaintEngineFeatures kFeatures = QPaintEngine::AllFeatures;
}

// ---------------------------------------------------------------------------
// LC_EmfPaintDevice
// ---------------------------------------------------------------------------

LC_EmfPaintDevice::LC_EmfPaintDevice(double widthMM, double heightMM)
    : m_widthMM(std::max(widthMM, 0.1))
    , m_heightMM(std::max(heightMM, 0.1))
    , m_engine(new LC_EmfPaintEngine(this)) {
    m_widthPx = static_cast<int>(std::lround(m_widthMM * UNITS_PER_MM));
    m_heightPx = static_cast<int>(std::lround(m_heightMM * UNITS_PER_MM));
}

LC_EmfPaintDevice::~LC_EmfPaintDevice() {
    delete m_engine;
}

QPaintEngine* LC_EmfPaintDevice::paintEngine() const {
    return m_engine;
}

void* LC_EmfPaintDevice::takeMetafile() {
    return m_engine->takeMetafile();
}

int LC_EmfPaintDevice::metric(PaintDeviceMetric metric) const {
    switch (metric) {
        case PdmWidth:            return m_widthPx;
        case PdmHeight:           return m_heightPx;
        case PdmWidthMM:          return static_cast<int>(std::lround(m_widthMM));
        case PdmHeightMM:         return static_cast<int>(std::lround(m_heightMM));
        case PdmNumColors:        return INT_MAX;
        case PdmDepth:            return 24;
        case PdmDpiX:             return LOGICAL_DPI;
        case PdmDpiY:             return LOGICAL_DPI;
        case PdmPhysicalDpiX:     return LOGICAL_DPI;
        case PdmPhysicalDpiY:     return LOGICAL_DPI;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled:
            return static_cast<int>(1 * QPaintDevice::devicePixelRatioFScale());
        default:                  return 0;
    }
}

// ---------------------------------------------------------------------------
// LC_EmfPaintEngine
// ---------------------------------------------------------------------------

LC_EmfPaintEngine::LC_EmfPaintEngine(LC_EmfPaintDevice* device)
    : QPaintEngine(kFeatures)
    , m_device(device)
    , m_hdc(nullptr)
    , m_emf(nullptr) {
}

LC_EmfPaintEngine::~LC_EmfPaintEngine() {
    if (m_hdc != nullptr) {
        HENHMETAFILE leftover = CloseEnhMetaFile(static_cast<HDC>(m_hdc));
        if (leftover != nullptr) {
            DeleteEnhMetaFile(leftover);
        }
        m_hdc = nullptr;
    }
    if (m_emf != nullptr) {
        DeleteEnhMetaFile(static_cast<HENHMETAFILE>(m_emf));
        m_emf = nullptr;
    }
}

bool LC_EmfPaintEngine::begin(QPaintDevice* /*pdev*/) {
    HDC referenceDc = GetDC(nullptr);

    // Pixels-per-mm of the reference device. The metafile records this resolution,
    // so recorded coordinates are interpreted relative to it.
    double pxPerMmX = 96.0 / 25.4;
    double pxPerMmY = 96.0 / 25.4;
    if (referenceDc != nullptr) {
        const int horzRes = GetDeviceCaps(referenceDc, HORZRES);
        const int vertRes = GetDeviceCaps(referenceDc, VERTRES);
        const int horzMm = GetDeviceCaps(referenceDc, HORZSIZE);
        const int vertMm = GetDeviceCaps(referenceDc, VERTSIZE);
        if (horzRes > 0 && horzMm > 0) pxPerMmX = double(horzRes) / horzMm;
        if (vertRes > 0 && vertMm > 0) pxPerMmY = double(vertRes) / vertMm;
    }

    RECT frameMm;   // metafile frame is specified in units of 0.01 mm
    frameMm.left = 0;
    frameMm.top = 0;
    frameMm.right = toGdi(m_device->widthMM() * 100.0);
    frameMm.bottom = toGdi(m_device->heightMM() * 100.0);

    HDC emfDc = CreateEnhMetaFileW(referenceDc, nullptr, &frameMm,
                                   L"LibreCAD\0CAD selection\0\0");
    if (referenceDc != nullptr) {
        ReleaseDC(nullptr, referenceDc);
    }
    if (emfDc == nullptr) {
        return false;
    }
    m_hdc = emfDc;
    SetMapMode(emfDc, MM_TEXT);
    SetGraphicsMode(emfDc, GM_ADVANCED);
    SetBkMode(emfDc, TRANSPARENT);
    SetPolyFillMode(emfDc, WINDING);

    // We draw in high-resolution logical units (UNITS_PER_MM per mm). Scale them to
    // the reference device units so the declared frame matches the geometry, while
    // keeping sub-pixel (0.01 mm) precision in the recorded coordinates.
    XFORM xf;
    xf.eM11 = static_cast<FLOAT>(pxPerMmX / LC_EmfPaintDevice::UNITS_PER_MM);
    xf.eM22 = static_cast<FLOAT>(pxPerMmY / LC_EmfPaintDevice::UNITS_PER_MM);
    xf.eM12 = 0.0f;
    xf.eM21 = 0.0f;
    xf.eDx = 0.0f;
    xf.eDy = 0.0f;
    SetWorldTransform(emfDc, &xf);
    return true;
}

bool LC_EmfPaintEngine::end() {
    if (m_hdc != nullptr) {
        m_emf = CloseEnhMetaFile(static_cast<HDC>(m_hdc));
        m_hdc = nullptr;
    }
    return true;
}

void* LC_EmfPaintEngine::takeMetafile() {
    void* result = m_emf;
    m_emf = nullptr;
    return result;
}

void LC_EmfPaintEngine::updateState(const QPaintEngineState& state) {
    const QPaintEngine::DirtyFlags flags = state.state();
    if (flags & DirtyPen)        m_pen = state.pen();
    if (flags & DirtyBrush)      m_brush = state.brush();
    if (flags & DirtyTransform)  m_transform = state.transform();
}

void LC_EmfPaintEngine::syncFromPainter() {
    if (const QPainter* p = painter()) {
        m_pen = p->pen();
        m_brush = p->brush();
        m_transform = p->worldTransform();
    }
}

void LC_EmfPaintEngine::emitPolygons(const QList<QPolygonF>& polys, bool fillIt,
                                     bool strokeIt, Qt::FillRule rule) {
    if (m_hdc == nullptr || polys.isEmpty() || (!fillIt && !strokeIt)) {
        return;
    }
    HDC hdc = static_cast<HDC>(m_hdc);

    std::vector<POINT> points;
    std::vector<INT> counts;
    for (const QPolygonF& poly : polys) {
        if (poly.size() < 2) {
            continue;
        }
        for (const QPointF& pt : poly) {
            POINT p{toGdi(pt.x()), toGdi(pt.y())};
            points.push_back(p);
        }
        counts.push_back(static_cast<INT>(poly.size()));
    }
    if (counts.empty()) {
        return;
    }

    // Build pen.
    HPEN hPen = nullptr;
    if (strokeIt) {
        LOGBRUSH lb{BS_SOLID, toColorRef(m_pen.color()), 0};
        const double widthF = m_pen.widthF();
        if (m_pen.isCosmetic() || widthF <= 0.0) {
            hPen = ExtCreatePen(PS_COSMETIC | PS_SOLID, 1, &lb, 0, nullptr);
        } else {
            const DWORD style = PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND;
            hPen = ExtCreatePen(style, static_cast<DWORD>(std::max<LONG>(1, toGdi(widthF))),
                                &lb, 0, nullptr);
        }
    }
    HBRUSH hBrush = fillIt ? CreateSolidBrush(toColorRef(m_brush.color())) : nullptr;

    HGDIOBJ oldPen = SelectObject(hdc, hPen ? hPen : GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush ? hBrush : GetStockObject(NULL_BRUSH));

    if (fillIt) {
        SetPolyFillMode(hdc, rule == Qt::OddEvenFill ? ALTERNATE : WINDING);
        PolyPolygon(hdc, points.data(), counts.data(), static_cast<int>(counts.size()));
    } else {
        int offset = 0;
        for (INT c : counts) {
            Polyline(hdc, points.data() + offset, c);
            offset += c;
        }
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    if (hPen) DeleteObject(hPen);
    if (hBrush) DeleteObject(hBrush);
}

void LC_EmfPaintEngine::drawPath(const QPainterPath& path) {
    syncFromPainter();
    const QList<QPolygonF> polys = path.toSubpathPolygons(m_transform);
    const bool strokeIt = m_pen.style() != Qt::NoPen;
    const bool fillIt = m_brush.style() != Qt::NoBrush;
    emitPolygons(polys, fillIt, strokeIt, path.fillRule());
}

void LC_EmfPaintEngine::drawPolygon(const QPointF* points, int count, PolygonDrawMode mode) {
    syncFromPainter();
    QPolygonF poly;
    poly.reserve(count);
    for (int i = 0; i < count; ++i) {
        poly << m_transform.map(points[i]);
    }
    const bool strokeIt = m_pen.style() != Qt::NoPen;
    const bool fillIt = (mode != PolylineMode) && m_brush.style() != Qt::NoBrush;
    const Qt::FillRule rule = (mode == OddEvenMode) ? Qt::OddEvenFill : Qt::WindingFill;
    emitPolygons({poly}, fillIt, strokeIt, rule);
}

void LC_EmfPaintEngine::drawPolygon(const QPoint* points, int count, PolygonDrawMode mode) {
    QVarLengthArray<QPointF, 64> pts(count);
    for (int i = 0; i < count; ++i) {
        pts[i] = QPointF(points[i]);
    }
    drawPolygon(pts.constData(), count, mode);
}

void LC_EmfPaintEngine::drawEllipse(const QRectF& rect) {
    QPainterPath pp;
    pp.addEllipse(rect);
    drawPath(pp);
}

void LC_EmfPaintEngine::drawLines(const QLineF* lines, int count) {
    syncFromPainter();
    if (m_pen.style() == Qt::NoPen || m_hdc == nullptr) {
        return;
    }
    HDC hdc = static_cast<HDC>(m_hdc);
    LOGBRUSH lb{BS_SOLID, toColorRef(m_pen.color()), 0};
    const double widthF = m_pen.widthF();
    HPEN hPen = (m_pen.isCosmetic() || widthF <= 0.0)
        ? ExtCreatePen(PS_COSMETIC | PS_SOLID, 1, &lb, 0, nullptr)
        : ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                       static_cast<DWORD>(std::max<LONG>(1, toGdi(widthF))), &lb, 0, nullptr);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    for (int i = 0; i < count; ++i) {
        const QPointF a = m_transform.map(lines[i].p1());
        const QPointF b = m_transform.map(lines[i].p2());
        MoveToEx(hdc, toGdi(a.x()), toGdi(a.y()), nullptr);
        LineTo(hdc, toGdi(b.x()), toGdi(b.y()));
    }
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

void LC_EmfPaintEngine::drawLines(const QLine* lines, int count) {
    QVarLengthArray<QLineF, 32> f(count);
    for (int i = 0; i < count; ++i) {
        f[i] = QLineF(lines[i]);
    }
    drawLines(f.constData(), count);
}

void LC_EmfPaintEngine::drawRects(const QRectF* rects, int count) {
    syncFromPainter();
    const bool strokeIt = m_pen.style() != Qt::NoPen;
    const bool fillIt = m_brush.style() != Qt::NoBrush;
    for (int i = 0; i < count; ++i) {
        const QRectF& r = rects[i];
        QPolygonF poly;
        poly << m_transform.map(r.topLeft()) << m_transform.map(r.topRight())
             << m_transform.map(r.bottomRight()) << m_transform.map(r.bottomLeft())
             << m_transform.map(r.topLeft());
        emitPolygons({poly}, fillIt, strokeIt, Qt::WindingFill);
    }
}

void LC_EmfPaintEngine::drawRects(const QRect* rects, int count) {
    QVarLengthArray<QRectF, 32> f(count);
    for (int i = 0; i < count; ++i) {
        f[i] = QRectF(rects[i]);
    }
    drawRects(f.constData(), count);
}

void LC_EmfPaintEngine::drawImage(const QRectF& r, const QImage& image, const QRectF& sr,
                                  Qt::ImageConversionFlags /*flags*/) {
    if (m_hdc == nullptr || image.isNull()) {
        return;
    }
    syncFromPainter();
    QImage src = image;
    if (!sr.isNull() && sr != QRectF(image.rect())) {
        src = image.copy(sr.toRect());
    }
    src = src.convertToFormat(QImage::Format_ARGB32);   // memory order B,G,R,A == GDI BGRA

    const QRectF dst = m_transform.mapRect(r);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = src.width();
    bmi.bmiHeader.biHeight = -src.height();   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = static_cast<HDC>(m_hdc);
    SetStretchBltMode(hdc, HALFTONE);
    StretchDIBits(hdc,
                  toGdi(dst.left()), toGdi(dst.top()), toGdi(dst.width()), toGdi(dst.height()),
                  0, 0, src.width(), src.height(),
                  src.constBits(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void LC_EmfPaintEngine::drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) {
    drawImage(r, pm.toImage(), sr, Qt::AutoColor);
}

void LC_EmfPaintEngine::drawTextItem(const QPointF& p, const QTextItem& textItem) {
    syncFromPainter();
    const QString text = textItem.text();
    if (text.isEmpty() || m_hdc == nullptr) {
        return;
    }
    HDC hdc = static_cast<HDC>(m_hdc);
    const QFont qf = textItem.font();

    // Character height in logical (UNITS_PER_MM-per-mm) units. The GDI world
    // transform scales this to the reference device, so text stays vector and true
    // to size, and is recorded as real (editable) text in the metafile.
    double pixel = qf.pixelSize() > 0
        ? double(qf.pixelSize())
        : qf.pointSizeF() * (LC_EmfPaintDevice::LOGICAL_DPI / 72.0);
    if (pixel <= 0.0) {
        pixel = LC_EmfPaintDevice::UNITS_PER_MM * 3.0;   // 3 mm fallback
    }

    // Rotation, if any, carried by the painter transform.
    const double angleRad = std::atan2(m_transform.m12(), m_transform.m11());
    const int escapementTenths = static_cast<int>(std::lround(-angleRad * 1800.0 / M_PI));

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -static_cast<LONG>(std::lround(pixel));
    lf.lfWeight = qf.bold() ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = qf.italic() ? TRUE : FALSE;
    lf.lfUnderline = qf.underline() ? TRUE : FALSE;
    lf.lfStrikeOut = qf.strikeOut() ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfEscapement = escapementTenths;
    lf.lfOrientation = escapementTenths;
    lf.lfQuality = ANTIALIASED_QUALITY;
    const std::wstring face = qf.family().toStdWString();
    lstrcpynW(lf.lfFaceName, face.c_str(), LF_FACESIZE);

    HFONT hFont = CreateFontIndirectW(&lf);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);
    const COLORREF oldColor = SetTextColor(hdc, toColorRef(m_pen.color()));
    const int oldBk = SetBkMode(hdc, TRANSPARENT);
    const UINT oldAlign = SetTextAlign(hdc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);

    const QPointF dp = m_transform.map(p);
    const std::wstring wtext = text.toStdWString();
    ExtTextOutW(hdc, toGdi(dp.x()), toGdi(dp.y()), 0, nullptr,
                wtext.c_str(), static_cast<UINT>(wtext.size()), nullptr);

    SetTextAlign(hdc, oldAlign);
    SetBkMode(hdc, oldBk);
    SetTextColor(hdc, oldColor);
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

#endif // Q_OS_WIN
