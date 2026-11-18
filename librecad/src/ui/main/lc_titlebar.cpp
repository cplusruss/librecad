#include "lc_titlebar.h"

#ifdef Q_OS_WIN

#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QScreen>
#include <QWindowStateChangeEvent>
#include <qdrawutil.h>

#include <windows.h>

namespace {
HWND hwndOf(const QWidget* w) {
    return reinterpret_cast<HWND>(w->winId());
}
}

LC_CaptionButton::LC_CaptionButton(unsigned kind):
    kind(kind) {
    setFocusPolicy(Qt::NoFocus);
    const qreal dpr = devicePixelRatioF();
    const UINT dpi = static_cast<UINT>(qRound(96 * dpr));
    // classic caption buttons are the caption size less the 2 px frame gaps
    const int w = GetSystemMetricsForDpi(SM_CXSIZE, dpi) - 2;
    const int h = GetSystemMetricsForDpi(SM_CYSIZE, dpi) - 4;
    setFixedSize(qRound(w / dpr), qRound(h / dpr));
}

void LC_CaptionButton::paintEvent([[maybe_unused]] QPaintEvent* event) {
    const qreal dpr = devicePixelRatioF();
    QImage img = LC_TitleBar::frameControl(kind, isDown(), qRound(width() * dpr), qRound(height() * dpr));
    img.setDevicePixelRatio(dpr);
    QPainter p(this);
    p.drawImage(0, 0, img);
}

QImage LC_TitleBar::frameControl(unsigned kind, bool pushed, int w, int h) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = dc != nullptr ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (dc == nullptr || bmp == nullptr) {
        qFatal("DrawFrameControl: GDI allocation failed");
    }
    HGDIOBJ old = SelectObject(dc, bmp);
    RECT rect{0, 0, w, h};
    DrawFrameControl(dc, &rect, DFC_CAPTION, kind | (pushed ? DFCS_PUSHED : 0));
    GdiFlush();
    // GDI leaves alpha 0; the conversion is also the deep copy out of the DIB
    QImage img = QImage(static_cast<const uchar*>(bits), w, h, w * 4, QImage::Format_RGB32)
                     .convertToFormat(QImage::Format_ARGB32);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return img;
}

int LC_TitleBar::edgeHit(const QSize& size, const QPoint& pos) {
    const bool left = pos.x() < BORDER, right = pos.x() >= size.width() - BORDER;
    const bool top = pos.y() < BORDER, bottom = pos.y() >= size.height() - BORDER;
    if (top) {
        return left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
    }
    if (bottom) {
        return left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
    }
    return left ? HTLEFT : right ? HTRIGHT : 0;
}

QString LC_TitleBar::captionText(const QString& title, bool modified) {
    QString text = title;
    return text.replace(QStringLiteral("[*]"), modified ? QStringLiteral("*") : QString());
}

LC_TitleBar::LC_TitleBar(QMainWindow* win):
    m_win(win) {
    m_icon = new QLabel(this);
    m_menuBar = new QMenuBar(this);
    m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_title = new QLabel(this);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont font = m_title->font();
    font.setBold(true);
    m_title->setFont(font);
    m_title->setMinimumWidth(TITLE_MIN);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(2, 1, 2, 1);
    row->setSpacing(2);
    row->addWidget(m_icon, 0, Qt::AlignVCenter);
    row->addWidget(m_menuBar, 0, Qt::AlignVCenter);
    row->addWidget(m_title, 1);

    auto* caption = new QHBoxLayout();
    caption->setSpacing(0);
    auto* minimize = new LC_CaptionButton(DFCS_CAPTIONMIN);
    connect(minimize, &QAbstractButton::clicked, win, &QWidget::showMinimized);
    m_max = new LC_CaptionButton(DFCS_CAPTIONMAX);
    connect(m_max, &QAbstractButton::clicked, this, [this] { zoom(!m_win->isMaximized()); });
    auto* close = new LC_CaptionButton(DFCS_CAPTIONCLOSE);
    connect(close, &QAbstractButton::clicked, win, &QWidget::close);
    caption->addWidget(minimize);
    caption->addWidget(m_max);
    caption->addSpacing(2);
    caption->addWidget(close);
    row->addLayout(caption);

    // caption, not controls: hitTest sees no child here and reports HTCAPTION
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(win, &QWidget::windowTitleChanged, this, &LC_TitleBar::syncTitle);
}

void LC_TitleBar::install() {
    m_win->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    m_win->setMenuWidget(this);
    applyFrameStyle();
    syncIcon();
    syncTitle();
}

void LC_TitleBar::syncIcon() {
    m_icon->setPixmap(m_win->windowIcon().pixmap(16, 16));
}

void LC_TitleBar::syncTitle() {
    m_title->setText(captionText(m_win->windowTitle(), m_win->isWindowModified()));
}

bool LC_TitleBar::event(QEvent* event) {
    // an application icon change reaches the window's children directly
    if (event->type() == QEvent::WindowIconChange) {
        syncIcon();
    }
    return QWidget::event(event);
}

void LC_TitleBar::applyFrameStyle() {
    // Qt's frameless window is a bare WS_POPUP, and Windows only sizes windows that carry WS_THICKFRAME; add it back
    // and report a zero frame in WM_NCCALCSIZE so nothing extra is drawn. WS_CAPTION is added too so external window
    // managers treat this as a captioned window; the zero frame leaves nothing for it to paint
    HWND hwnd = hwndOf(m_win);
    SetWindowLongPtrW(hwnd, GWL_STYLE, GetWindowLongPtrW(hwnd, GWL_STYLE) | WS_THICKFRAME | WS_CAPTION);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    syncFrame();
}

bool LC_TitleBar::restoreFrameGeometry(const QByteArray& geometry) {
    // true when the saved window was maximized. Qt saves a frameless window maximized on a screen with no taskbar as
    // full screen, and restoring that swaps in a bare popup style that drops the caption and frame bits (no caption
    // double-click, no resize edges); that state also comes back maximized, through zoom()
    m_win->restoreGeometry(geometry);
    const bool zoomed = m_win->isMaximized() || m_win->isFullScreen();
    if (zoomed) {
        m_win->setWindowState(Qt::WindowNoState); // the restored size stays the saved normal geometry
    }
    applyFrameStyle();
    return zoomed;
}

void LC_TitleBar::zoom(bool on) {
    // Qt's showMaximized only stretches a frameless window over the screen without WS_MAXIMIZE, so Windows has no
    // restore size and caption double-click disagrees with the button; let Windows maximize and restore it
    SendMessageW(hwndOf(m_win), WM_SYSCOMMAND, on ? SC_MAXIMIZE : SC_RESTORE, 0);
}

void LC_TitleBar::syncFrame() {
    if (m_win->isFullScreen()) {
        m_win->setContentsMargins(0, 0, 0, 0);
    }
    else if (m_win->isMaximized()) {
        // a maximized captioned window hangs a frame width past every edge of the work area; keep the content on
        // screen by padding exactly that overhang
        const QRect g = m_win->geometry();
        const QRect a = m_win->screen()->availableGeometry();
        m_win->setContentsMargins(qMax(0, a.left() - g.left()), qMax(0, a.top() - g.top()),
                                  qMax(0, g.right() - a.right()), qMax(0, g.bottom() - a.bottom()));
    }
    else {
        m_win->setContentsMargins(BORDER, BORDER, BORDER, BORDER);
    }
    m_max->kind = m_win->isMaximized() ? DFCS_CAPTIONRESTORE : DFCS_CAPTIONMAX;
    m_max->update();
    m_win->update(); // the 3D border lives in the margin; a state change leaves it stale otherwise
}

void LC_TitleBar::windowChanged(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        // leaving full screen puts back the style Qt saved, which may predate applyFrameStyle
        if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowFullScreen) {
            applyFrameStyle();
        }
        else {
            syncFrame();
        }
    }
    else if (event->type() == QEvent::ModifiedChange) {
        syncTitle();
    }
}

void LC_TitleBar::windowResized() {
    if (m_win->isMaximized()) {
        syncFrame(); // the maximized geometry arrives after the state change
    }
}

void LC_TitleBar::paintBorder() {
    if (m_win->contentsMargins().left() != 0 && !m_win->isMaximized()) {
        QPainter p(m_win);
        const QBrush fill = m_win->palette().window();
        qDrawWinPanel(&p, m_win->rect(), m_win->palette(), false, &fill);
    }
}

int LC_TitleBar::hitTest(const QPoint& pos) const {
    if (m_win->contentsMargins().left() != 0 && !m_win->isMaximized()) {
        const int code = edgeHit(m_win->size(), pos);
        if (code != 0) {
            return code;
        }
    }
    const QPoint local = mapFrom(m_win, pos);
    if (rect().contains(local) && childAt(local) == nullptr) {
        return HTCAPTION;
    }
    return 0;
}

bool LC_TitleBar::nativeEvent(void* message, qintptr* result) {
    const MSG* msg = static_cast<MSG*>(message);
    switch (msg->message) {
        case WM_NCCALCSIZE:
            if (msg->wParam != 0) {
                *result = 0;
                return true;
            }
            return false;
        case WM_NCACTIVATE:
            // with WS_CAPTION, the default handler repaints a native caption strip over the top edge on
            // (de)activation; lParam -1 keeps the activation bookkeeping and skips that paint
            *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, -1);
            return true;
        case WM_NCHITTEST: {
            const int code = hitTest(m_win->mapFromGlobal(QCursor::pos()));
            if (code != 0) {
                *result = code;
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

#endif
