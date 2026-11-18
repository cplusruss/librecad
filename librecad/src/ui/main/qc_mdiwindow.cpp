/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2019 Shawn Curry (noneyabiz@mail.wasent.cz)
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

#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QMdiArea>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionTitleBar>

#include "lc_documentsstorage.h"
#include "lc_graphicviewport.h"
#include "lc_printpreviewview.h"
#include "qc_applicationwindow.h"
#include "qc_mdiwindow.h"

#include "lc_fontfileviewer.h"
#include "qg_exitdialog.h"
#include "rs_debug.h"

#ifdef Q_OS_WIN
#include <QImage>
#include <qt_windows.h>
#endif

// Windows-Classic MDI title bar with a single, fixed background colour.
//
// We wrap QStyleFactory::create("Windows") in a QProxyStyle and override
// CC_TitleBar drawing only. The base Windows style provides the metrics
// (button positions, frame thickness). The proxy paints a flat title bar
// fill+text, and forces State_Active so the inactive frame doesn't dim or
// recolour.
//
// Qt's own caption-button glyphs are heavier than the native ones, so both
// the MDI child's title-bar buttons and the maximized-child controls in the
// menubar (CC_MdiControls) are drawn with Win32 DrawFrameControl - the same
// call Windows uses for top-level captions - so they match the main window.
//
// Owned by qApp; one shared instance, also installed as the app style.
namespace {

#ifdef Q_OS_WIN
// Draws one native caption button (DFCS_CAPTIONMIN/MAX/RESTORE/CLOSE/HELP)
// into r. Rendered into an offscreen GDI bitmap at device resolution, then
// blitted, since the QPainter target has no HDC.
bool drawNativeCaptionButton(QPainter* p, const QRect& r, UINT type,
                             bool pressed, bool disabled) {
    if (!r.isValid()) {
        return false;
    }
    const qreal dpr = (p->device() != nullptr) ? p->device()->devicePixelRatioF() : 1.0;
    const int w = qMax(1, qRound(r.width() * dpr));
    const int h = qMax(1, qRound(r.height() * dpr));

    // Top-down 32bpp DIB; GDI leaves the 4th byte at 0, so read it as RGB32
    // (alpha ignored) rather than via QImage::fromHBITMAP.
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage img;
    if (mem != nullptr && bmp != nullptr && bits != nullptr) {
        HGDIOBJ old = SelectObject(mem, bmp);
        RECT rc{0, 0, w, h};
        UINT state = type;
        if (pressed) state |= DFCS_PUSHED;
        if (disabled) state |= DFCS_INACTIVE;
        if (DrawFrameControl(mem, &rc, DFC_CAPTION, state)) {
            GdiFlush();
            img = QImage(static_cast<const uchar*>(bits), w, h, w * 4,
                         QImage::Format_RGB32).copy();
        }
        SelectObject(mem, old);
    }
    if (bmp != nullptr) DeleteObject(bmp);
    if (mem != nullptr) DeleteDC(mem);

    if (img.isNull()) {
        return false;
    }
    img.setDevicePixelRatio(dpr);
    p->drawImage(r.topLeft(), img);
    return true;
}
#endif

class LC_MdiTitleBarProxyStyle : public QProxyStyle {
public:
    explicit LC_MdiTitleBarProxyStyle(QStyle* baseStyle) : QProxyStyle(baseStyle) {}

    // QCommonStyle paints CC_TitleBar's gradient under the SC_TitleBarLabel
    // branch (Highlight->Base linear gradient). We can't kill the gradient by
    // patching palette colours alone - the Windows style still gradients
    // between Highlight and Base. Instead we fully take over the background +
    // label + icon painting, then let the base style draw just the buttons.
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex* opt,
                            QPainter* p, const QWidget* w) const override {
#ifdef Q_OS_WIN
        // Min / restore / close shown in the menubar for a maximized child.
        if (cc == CC_MdiControls) {
            const struct { SubControl sc; UINT type; } buttons[] = {
                {SC_MdiMinButton,    DFCS_CAPTIONMIN},
                {SC_MdiNormalButton, DFCS_CAPTIONRESTORE},
                {SC_MdiCloseButton,  DFCS_CAPTIONCLOSE},
            };
            const bool disabled = !(opt->state & State_Enabled);
            for (const auto& b : buttons) {
                if (!(opt->subControls & b.sc)) continue;
                const bool pressed = (opt->activeSubControls & b.sc)
                                     && (opt->state & State_Sunken);
                drawNativeCaptionButton(p, proxy()->subControlRect(cc, opt, b.sc, w),
                                        b.type, pressed, disabled);
            }
            return;
        }
#endif
        if (cc == CC_TitleBar && qobject_cast<const QMdiSubWindow*>(w) != nullptr) {
            if (auto* tb = qstyleoption_cast<const QStyleOptionTitleBar*>(opt)) {
                QStyleOptionTitleBar copy = *tb;
                // Force active state so the inactive frame doesn't dim or
                // change colour at all.
                copy.titleBarState |= int(QStyle::State_Active);
                copy.state |= QStyle::State_Active;

                const QColor bg(212, 208, 200);

                p->save();

                // Flat single-colour background, no gradient.
                p->fillRect(copy.rect, bg);

                // System-menu icon (left edge).
                if ((copy.subControls & SC_TitleBarSysMenu)
                    && (copy.titleBarFlags & Qt::WindowSystemMenuHint)) {
                    QRect sysRect = proxy()->subControlRect(CC_TitleBar, &copy,
                                                           SC_TitleBarSysMenu, w);
                    if (sysRect.isValid()) {
                        QIcon ic = copy.icon;
                        if (ic.isNull()) {
                            ic = proxy()->standardIcon(SP_TitleBarMenuButton,
                                                       &copy, w);
                        }
                        if (!ic.isNull()) {
                            ic.paint(p, sysRect);
                        }
                    }
                }

                // Title text.
                if (copy.subControls & SC_TitleBarLabel) {
                    QRect labelRect = proxy()->subControlRect(CC_TitleBar, &copy,
                                                              SC_TitleBarLabel, w);
                    if (labelRect.isValid() && !copy.text.isEmpty()) {
                        p->setPen(Qt::black);
                        if (w != nullptr) p->setFont(w->font());
                        QString shown = p->fontMetrics().elidedText(
                            copy.text, Qt::ElideRight, labelRect.width() - 4);
                        p->drawText(labelRect.adjusted(2, 0, -2, 0),
                                    Qt::AlignVCenter | Qt::AlignLeft, shown);
                    }
                }

                p->restore();
                copy.subControls &= ~(SC_TitleBarLabel | SC_TitleBarSysMenu);

#ifdef Q_OS_WIN
                // Native min / max / restore / close / help. Visibility rules
                // mirror QCommonStyle's CC_TitleBar so the same buttons appear.
                {
                    const auto flags = copy.titleBarFlags;
                    const auto tbState = copy.titleBarState;
                    const bool minimized = tbState & Qt::WindowMinimized;
                    const bool maximized = tbState & Qt::WindowMaximized;
                    const bool showNormal =
                        ((flags & Qt::WindowMinimizeButtonHint) && minimized)
                        || ((flags & Qt::WindowMaximizeButtonHint) && maximized);
                    const struct { SubControl sc; UINT type; bool visible; } buttons[] = {
                        {SC_TitleBarCloseButton, DFCS_CAPTIONCLOSE,
                         bool(flags & Qt::WindowSystemMenuHint)},
                        {SC_TitleBarMaxButton, DFCS_CAPTIONMAX,
                         (flags & Qt::WindowMaximizeButtonHint) && !maximized},
                        {SC_TitleBarMinButton, DFCS_CAPTIONMIN,
                         (flags & Qt::WindowMinimizeButtonHint) && !minimized},
                        {SC_TitleBarNormalButton, DFCS_CAPTIONRESTORE, showNormal},
                        {SC_TitleBarContextHelpButton, DFCS_CAPTIONHELP,
                         bool(flags & Qt::WindowContextHelpButtonHint)},
                    };
                    const bool disabled = !(copy.state & State_Enabled);
                    for (const auto& b : buttons) {
                        if (!(copy.subControls & b.sc)) continue;
                        if (b.visible) {
                            const bool pressed = (copy.activeSubControls & b.sc)
                                                 && (copy.state & State_Sunken);
                            QRect r = proxy()->subControlRect(CC_TitleBar, &copy, b.sc, w);
                            if (!drawNativeCaptionButton(p, r, b.type, pressed, disabled)) {
                                continue; // leave it for the base style
                            }
                        }
                        copy.subControls &= ~QStyle::SubControls(b.sc);
                    }
                }
#endif

                // Hand off to base style for whatever is left (shade /
                // unshade, or anything native drawing couldn't handle).
                if (copy.subControls != 0) {
                    QProxyStyle::drawComplexControl(cc, &copy, p, w);
                }
                return;
            }
        }
        QProxyStyle::drawComplexControl(cc, opt, p, w);
    }

    int pixelMetric(PixelMetric pm, const QStyleOption* opt,
                    const QWidget* w) const override {
        if (pm == PM_TitleBarHeight && qobject_cast<const QMdiSubWindow*>(w) != nullptr) {
            // Standard Windows-Classic MDI child caption (~SM_CYSMCAPTION).
            // Base style uses SM_CYCAPTION which is the larger top-level value
            // (~22-26) plus font padding, hence the tall bar.
            int fontH = w->fontMetrics().height();
            return qMax(fontH + 2, 18);
        }
        return QProxyStyle::pixelMetric(pm, opt, w);
    }
};

} // namespace

QStyle* QC_MDIWindow::classicStyle() {
    static QStyle* shared = nullptr;
    if (shared == nullptr) {
        QStyle* base = QStyleFactory::create(QStringLiteral("Windows"));
        if (base == nullptr) {
            base = QStyleFactory::create(QStringLiteral("windows"));
        }
        if (base == nullptr) {
            return nullptr;
        }
        shared = new LC_MdiTitleBarProxyStyle(base);
        shared->setParent(qApp); // keep ownership off any individual widget
    }
    return shared;
}

/**
 * Constructor.
 *
 * @param doc Pointer to an existing document of NULL if a new
 *   document shall be created for this window.
 * @param parent An instance of QMdiArea.
 */
QC_MDIWindow::QC_MDIWindow(RS_Document *doc, QWidget *parent, bool printPreview, LC_ActionContext* actionContext)
    : QMdiSubWindow(parent, {Qt::WA_DeleteOnClose})
    , m_owner{doc == nullptr}{
    setAttribute(Qt::WA_DeleteOnClose);
    if (QStyle* s = classicStyle()) {
        setStyle(s);
    }
    m_cadMdiArea=qobject_cast<QMdiArea*>(parent);

    if (doc==nullptr) {
        m_document = new RS_Graphic();
        m_document->newDoc();
        // Pick up the user's globally-remembered grid visibility + spacing
        // for blank drawings too, not just file opens. No-op until the user
        // has captured a preference.
        if (auto* g = m_document->getGraphic()) {
            g->applyGlobalGridDefaults();
        }
    } else {
        m_document = doc;
    }

    setupGraphicView(parent, printPreview, actionContext);

    static unsigned idCounter = 0;
    id = idCounter++;

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    addWidgetsListeners();
}

/**
 * Destructor.
 *
 * Deletes the document associated with this window.
 */
QC_MDIWindow::~QC_MDIWindow(){
    try {
        if (!(m_graphicView != nullptr && m_graphicView->isCleanUp())) {
            //do not clear layer/block lists, if application is being closed
            removeWidgetsListeners();
            if (m_owner) {
                delete m_document;
            }
            m_document = nullptr;
        }
    } catch (...) {
        LC_ERR << __func__ << "(): received exception";
    }
}

void QC_MDIWindow::setupGraphicView(QWidget *parent, bool printPreview, LC_ActionContext* actionContext){
    if (printPreview){
        m_graphicView = new LC_PrintPreviewView(this, m_document, actionContext);
        m_graphicView->initView();
    }
    else{
        m_graphicView = new QG_GraphicView(this, m_document, actionContext);
        m_graphicView->initView();
    }
    m_graphicView->setPrintPreview(printPreview);
    m_graphicView->setObjectName("graphicview");

    auto receiver = dynamic_cast<QC_ApplicationWindow *>(parent->window());
    if (receiver != nullptr) {
        connect(m_graphicView, &RS_GraphicView::previous_zoom_state, receiver, &QC_ApplicationWindow::setPreviousZoomEnable);
    }

    setWidget(m_graphicView);
}

void QC_MDIWindow::addWidgetsListeners(){
    if (m_document != nullptr) {
        RS_Graphic* graphic = m_document -> getGraphic();
        if (graphic != nullptr) {
            graphic->setModificationListener(this);
        }
    }
}

void QC_MDIWindow::removeWidgetsListeners() const {
    if (m_document != nullptr) {
        RS_Graphic* graphic = m_document -> getGraphic();
        if (graphic != nullptr) {
            graphic->setModificationListener(nullptr);
        }
    }
}


QG_GraphicView* QC_MDIWindow::getGraphicView() const{
    return m_graphicView;
}

RS_Document* QC_MDIWindow::getDocument() const{
	return m_document;
}

unsigned QC_MDIWindow::getId() const{
	return id;
}

void QC_MDIWindow::setParentWindow(QC_MDIWindow* p) {
	m_parentWindow = p;
}

QC_MDIWindow* QC_MDIWindow::getParentWindow() const {
	return m_parentWindow;
}

RS_Graphic* QC_MDIWindow::getGraphic() const {
	return m_document->getGraphic();
}

/**
 * Adds another MDI window to the list of known windows that
 * depend on this one. This can be another view or a view for
 * a particular block.
 */
void QC_MDIWindow::addChildWindow(QC_MDIWindow* w) {
    m_childWindows.append(w);
    w->setParentWindow(this);

    const size_t size = m_childWindows.count();
    LC_LOG<<__func__<<"(): children: "<<size;
}

/**
 * Removes a child window.
 *
 * @see addChildWindow
 */
void QC_MDIWindow::removeChildWindow(QC_MDIWindow* w) {
  if(!m_childWindows.empty()){
        if(m_childWindows.contains(w)){
            m_childWindows.removeAll(w);
        }
    }
}

QList<QC_MDIWindow*>& QC_MDIWindow::getChildWindows(){
	return m_childWindows;
}

/**
 * @return pointer to the print preview of this drawing or NULL.
 */
QC_MDIWindow* QC_MDIWindow::getPrintPreview() {
  for(auto* w: m_childWindows){
    if(w != nullptr && w->getGraphicView()->isPrintPreview()){
      return w;
    }
  }
  return nullptr;
}

/**
 * Called by Qt when the user closes this MDI window.
 */
// fixme - sand - files - fully delegate to main window ?
void QC_MDIWindow::closeEvent(QCloseEvent* ce) {
    bool cancel = false;
    bool hasParent = getParentWindow() != nullptr;
    const auto& appWin = QC_ApplicationWindow::getAppWindow();

    if (getDocument()->isModified() && !hasParent) {
        int exitChoice = QG_ExitDialog::Save;

        switch (m_saveOnClosePolicy) {
            case ASK:
            // CANCEL is a transient flag the *previous* close attempt set so a
            // wrapping Close-All loop knew to abort. For a fresh manual close
            // (Ctrl+W again after the user dismissed the save prompt) the
            // intent is "ask me again." Without this fallthrough the switch
            // ran into `default:` and exitChoice silently kept its initial
            // `Save` value — the save then succeeded, doClose hit the CANCEL
            // early-return guard, and ce->accept() left m_windowList desynced
            // from the QMdiArea, which manifested as the app exiting.
            case CANCEL: {
                exitChoice = appWin->showCloseDialog(this);
                break;
            }
            case SAVE: {
                exitChoice = QG_ExitDialog::Save;
                break;
            }
            case DONT_SAVE: {
                exitChoice = QG_ExitDialog::DontSave;
                break;
            }
            default:
                break;
        }
        switch (exitChoice) {
            case QG_ExitDialog::Save:
                cancel = !appWin->doSave(this);
                break;
            case QG_ExitDialog::DontSave:
                break;
            case QG_ExitDialog::Cancel:
            default:
                cancel = true;
                break;
        }
    }


    if (cancel){
        appWin->autoSaveCurrentDrawing();
        m_saveOnClosePolicy = CANCEL;
        ce->ignore();
    } else {
        appWin->doClose(this);
        appWin->doArrangeWindows(RS2::CurrentMode);
        ce->accept(); // handling delegated to QApplication
    }
}

/**
 * Called when the current pen (color, style, width) has changed.
 * Sets the active pen for the document in this MDI window.
 */
void QC_MDIWindow::slotPenChanged(const RS_Pen& pen) {
	if (m_document != nullptr) {
        m_document->setActivePen(pen);
    }
}

/**
 * Creates a new empty document in this MDI window.
 */
void QC_MDIWindow::slotFileNew() {
	if (m_document != nullptr && m_graphicView != nullptr) {
        m_document->newDoc();
        m_graphicView->redraw();
    }
}

/**
 * Creates a new document, loading template, in this MDI window.
 */
bool QC_MDIWindow::loadDocumentFromTemplate(const QString& fileName, RS2::FormatType type) {
    return m_documentsStorage->loadDocumentFromTemplate(m_document, m_graphicView, fileName, type);
}

/**
 * Opens the given file in this MDI window.
 */
bool QC_MDIWindow::loadDocument(const QString& fileName, RS2::FormatType type) {
    removeWidgetsListeners();
    bool loaded = m_documentsStorage->loadDocument(m_document, fileName, type);
    addWidgetsListeners();
    if (loaded) {
        RS_Graphic* graphic = m_document->getGraphic();
        if (graphic != nullptr) {
            RS_GraphicView *gv = graphic->getGraphicView(); // fixme - eliminate this dependency!
            if (gv != nullptr) {
                // fixme - sand - review and probably move initialization of UCS - as normal support of VIEWPORT will be available
                // todo - not sure whether this is right place for setting up current wcs.
                // Actually, it seems that it's better to rely on reading viewport (were setting for the offset and zoom are set.
                // however, must probably with proper support of VIEW, they will be reworked too..
                // So let it have here for now so far
                LC_GraphicViewport* viewport = gv->getViewPort();
                viewport->initAfterDocumentOpen();
            }
        }

        // fixme - sand - move support of fonts in some separate space?
        if (fileName.endsWith(".lff") || fileName.endsWith(".cxf")) {
            // fixme - sand - move to upper layer
            drawChars();
            m_graphicView->zoomAuto(false);
        } else
            m_graphicView->redraw();

        // Defense in depth: even after we flipped ModifyOnViewChange to OFF,
        // other load-time steps (DXF format up-conversion, applyUCSAfterLoad,
        // layer-0 auto-create, applyGlobalGridDefaults) can spuriously flag
        // the document modified. The user did NOTHING — clear the flag so
        // close doesn't prompt for save. This runs AFTER all post-load
        // programmatic setup so anything those steps did is wiped.
        if (m_document != nullptr) {
            m_document->setModified(false);
        }
    } else {

    }
    return loaded;
}

/**
 * Saves the current file.
 *
 * @param  isAutoSave true if this is an "autosave" operation.
 *                    false if this is "Save" operation requested
 *                    by the user.
 * @return true if the file was saved successfully.
 *         false if the file could not be saved or the document
 *         is invalid.
 */
bool QC_MDIWindow::saveDocument(bool &cancelled, [[maybe_unused]]bool isAutoSave) {
    bool result = m_documentsStorage->saveDocument(m_document, m_graphicView, cancelled);
    setWindowModified(m_document->isModified());
    return result;
}

bool QC_MDIWindow::autoSaveDocument(QString& autosaveFileName){
    bool result = m_documentsStorage->autoSaveDocument(m_document, m_graphicView, autosaveFileName);
    return result;
}

/**
 * Saves the current file. The user is asked for a new filename
 * and format.
 *
 * @return true if the file was saved successfully or the user cancelled.
 *         false if the file could not be saved or the document
 *         is invalid.
 */
bool QC_MDIWindow::saveDocumentAs(bool &cancelled) {
    bool result = m_documentsStorage->saveDocumentAs(m_document, m_graphicView, cancelled);
    setWindowModified(m_document->isModified());
    return result;
}

void QC_MDIWindow::zoomAuto() {
	if(m_graphicView){
        if(m_graphicView->isPrintPreview()){
            m_graphicView->getViewPort()->zoomPage();
        }else{
            m_graphicView->zoomAuto();
        }
    }
}

bool QC_MDIWindow::isModified(){
    return getDocument()->isModified();
}

void QC_MDIWindow::drawChars() {
   LC_FontFileViewer viewer(m_document);
   viewer.drawFontChars();
}

// fixme - sand - refactor printing in general!!!
void QC_MDIWindow::slotFilePrint() {

    RS_DEBUG->print("QC_MDIWindow::slotFilePrint");

    //statusBar()->showMessage(tr("Printing..."));
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    if (dialog.exec()) {
        QPainter painter;
        painter.begin(&printer);

        ///////////////////////////////////////////////////////////////////
        // TODO: Define printing by using the QPainter methods here

        painter.end();
    };

    //statusBar()->showMessage(tr("Ready."));
}

/**
 * Streams some info about an MDI window to stdout.
 */
std::ostream& operator << (std::ostream& os, QC_MDIWindow& w) {
    os << "QC_MDIWindow[" << w.getId() << "]:\n";
	if (w.m_parentWindow) {
        os << "  parentWindow: " << w.m_parentWindow->getId() << "\n";
    } else {
        os << "  parentWindow: nullptr\n";
    }
    int i=0;
    for(auto p: const_cast<const QList<QC_MDIWindow*>&>(w.m_childWindows)){
		os << "  childWindow[" << i++ << "]: "
		   << p->getId() << "\n";
	}
    return os;
}

/**
 * Return true if this window has children (QC_MDIWindow).
 */
bool QC_MDIWindow::has_children() const{
    return !m_childWindows.isEmpty();
}

void QC_MDIWindow::graphicModified([[maybe_unused]]const RS_Graphic* g, bool modified){
    setWindowModified(modified);
    auto& appWin = QC_ApplicationWindow::getAppWindow();
    if (appWin !=nullptr) {
        appWin->setSaveEnable(modified);
    }
}

void QC_MDIWindow::undoStateChanged([[maybe_unused]]const RS_Graphic *g, bool undoAvailable, bool redoAvailable){
    auto& appWin = QC_ApplicationWindow::getAppWindow();
    if (appWin !=nullptr) {
        appWin->setRedoEnable(redoAvailable);
        appWin->setUndoEnable(undoAvailable);
    }
}

QString QC_MDIWindow::getFileName() const{
    RS_Graphic* graphic = m_document->getGraphic();
    if (graphic == nullptr) {
        return "";
    }
    return graphic->getFilename();
}
