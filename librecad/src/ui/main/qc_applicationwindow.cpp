/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2019 Shawn Curry (noneyabiz@mail.wasent.cz)
** Copyright (C) 2018 Simon Wells (simonrwells@gmail.com)
** Copyright (C) 2015-2016 ravas (github.com/r-a-v-a-s)
** Copyright (C) 2015 A. Stebich (librecad@mail.lordofbikes.de)
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
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

// Changes: https://github.com/LibreCAD/LibreCAD/commits/master/librecad/src/main/qc_applicationwindow.cpp


#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMdiArea>
#include <QMessageBox>
#include <QMimeData>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QStyleHints>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "lc_iconcolorsoptions.h"

#include "lc_actiongroupmanager.h"
#include "lc_actionoptionsmanager.h"
#include "lc_actionsshortcutsdialog.h"
#include "lc_anglesbasiswidget.h"
#include "lc_foreground.h"
#include "lc_applicationwindowinitializer.h"
#include "lc_appwindowdialogsinvoker.h"
#include "lc_creatorinvoker.h"
#include "lc_customstylehelper.h"
#include "lc_defaultactioncontext.h"
#include "lc_dialog.h"
#include "lc_exporttoimageservice.h"
#include "lc_graphicviewport.h"
#include "lc_gridviewinvoker.h"
#include "lc_infocursorsettingsmanager.h"
#include "lc_lastopenfilesopener.h"
#include "lc_layertreewidget.h"
#include "lc_menufactory.h"
#include "lc_namedviewslistwidget.h"
#include "lc_penpalettewidget.h"
#include "lc_penwizard.h"
#include "lc_printing.h"
#include "lc_plugininvoker.h"
#include "lc_titlebar.h"
#include "lc_qtstatusbarmanager.h"
#include "lc_quickinfowidget.h"
#include "lc_relzerocoordinateswidget.h"
#include "lc_snapoptionswidgetsholder.h"
#include "lc_snapmanager.h"
#include "lc_ucslistwidget.h"
#include "lc_ucsstatewidget.h"
#include "lc_workspacesinvoker.h"
#include "qc_applicationwindow.h"

#include "qc_dialogfactory.h"
#include "lc_dlgdimstylemanager.h"
#include "qc_mdiwindow.h"
#include "rs_color.h"
#include "rs_document.h"
#include "rs_layer.h"
#include "rs_graphic.h"
#include "rs_modification.h"
#include "qg_actionhandler.h"
#include "qg_activelayername.h"
#include "qg_blockwidget.h"
#include "qg_commandwidget.h"
#include "qg_coordinatewidget.h"
#include "qg_exitdialog.h"
#include "lc_unsavedchangesdialog.h"
#include "qg_graphicview.h"
#include "qg_layerwidget.h"
#include "qg_librarywidget.h"
#include "qg_mousewidget.h"
#include "qg_pentoolbar.h"
#include "qg_recentfiles.h"
#include "qg_selectionwidget.h"
#include "qg_snaptoolbar.h"
#include "rs_actioninterface.h"
#include "rs_actionlibraryinsert.h"
#include "rs_actionprintpreview.h"
#include "rs_debug.h"
#include "rs_settings.h"
#include "rs_units.h"
#include "twostackedlabels.h"

#ifndef QC_ABOUT_ICON
# define QC_ABOUT_ICON ":/images/intro_librecad.png"
#endif

/*	- Window Title Bar Extra (character) Size.
 *	- Notes: Extra characters appearing in the windows title bar
 *	  are " - [", ... "]" (5), and sometimes "Print preview of " (17).
 *	*/
#define WTB_EXTRA_SIZE        (5 + 17)

/*	Window Title Bar Maximum Size.
 *	Notes: On Windows XP, this is 79.
 *	*/
#define WTB_MAX_SIZE        79

/**
 * Constructor. Initializes the app.
 */
QC_ApplicationWindow::QC_ApplicationWindow(){
#ifdef _WINDOWS
    qt_ntfs_permission_lookup++; // turn checking on
#endif

    LC_ApplicationWindowInitializer initializer(this);
    initializer.initApplication();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Re-apply icon styling defaults when the OS color scheme flips so the
    // toolbar icons don't go invisible on a newly-dark palette. Stored user
    // overrides are preserved because loadColor() returns the stored value
    // when present and falls back to the (now theme-aware) default only
    // when the key is absent.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
                LC_IconColorsOptions opts;
                opts.loadSettings();
                opts.applyOptions();
                fireIconsRefresh();
            });
#endif

    // Re-paint toolbars when the OS-level primary screen changes (e.g. the
    // user swaps which monitor is primary while LibreCAD is running). The
    // canvas's own m_isHiDpi state is computed once at LC_GraphicViewRenderer
    // construction and currently does NOT re-evaluate here — see the
    // // fixme - sand comment at lc_graphicviewrenderer.cpp:43. Tracking that
    // through to the per-MDI-window renderer is follow-up work documented in
    // Task D of the plan; this connect at least refreshes icons and any
    // listener wired to iconsRefreshed().
    connect(qApp, &QGuiApplication::primaryScreenChanged,
            this, [this](QScreen*) { fireIconsRefresh(); });

    // Ctrl+Space launches Layer Walk (AutoCAD LAYWALK). LayerWalk has no QAction
    // in the action factory, so it can't ride the normal shortcut pipeline; we
    // install a window-level QShortcut instead. Skip if some user-registered
    // QAction already owns Ctrl+Space, so customised bindings always win.
    {
        const QKeySequence ks(Qt::CTRL | Qt::Key_Space);
        bool conflict = false;
        for (QAction* a : findChildren<QAction*>()) {
            if (a == nullptr) continue;
            for (const QKeySequence& s : a->shortcuts()) {
                if (s == ks) { conflict = true; break; }
            }
            if (conflict) break;
        }
        if (!conflict) {
            auto* sc = new QShortcut(ks, this);
            sc->setContext(Qt::ApplicationShortcut);
            connect(sc, &QShortcut::activated, this, [this]() {
                if (m_actionHandler != nullptr) {
                    m_actionHandler->setCurrentAction(RS2::ActionLayerWalk);
                }
            });
        }
    }

    // Ctrl+Shift+Space: fuzzy-finder palette over recent files. Same conflict
    // check as Ctrl+Space — yield to any QAction that already owns the slot.
    {
        const QKeySequence ks(Qt::CTRL | Qt::SHIFT | Qt::Key_Space);
        bool conflict = false;
        for (QAction* a : findChildren<QAction*>()) {
            if (a == nullptr) continue;
            for (const QKeySequence& s : a->shortcuts()) {
                if (s == ks) { conflict = true; break; }
            }
            if (conflict) break;
        }
        if (!conflict) {
            auto* sc = new QShortcut(ks, this);
            sc->setContext(Qt::ApplicationShortcut);
            connect(sc, &QShortcut::activated, this, [this]() {
                showRecentFilesPalette();
            });
        }
    }

    // Ctrl+E: toggle MDI arrangement. If nothing's currently maximized,
    // tile all subwindows so the user can see every open drawing at once.
    // If anything is already maximized (i.e. the previous Ctrl+E tiled and
    // the user pressed it again), maximize the active subwindow instead.
    // Stateless heuristic - survives any window state changes the user makes
    // between presses. slotTile() lives on the menu only and has no QAction
    // in the action factory, so we install a window-level shortcut for it.
    {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, [this]() {
            QMdiArea* mdi = getMdiArea();
            if (mdi == nullptr) { slotTile(); return; }
            bool anyMaximized = false;
            for (QMdiSubWindow* sub : mdi->subWindowList()) {
                if (sub != nullptr && sub->isMaximized()) {
                    anyMaximized = true;
                    break;
                }
            }
            if (anyMaximized) {
                slotTile();
            } else if (QMdiSubWindow* active = mdi->activeSubWindow()) {
                active->showMaximized();
            } else {
                slotTile();
            }
        });
    }

    // Crash-recovery: defer until the event loop is running and the main
    // window has been shown, so the modal dialog has a real parent and
    // doesn't fight the startup-file-load path.
    QTimer::singleShot(0, this, &QC_ApplicationWindow::checkForCrashRecovery);
}
/**
 * Destructor.
 */
QC_ApplicationWindow::~QC_ApplicationWindow() {
    RS_DEBUG->print("QC_ApplicationWindow::~QC_ApplicationWindow");

#ifdef _WINDOWS
    qt_ntfs_permission_lookup--; // turn it off again
#endif

    delete m_dialogFactory;
    delete m_actionContext;

}

// Crash-recovery tracker. Stored in the INI group "Recovery" as
// a ';'-joined list of autosave file paths. Every autosave appends; clean exit
// (tryCloseAllBeforeExist success) clears the whole list. On startup we filter
// to entries that still exist on disk — a real Save deletes its autosave, so
// only crash artifacts survive — and prompt the user to recover them.
namespace {
    constexpr char kRecoveryGroup[] = "Recovery";
    constexpr char kRecoveryKey[] = "PendingAutosaves";
    constexpr QChar kRecoverySep = QLatin1Char(';');

    QStringList readPendingAutosaves() {
        const auto s = RS_Settings::createQSettings();
        s->beginGroup(kRecoveryGroup);
        const QString raw = s->value(kRecoveryKey).toString();
        s->endGroup();
        if (raw.isEmpty()) return {};
        return raw.split(kRecoverySep, Qt::SkipEmptyParts);
    }

    void writePendingAutosaves(const QStringList& paths) {
        const auto s = RS_Settings::createQSettings();
        s->beginGroup(kRecoveryGroup);
        s->setValue(kRecoveryKey, paths.join(kRecoverySep));
        s->endGroup();
    }
}

void QC_ApplicationWindow::addPendingAutosave(const QString& autosavePath) {
    if (autosavePath.isEmpty()) return;
    QStringList paths = readPendingAutosaves();
    if (paths.contains(autosavePath)) return;
    paths.append(autosavePath);
    writePendingAutosaves(paths);
}

void QC_ApplicationWindow::removePendingAutosave(const QString& autosavePath) {
    if (autosavePath.isEmpty()) return;
    QStringList paths = readPendingAutosaves();
    if (!paths.removeAll(autosavePath)) return;
    writePendingAutosaves(paths);
}

void QC_ApplicationWindow::clearPendingAutosaves() {
    const auto s = RS_Settings::createQSettings();
    s->beginGroup(kRecoveryGroup);
    s->remove(kRecoveryKey);
    s->endGroup();
}

void QC_ApplicationWindow::checkForCrashRecovery() {
    QStringList paths = readPendingAutosaves();
    QStringList survivors;
    for (const QString& p : paths) {
        if (QFileInfo::exists(p)) survivors.append(p);
    }
    // Persist the filtered list — stale entries (files cleaned up by a real
    // save) shouldn't keep haunting startup if the user dismisses the prompt.
    writePendingAutosaves(survivors);
    if (survivors.isEmpty()) return;

    const QString preview = survivors.size() <= 5
        ? survivors.join(QLatin1Char('\n'))
        : QStringList(survivors.mid(0, 5)).join(QLatin1Char('\n'))
              + QStringLiteral("\n  … and %1 more").arg(survivors.size() - 5);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Recover unsaved work?"));
    box.setText(tr("LibreCAD found %1 autosave file(s) from a previous session "
                   "that didn't shut down cleanly.").arg(survivors.size()));
    box.setInformativeText(preview);
    QPushButton* openBtn = box.addButton(tr("Open Autosaves"), QMessageBox::AcceptRole);
    QPushButton* discardBtn = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    box.addButton(tr("Decide Later"), QMessageBox::RejectRole);
    box.setDefaultButton(openBtn);
    box.exec();

    QAbstractButton* clicked = box.clickedButton();
    if (clicked == openBtn) {
        for (const QString& p : survivors) {
            if (QFileInfo::exists(p)) openFile(p);
        }
        // Successful open transfers responsibility to the normal save/close
        // cycle — drop the pending list so a subsequent clean save doesn't
        // re-trigger the prompt on next launch.
        clearPendingAutosaves();
    } else if (clicked == discardBtn) {
        for (const QString& p : survivors) {
            QFile::remove(p);
        }
        clearPendingAutosaves();
    }
    // "Decide Later": leave survivors in the list, prompt again next launch.
}

// fixme - should it be there or in persistence?
void QC_ApplicationWindow::startAutoSaveTimer(bool startAutoBackup) {
    if (startAutoBackup) {
        if (m_autosaveTimer == nullptr) {
            m_autosaveTimer = std::make_unique<QTimer>(this);
            m_autosaveTimer->setObjectName("autosave");
            connect(m_autosaveTimer.get(), &QTimer::timeout, this, &QC_ApplicationWindow::autoSaveCurrentDrawing);
        }
        if (!m_autosaveTimer->isActive()) {
            // autosaving has been turned on. Make a backup immediately
            LC_GROUP_GUARD("Defaults");
            {
                LC_SET("AutoBackupDocument", 1);
                autoSaveCurrentDrawing();
                int ms = 60000 * LC_GET_INT("AutoSaveTime", 5);
                m_autosaveTimer->start(ms);
            }
        }
    } else {
        if (m_autosaveTimer != nullptr) {
            m_autosaveTimer.reset();
        }
    }
}

/**
 * @brief QC_ApplicationWindow::getAppWindow() accessor for the application window singleton instance
 * @return QC_ApplicationWindow* the application window instance
 */
std::unique_ptr<QC_ApplicationWindow>& QC_ApplicationWindow::getAppWindow(){
    static auto instance = std::unique_ptr<QC_ApplicationWindow>(new QC_ApplicationWindow);
    // singleton could be reset: cannot be called after reseting
    Q_ASSERT(instance != nullptr);
    return instance;
}

void QC_ApplicationWindow::setupMDIWindowTitleByFile(QC_MDIWindow *w, const QString& drawingFileFullPath, bool draftMode, bool forPreview){
    QString fileName = getFileNameFromFullPath(drawingFileFullPath);
    QString baseName;
    if (forPreview) {
        baseName = tr("Print preview for %1").arg(fileName);
    }
    else {
        baseName = fileName;
    }
    setupMDIWindowTitleByName(w, baseName , draftMode);
}

void QC_ApplicationWindow::setupMDIWindowTitleByName(QC_MDIWindow *w, const QString& baseTitleStr, bool draftMode){
    auto title = baseTitleStr + "[*]";
    if (draftMode) {
        title = title + " [" + tr("Draft Mode") + "]";
    }
    w->setWindowTitle(title);
    // Mirror to the main application titlebar if this subwindow is active.
    if (w == m_activeMdiSubWindow) {
        updateApplicationTitleFor(w);
    }
}

void QC_ApplicationWindow::updateApplicationTitleFor(QC_MDIWindow *w) {
    const QString appName = qApp->applicationName();
    if (w == nullptr) {
        setWindowTitle(appName);
        setWindowModified(false);
        return;
    }
    // Reuse the subwindow's already-formatted title (filename + draft mode tag).
    // Strip the Qt "[*]" placeholder; we re-apply it at the end of the main
    // title so a single dirty marker tracks the active document.
    QString subTitle = w->windowTitle();
    subTitle.remove(QStringLiteral("[*]"));
    if (subTitle.isEmpty()) {
        subTitle = tr("Untitled");
    }
    setWindowTitle(subTitle + "[*] - " + appName);
    setWindowModified(w->isWindowModified());
}

/**
 * Force-Save(as) the content of the sub window.  Retry on failure.
 * @return true success (or window was not modified)
 * @return false user cancelled (or window was null)
 */
bool QC_ApplicationWindow::doSave(QC_MDIWindow * w, bool forceSaveAs) {
    if (w == nullptr) {
        return false;
    }
    if (w->isModified() || forceSaveAs) {
        QString drawingFileFullPath = w->getFileName();
        if (drawingFileFullPath.isEmpty()) {
            doActivate(w); // show the user the drawing for save as
        }
        QString msg = drawingFileFullPath.isEmpty() ? tr("Saving drawing...") : tr("Saving drawing: %1").arg(drawingFileFullPath);
        showStatusMessage(msg);
        bool cancelled;
        bool saved = forceSaveAs ? w->saveDocumentAs(cancelled) : w->saveDocument(cancelled);
        if (saved) {
            if (cancelled) {
                showStatusMessage(tr("Save cancelled"), 2000);
                return false;
            }

            drawingFileFullPath = w->getFileName();
            msg = tr("Saved drawing: %1").arg(drawingFileFullPath);
            notificationMessage(msg, 2000);

            m_recentFilesList->addIfAbsent(drawingFileFullPath);

            auto graphicView = w->getGraphicView();
            bool draftMode   = graphicView->isDraftMode();
            setupMDIWindowTitleByFile(w, drawingFileFullPath, draftMode, graphicView->isPrintPreview());

            bool autoBackup = LC_GET_ONE_BOOL("Defaults", "AutoBackupDocument", true);
            startAutoSaveTimer(autoBackup);
        } else {
            msg = tr("Cannot save the file ") + w->getFileName() + tr(" , please check the filename and permissions.");
            notificationMessage(msg, 2000);
            return doSave(w, true);
        }
    }
    return true;
}

void QC_ApplicationWindow::activeMDIWindowChanged(QC_MDIWindow *window){
    m_activeMdiSubWindow = window;
    updateApplicationTitleFor(window);
}

/**
 * Force-Close this sub window.
 * @param w
 * @param activateNext also activate the next window in the window_list, if any
 */
void QC_ApplicationWindow::doClose(QC_MDIWindow *w, bool activateNext) {
    w->getGraphicView()->killAllActions();

    QC_MDIWindow *parentWindow = w->getParentWindow();
    if (parentWindow != nullptr) {
        parentWindow->removeChildWindow(w);
    }

    auto graphic = w->getDocument()->getGraphic();
    if (graphic != nullptr) {
        auto view = w->getGraphicView();
        graphic->removeLayerListListener(view);
    }

    for (auto &&child : std::as_const(w->getChildWindows())) {// block editors and print previews; just force these closed
        doClose(child, false); // they belong to the document (changes already saved there)
    }
    w->getChildWindows().clear();
    // m_mdiAreaCAD->removeSubWindow(w);
    if (w->getSaveOnClosePolicy() == QC_MDIWindow::SaveOnClosePolicy::CANCEL) {
        // support for cancelling of saving untitled new document (via close all and close event)
        return;
    }
    w->close();
    m_windowList.removeOne(w);

    if (m_activeMdiSubWindow == nullptr || m_activeMdiSubWindow == w) {
        setupWidgetsByWindow(nullptr);
    }
    openedFiles.removeAll(w->getFileName());

    activeMDIWindowChanged(nullptr);
    m_actionHandler->setDocumentAndView(nullptr, nullptr);

    if (activateNext && !m_windowList.empty()) {
        if (parentWindow != nullptr) {
            doActivate(parentWindow);
        } else {
            doActivate(m_windowList.back());
        }
    }
    enableFileActions();
}

void QC_ApplicationWindow::enableFileActions() {
    if (m_windowList.isEmpty()) {
        enableFileActions(nullptr);
    }
}

// fixme - sand - files - change to signals?
void QC_ApplicationWindow::setupWidgetsByWindow(QC_MDIWindow *w) const {
    RS_GraphicView* gv = (w == nullptr) ? nullptr : w->getGraphicView();

    // fixme - sand - files - replace by updating list of instances, to simplify introduction of new widgets
    m_layerWidget->setGraphicView(gv);
    m_layerTreeWidget->setGraphicView(gv);
    m_namedViewsWidget->setGraphicView(gv);
    m_ucsListWidget->setGraphicView(gv);
    m_quickInfoWidget->setGraphicView(gv);
    m_anglesBasisWidget->setGraphicView(gv);
    m_penPaletteWidget->setGraphicView(gv);
    m_blockWidget->setGraphicView(gv);
    m_coordinateWidget->setGraphicView(gv);
    m_relativeZeroCoordinatesWidget->setGraphicView(gv);
    m_penToolBar->setGraphicView(gv);
    m_activeLayerNameWidget->setGraphicView(gv);
    m_selectionWidget->setGraphicView(gv);
    m_penWizard->setGraphicView(gv);
}

/**
 * Force-Activate this sub window.
 */
void QC_ApplicationWindow::doActivate(QMdiSubWindow *w) {
    LC_MDIApplicationWindow::doActivate(w);
    enableFileActions(qobject_cast<QC_MDIWindow *>(w));
    // fixme - sand - potentially, there we may just fire signal to widgets...
}

int QC_ApplicationWindow::showCloseDialog(QC_MDIWindow *w, bool showSaveAll) {
    return m_dlgHelpr->showCloseDialog(w, showSaveAll);
}

/**
 * Enable the available file actions for this sub-window.
 */
void QC_ApplicationWindow::enableFileActions(const QC_MDIWindow *w) {
    bool hasWindow = w != nullptr;
    QString fileName;
    if (hasWindow) {
        fileName = w->getFileName();
    }

    if (!hasWindow || fileName.isEmpty()) {
        getAction("FileSave")->setText(tr("&Save"));
        getAction("FileSaveAs")->setText(tr("Save &as..."));
    } else {
        QString name = getFileNameFromFullPath(fileName);
        getAction("FileSave")->setText(tr("&Save %1").arg(name));
        getAction("FileSaveAs")->setText(tr("Save %1 &as...").arg(name));
    }

    enableActions({
            "FileSave",
            "FileSaveAs",
            "FileExportMakerCam",
            "FilePrintPDF",
            "FileExport",
            "FilePrint",
            "FilePrintPreview",
            "FileClose"
        },hasWindow);

    enableActions({
        "FileSaveAll",
        "FileCloseAll"
        },hasWindow && m_windowList.count() > 1);
}

LC_ActionContext* QC_ApplicationWindow::getActionContext() const {
    return m_actionContext;
}

/**
 * Close Event. Called when the user tries to close the app.
 */
void QC_ApplicationWindow::closeEvent(QCloseEvent *ce) {
    tryCloseAllBeforeExist() ? ce->accept() : ce->ignore();
}

bool QC_ApplicationWindow::isAcceptableDragNDropFileName(const QString& fileName) {
    if (fileName.endsWith(R"(.dxf)", Qt::CaseInsensitive) ||
        fileName.endsWith(R"(.cxf)", Qt::CaseInsensitive) ||
        fileName.endsWith(R"(.lff)", Qt::CaseInsensitive)) {
        return QFileInfo::exists(fileName);
    }
    return false;
}

void QC_ApplicationWindow::dropEvent(QDropEvent *event) {
    event->acceptProposedAction();
    //limit maximum number of dropped files to be opened
    unsigned counts = 0;
    for (QUrl const &url: event->mimeData()->urls()) {
        const QString &fileName = url.toLocalFile();
        if (isAcceptableDragNDropFileName(fileName)) {
            openFile(fileName);
            if (++counts > 32) return;
        }
    }
}

void QC_ApplicationWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (QUrl const &url: event->mimeData()->urls()) {
            const QString &fileName = url.toLocalFile();
            if (isAcceptableDragNDropFileName(fileName)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void QC_ApplicationWindow::setPreviousZoomEnable(bool enable){
    m_previousZoomEnable=enable;
    enableAction("ZoomPrevious", enable);
}

void QC_ApplicationWindow::setUndoEnable(bool enable){
    m_undoEnable = enable;
    enableAction("EditUndo", enable);
}

void QC_ApplicationWindow::setRedoEnable(bool enable){
    m_redoEnable = enable;
    enableAction("EditRedo", enable);
}

void QC_ApplicationWindow::setSaveEnable(bool enable){
    enableAction("FileSave", enable);
}

void QC_ApplicationWindow::slotEnableActions(bool enable) {
    enableAction("ZoomPrevious", enable && m_previousZoomEnable);
    enableAction("EditUndo", enable && m_undoEnable);
    enableAction("EditRedo", enable && m_redoEnable);
}

// fixme - sand - rework, think about changed to signal from the widget?
void QC_ApplicationWindow::slotUpdateActiveLayer() {
    if (m_layerWidget != nullptr && m_activeLayerNameWidget != nullptr) {
        m_activeLayerNameWidget->activeLayerChanged(m_layerWidget->getActiveName());
    }
}

/**
 * Initializes the global application settings from the
 * config file (unix, mac) or registry (windows).
 */
void QC_ApplicationWindow::initSettings() {
    RS_DEBUG->print("QC_ApplicationWindow::initSettings()");

    bool first_load = LC_GET_ONE_BOOL("Startup","FirstLoad", true);
    if (!first_load) {
        m_workspacesInvoker->init();
    }
    fireWorkspacesChanged();
    m_styleHelper->loadFromSettings();
    LC_GROUP("Appearance");
    {
        QAction *viewLinesDraftAction = getAction("ViewLinesDraft");
        viewLinesDraftAction->setChecked(LC_GET_BOOL("DraftLinesMode", false));

        bool draftMode = LC_GET_BOOL("DraftMode", false);

        getAction("ViewDraft")->setChecked(draftMode);
        viewLinesDraftAction->setDisabled(draftMode);

        QAction* viewAntialiasing = getAction("ViewAntialiasing");
        bool antialiasing = LC_GET_BOOL("Antialiasing", false);
        viewAntialiasing->setChecked(antialiasing);

        // Restore the toolbar lock state. Setting the action's checked state
        // without emitting `triggered` (setChecked does NOT emit triggered);
        // we apply the lock to all existing toolbars directly.
        bool lockToolbars = LC_GET_BOOL("LockToolbars", false);
        getAction("ViewLockToolbars")->setChecked(lockToolbars);
        applyToolbarsLocked(lockToolbars);
    }
    LC_GROUP_END();
    m_infoCursorSettingsManager->loadFromSettings();
    // Startup dock-state restore is finished — from here on, dock changes are
    // the user's and should be persisted immediately.
    m_dockPersistReady = true;
}

/**
 * Stores the global application settings to file or registry.
 */
void QC_ApplicationWindow::storeSettings() {
    if (RS_Settings::save_is_allowed) {
       m_workspacesInvoker->persist();
       m_penPaletteWidget->persist();
       // fixme - sand - decided whether shortcuts should be also saved... This may be necessary if
       // the path for settins was changed.
       // m_actionGroupManager->persist();
    }
}

void QC_ApplicationWindow::persistDockLayout() {
    // Gated until startup restore completes so restore-time visibility changes
    // are not written back as the "user's" layout.
    if (!m_dockPersistReady || !RS_Settings::save_is_allowed) {
        return;
    }
    LC_GROUP("Geometry");
    {
        LC_SET("StateOfWidgets", QString(saveState().toBase64(QByteArray::Base64Encoding)));
        LC_SET("WindowGeometry", QString(saveGeometry().toBase64(QByteArray::Base64Encoding)));
    }
    LC_GROUP_END();
}

void QC_ApplicationWindow::slotKillAllActions() {
    QC_MDIWindow* win  = getCurrentMDIWindow();
    if (win != nullptr) {
        RS_GraphicView* gv = win->getGraphicView();
        if (gv != nullptr) {
            gv->switchToDefaultAction();
            auto doc = gv->getContainer();
            if (doc != nullptr) {
                const RS_EntityContainer::LC_SelectionInfo &selectionInfo = doc->getSelectionInfo();

                m_actionContext->updateSelectionWidget((int)selectionInfo.count, selectionInfo.length);
                // m_selectionWidget->setNumber((int)selectionInfo.count);
                // m_selectionWidget->setTotalLength(selectionInfo.length);
            }
        }
    }
}

/**
 * Sets the keyboard focus on the command line.
 */
void QC_ApplicationWindow::slotFocusCommandLine() {
    // if command widget is not visible - show it first
    auto* cmd_dockwidget = findChild<QDockWidget*>("command_dockwidget");
    if (cmd_dockwidget->isHidden()) {
        cmd_dockwidget->show();
    }
    m_commandWidget->setFocus();
}

void QC_ApplicationWindow::slotFocusOptionsWidget(){
    // fixme - sand - files - fix for mor reliable focus settings
    if (m_toolOptionsToolbar != nullptr){
        m_toolOptionsToolbar->setFocus();
    }
}

/**
 * Shows the given error on the command line.
 */
void QC_ApplicationWindow::slotError(const QString& msg) {
  m_commandWidget->appendHistory(msg);
}

void QC_ApplicationWindow::slotShowDrawingOptions() {
    m_actionHandler->setCurrentAction(RS2::ActionOptionsDrawingGrid);
}

void QC_ApplicationWindow::slotShowDrawingOptionsUnits() {
    m_actionHandler->setCurrentAction(RS2::ActionOptionsDrawingUnits);
}

/**
 * Hands focus back to the application window. In the rare event
 * of a escape press from the layer widget (e.g after switching desktops
 * in XP).
 */
void QC_ApplicationWindow::slotFocus() {
    setFocus();
}

void QC_ApplicationWindow::disableUIForAbsentDrawing() {
    enableWidgets(false);
    enableWidgetList(false, {
                         m_layerTreeWidget,
                         m_layerWidget,
                         m_commandWidget
                     });
    m_snapToolBar->getSnapOptionsHolder()->hideSnapOptions();
    m_coordinateWidget->clearContent();
    m_relativeZeroCoordinatesWidget->clearContent();
}

/**
 * Called when a document window was activated.
 */
void QC_ApplicationWindow::doWindowActivated(QMdiSubWindow *w, bool forced) {

    if (w == nullptr) { // when it may occur???
        disableUIForAbsentDrawing();
        // todo - check which other widgets in status bar or so should be cleared if no files..
        emit windowsChanged(false);
        activeMDIWindowChanged(nullptr);
        return;
    }

    if (w == m_activeMdiSubWindow) {
        // this may occur after file open, so additional update is needed :(
        RS_GraphicView* activatedGraphicView = getCurrentGraphicView();
        if (activatedGraphicView != nullptr) {
            RS_Graphic* activatedGraphic = activatedGraphicView->getGraphic();
            if (activatedGraphic != nullptr) {
                bool printPreview = activatedGraphicView->isPrintPreview();
                if (!printPreview){
                    bool isometricGrid = activatedGraphic->isIsometricGrid();
                    RS2::IsoGridViewType isoViewType = activatedGraphic->getIsoView();
                    updateGridViewActions(isometricGrid, isoViewType);
                }
                activatedGraphicView->loadSettings();
                activatedGraphicView->redraw();
            }
        }
        return;
    }

    // kill active actions in previous windows.that will prevent the situation described by issue #1762 with
    // non-finished action started on previous window and action that is active with UI still checked  after window switch
    doForEachSubWindowGraphicView([](QG_GraphicView *graphicView,[[maybe_unused]] QC_MDIWindow *sw){
        RS_ActionInterface *ai = graphicView->getCurrentAction();
        if (ai != nullptr) {
            ai->hideOptions();
            ai->hideSnapOptions();
            // actually, this is a "brute force" approach for now.
            // todo - more intelligent approach is for sure to uncheck the action (for action in progress in  old
            // todo window without killing action and restore the action toggle state on return to that view.
            // todo hawever, it seems that will require some additional relation between action an QAction and
            // todo support from ActionHandler
            // fixme - return to this later
            graphicView->killAllActions();
        }
    });

    auto windowActivated = dynamic_cast<QC_MDIWindow *>(w);
    activeMDIWindowChanged(windowActivated);
    enableFileActions(windowActivated);

    bool hasDocumentInActivatedWindow = false;

    RS_Document *activatedDocument = windowActivated->getDocument();
    if (activatedDocument != nullptr) {
        hasDocumentInActivatedWindow         = true;
        QG_GraphicView *activatedGraphicView = windowActivated->getGraphicView();
        activatedGraphicView->loadSettings();

        RS_Graphic *activatedGraphic = windowActivated->getGraphic();
        RS_Units::setCurrentDrawingUnits(activatedDocument->getGraphic()->getUnit());

        setupWidgetsByWindow(windowActivated);

        // Update all inserts in this graphic (blocks might have changed):
        activatedDocument->updateInserts();
        // whether to enable undo/redo buttons
        activatedDocument->updateUndoState();

        QAction *lockRelZeroAction = m_actionGroupManager->getActionByName("LockRelativeZero");
        if (lockRelZeroAction != nullptr){
            bool locked = activatedGraphicView->getViewPort()->isRelativeZeroLocked();
            lockRelZeroAction->setChecked(locked);
        }

        activatedGraphicView->redraw();

        // set pen from pen toolbar
        slotPenChanged(m_penToolBar->getPen());

        if (!forced) {
            // update toggle button status:
            emit gridChanged(activatedGraphic->isGridOn());
        }

        m_actionHandler->setDocumentAndView(activatedDocument, activatedGraphicView);

        RS_ActionInterface *currentAction = activatedGraphicView->getCurrentAction();
        if (currentAction != nullptr) {
            currentAction->showOptions();
        }

        bool printPreview  = activatedGraphicView->isPrintPreview();
        if (!printPreview){
            bool isometricGrid = activatedGraphic->isIsometricGrid();
            RS2::IsoGridViewType isoViewType = activatedGraphic->getIsoView();
            updateGridViewActions(isometricGrid, isoViewType);
            m_actionHandler->setSnaps(m_snapToolBar->getSnaps());
        }

        updateActionsAndWidgetsForPrintPreview(printPreview);
    }

    // Disable/Enable menu and toolbar items
    emit windowsChanged(hasDocumentInActivatedWindow);
}

/**
 * Called when the menu 'workspaces' is about to be shown.
 * This is used to update the window list in the menu.
 */
void QC_ApplicationWindow::slotWorkspacesMenuAboutToShow() {
    m_menuFactory->onWorkspaceMenuAboutToShow(m_windowList);
}

QMenu* QC_ApplicationWindow::createGraphicViewContentMenu(QMouseEvent* event, QG_GraphicView* view, RS_Entity* entity, const RS_Vector& pos) {
    QStringList actions;
    bool mayInvokeDefaultMenu = m_creatorInvoker->getMenuActionsForMouseEvent(event, entity, actions);
    return m_menuFactory->createGraphicViewPopupMenu(view, entity, pos, actions, mayInvokeDefaultMenu);
}


/**
 * Called when the user selects a document window from the
 * window list.
 */
void QC_ApplicationWindow::slotWindowsMenuActivated(bool /*id*/) {
    int ii = qobject_cast<QAction*>(sender())->data().toInt();
    QMdiSubWindow* w = m_mdiAreaCAD->subWindowList().at(ii);
    if (w != nullptr && w != m_mdiAreaCAD->activeSubWindow()) {
		doActivate(w);
    }
}

/**
 * Called when something changed in the pen toolbar
 * (e.g. color, width, style).
 */
void QC_ApplicationWindow::slotPenChanged(const RS_Pen& pen) {
    QC_MDIWindow *w = getCurrentMDIWindow();
    if (w != nullptr) {
        w->slotPenChanged(pen);
    }
}

/**
 * Called when the user changes a pen attribute (color/width/linetype) in the pen
 * toolbar. Applies just that attribute to the currently selected entities, with undo.
 * Does nothing when there is no selection (the active pen for new entities is handled
 * separately by slotPenChanged()).
 */
void QC_ApplicationWindow::applyPenAttributeToSelection(const RS_Pen& pen, bool color, bool width, bool lineType) {
    QC_MDIWindow* w = getCurrentMDIWindow();
    if (w == nullptr) {
        return;
    }
    RS_Document* doc = w->getDocument();
    QG_GraphicView* view = w->getGraphicView();
    if (doc == nullptr || view == nullptr) {
        return;
    }
    if (doc->countSelected() == 0) {
        return;
    }
    RS_AttributesData data;
    data.pen = pen;
    data.changeColor = color;
    data.changeWidth = width;
    data.changeLineType = lineType;
    RS_Modification m(*doc, view->getViewPort());
    m.changeAttributes(data, true);
    view->redraw(RS2::RedrawDrawing);
}

void QC_ApplicationWindow::applyLayerToSelection(RS_Layer* layer) {
    if (layer == nullptr) {
        return;
    }
    QC_MDIWindow* w = getCurrentMDIWindow();
    if (w == nullptr) {
        return;
    }
    RS_Document* doc = w->getDocument();
    QG_GraphicView* view = w->getGraphicView();
    if (doc == nullptr || view == nullptr) {
        return;
    }
    if (doc->countSelected() == 0) {
        return;
    }
    RS_AttributesData data;
    data.layer = layer->getName();
    data.changeLayer = true;
    // Also reset the pen to ByLayer (AutoCAD SETBYLAYER) so the moved objects
    // actually take on the layer's colour/width/linetype. Without this, an
    // explicit colour (e.g. imported colour 7 = white) would mask the layer.
    data.pen.setColor(RS_Color(RS2::FlagByLayer));
    data.pen.setWidth(RS2::WidthByLayer);
    data.pen.setLineType(RS2::LineByLayer);
    data.changeColor = true;
    data.changeWidth = true;
    data.changeLineType = true;
    RS_Modification m(*doc, view->getViewPort());
    m.changeAttributes(data, true);
    view->redraw(RS2::RedrawDrawing);
}


QC_MDIWindow *QC_ApplicationWindow::createNewDrawingWindow(RS_Document *doc, const QString& expectedFileName) {
    static unsigned id = 0;
    id++;

    auto *w = new QC_MDIWindow(doc, m_mdiAreaCAD, false, m_actionContext);
    QG_GraphicView* view = setupNewGraphicView(w);

    m_actionHandler->setDocumentAndView(w->getDocument(), view);

    QString baseTitleString;
    if (w->getDocument()->rtti() == RS2::EntityBlock) {
        baseTitleString = tr("Block '%1'").arg(expectedFileName);
    } else {
        if (expectedFileName.isEmpty()) {
            baseTitleString = tr("unnamed document %1").arg(id);
        }
        else {
            baseTitleString = getFileNameFromFullPath(expectedFileName);
        }
    }

    bool draftMode = LC_GET_ONE_BOOL("Appearance", "DraftMode", false);
    view->setDraftMode(draftMode);

    setupMDIWindowTitleByName(w, baseTitleString, draftMode);
    w->setWindowIcon(QIcon(":/icons/document.lci"));

    // fixme - sand- where that listeners are removed?
    RS_Graphic *graphic = w->getDocument()->getGraphic();
    if (graphic != nullptr) {
        graphic->addLayerListListener(view);
    }

    m_windowList << w;
    m_mdiAreaCAD->addSubWindow(w);
    return w;
}

QG_GraphicView* QC_ApplicationWindow::setupNewGraphicView(const QC_MDIWindow* w) {
    QG_GraphicView* view = w->getGraphicView();
    LC_GROUP("Appearance");
    bool antialiasing = LC_GET_BOOL("Antialiasing"); // fixme - sand - check whether its not loaded in loadSettings() later
    bool showScrollbars = LC_GET_BOOL("ScrollBars", true);
    bool cursor_hiding = LC_GET_BOOL("cursor_hiding");
    LC_GROUP_END();

    view->setAntialiasing(antialiasing);
    view->setCursorHiding(cursor_hiding);
    view->setDeviceName(LC_GET_ONE_STR("Hardware","Device", "Mouse"));
    if (showScrollbars) {
        view->addScrollbars();
    }

    connect(view, &QG_GraphicView::gridStatusChanged, this, &QC_ApplicationWindow::updateGridStatus);
    connect(view, &RS_GraphicView::currentActionChanged, this, &QC_ApplicationWindow::onViewCurrentActionChanged);

    return view;
}


bool QC_ApplicationWindow::newDrawingFromTemplate(const QString &fileName, QC_MDIWindow *w) {
    bool ret = false;
    RS2::FormatType type = RS2::FormatDXFRW;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    showStatusMessage(tr("Creating new file..."));
    w = createNewDrawingWindow(nullptr, "");
    qApp->processEvents(QEventLoop::AllEvents, 1000);

    bool noFile = fileName.isEmpty();
    if (noFile) {
        ret = true;
    } else {
        // loads the template file in the new view:
        ret = w->loadDocumentFromTemplate(fileName, type);
    }

    if (ret) {
        updateCoordinateWidgetFormat();
        doActivate(w);
        doArrangeWindows(RS2::CurrentMode);
        autoZoomAfterLoad(w->getGraphicView());
        if (!noFile) {
            QString message = tr("New document from template: ") + fileName;
            notificationMessage(message, 2000);
        }
        else {
            showStatusMessage(tr("New Drawing created."), 2000);
        }
        auto graphic = w->getGraphic();
        if (graphic != nullptr) {
            if (noFile) {
                graphic->applyNewDrawingDimDefaults();
                // indicate that loading is completed so we could update default dim style from vars
                graphic->onLoadingCompleted();
            }
            emit(gridChanged(graphic->isGridOn()));
        }
    }

    QApplication::restoreOverrideCursor();
    return ret;
}

/**
 * Menu file -> New (using a predefined Template).
 */
void QC_ApplicationWindow::slotFileNewFromDefaultTemplate() {
    //tried to load template file indicated in RS_Settings
    QString templateFileName = LC_GET_ONE_STR("Paths","Template", "");
    newDrawingFromTemplate(templateFileName);
}

void QC_ApplicationWindow::slotFileNewFromTemplate() {
    QString fileName = m_dlgHelpr->requestDrawingFileName().first;
    if (fileName.isEmpty()) {
        showStatusMessage(tr("Select Template aborted"), 2000);
        return;
    }

    // Create new document window:
    QMdiSubWindow *old = m_activeMdiSubWindow;
    QRect geo;
    bool maximized = false;
    if (old != nullptr) {//save old geometry
        geo = old->geometry();
        maximized = old->isMaximized();
    }
    QC_MDIWindow *w = nullptr;
    if (!newDrawingFromTemplate(fileName, w)) {
        // error
        QString msg = tr("Cannot open the file\n%1\nPlease check the permissions.").arg(fileName);
        m_commandWidget->appendHistory(msg);
        QMessageBox::information(this, QMessageBox::tr("Warning"),msg, QMessageBox::Ok);
        //file opening failed, clean up QC_MDIWindow and QMdiSubWindow
        if (w != nullptr) {
            slotFilePrintPreview(false); // fixme - sand  why it's there?
            doClose(w); //force closing, without asking user for confirmation
        }
        QMdiSubWindow *activeWindow = m_mdiAreaCAD->currentSubWindow();

        // activeMDIWindowChanged(w);
        // m_activeMdiSubWindow = nullptr; //to allow reactivate the previous active

        if (activeWindow != nullptr) {//restore old geometry
            m_mdiAreaCAD->setActiveSubWindow(activeWindow);
            activeWindow->raise();
            activeWindow->setFocus();
            if (old == nullptr || maximized) {
                activeWindow->showMaximized();
            } else {
                activeWindow->setGeometry(geo);
            }
        }
        RS_DEBUG->print("QC_ApplicationWindow::slotFileNewTemplate: load Template failed");
    } else
        RS_DEBUG->print("QC_ApplicationWindow::slotFileNewTemplate() OK");
}

/**
 * Menu file -> open.
 */
void QC_ApplicationWindow::slotFileOpen() {
    QPair<QString, RS2::FormatType> info = m_dlgHelpr->requestDrawingFileName(RS2::FormatUnknown);
    QString fileName = info.first;
    if (!fileName.isEmpty()) {
        openFile(fileName, info.second);
    }
}

void QC_ApplicationWindow::slotEditActiveBlock(){
    QC_MDIWindow* parent = getCurrentMDIWindow();
    if (parent == nullptr) {
        return;
    }

    // If block is opened from another block the parent must be set
    // to graphic that contain all these blocks.
    if (parent->getDocument()->rtti() == RS2::EntityBlock) {
        parent = parent->getParentWindow();
    }

    //get blocklist from block widget, bug#3497154
    RS_BlockList* blockList = m_blockWidget->getBlockList();

    if (blockList == nullptr)
        return;

    RS_Block* activeBlock = blockList->getActive();
    if (activeBlock == nullptr) {
        return;
    }

    QC_MDIWindow* blockWindow = getWindowWithDoc(activeBlock);
    if (blockWindow != nullptr) {
       m_mdiAreaCAD ->setActiveSubWindow(blockWindow);
    } else {
        QC_MDIWindow* w = createNewDrawingWindow(activeBlock, activeBlock->getName());
        setupWidgetsByWindow(w);
        parent->addChildWindow(w);
        qApp->processEvents(QEventLoop::AllEvents, 1000);
        // the parent needs a pointer to the block window and vice versa
        updateCoordinateWidgetFormat();
        doActivate(w);
        doArrangeWindows(RS2::CurrentMode);

        QG_GraphicView *graphicView = w->getGraphicView();
        graphicView->zoomAuto();
    }
}

void QC_ApplicationWindow::slotFileOpenRecent(const QAction *action){
    auto variant = action->data();
    if (variant.isValid()) {
        showStatusMessage(tr("Opening recent file..."));
        QString fileName = variant.toString();
        openFile(fileName, RS2::FormatUnknown);
    }
}

QString QC_ApplicationWindow::getFileNameFromFullPath(const QString &path) {
    QFileInfo info(path);
    return info.fileName();
}

void QC_ApplicationWindow::updateCoordinateWidgetFormat(){
    m_coordinateWidget->setCoordinates({0.0, 0.0}, {0.0, 0.0}, true);
}

void QC_ApplicationWindow::updateWidgetsAsDocumentLoaded(const QC_MDIWindow *w){
    m_layerWidget->slotUpdateLayerList();
    m_layerWidget->activateLayer(0);
    m_layerTreeWidget->slotFilteringMaskChanged();
    m_namedViewsWidget->reload();
    m_ucsListWidget->reload();
    m_quickInfoWidget->updateFormats();

    auto graphic = w->getGraphic();
    if (graphic != nullptr) {
        if (int objects_removed = graphic->clean()) {
            auto msg = QObject::tr("Invalid objects removed:");
            m_commandWidget->appendHistory(msg + " " + QString::number(objects_removed));
        }
        emit(gridChanged(graphic->isGridOn()));

        m_anglesBasisWidget->update(graphic);
    }

    // update coordinate widget format:
    updateCoordinateWidgetFormat();
}

void QC_ApplicationWindow::autoZoomAfterLoad(QG_GraphicView *graphicView){
    if (LC_GET_ONE_BOOL("CADPreferences", "AutoZoomDrawing", true)) {
        graphicView->zoomAuto(false);
    }
}

int QC_ApplicationWindow::maybeSurfaceBlocksDock(RS_Graphic *graphic) {
  if (graphic == nullptr || m_blockWidget == nullptr)
    return 0;
  if (graphic->countDeep() != 0)
    return 0;

  RS_BlockList *blockList = m_blockWidget->getBlockList();
  if (blockList == nullptr)
    return 0;

  int hits = 0;
  for (int i = 0; i < blockList->count(); ++i) {
    RS_Block *block = blockList->at(i);
    if (block == nullptr || block->isUndone())
      continue;
    // *Model_Space / *Paper_Space[N] are pseudo-blocks that mirror the
    // ENTITIES section; surfacing them is pointless.
    if (block->getName().startsWith('*'))
      continue;
    if (block->countDeep() > 0)
      ++hits;
  }
  if (hits == 0)
    return 0;

  if (auto *dock = qobject_cast<QDockWidget *>(m_blockWidget->parentWidget())) {
    dock->show();
    dock->raise();
    if (dock->isFloating())
      dock->activateWindow();
  }
  return hits;
}

void QC_ApplicationWindow::openFile(const QString &fileName, RS2::FormatType type) {
    if (!QFileInfo::exists(fileName)) {
        m_commandWidget->appendHistory(tr("File '%1' does not exist. Opening aborted").arg(fileName));
        showStatusMessage(tr("Opening aborted"), 2000);
        return;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (openedFiles.indexOf(fileName) >= 0) {
        QString message = tr("Warning: File already opened : ") + fileName;
        notificationMessage(message, 2000);
    }

    // Create new document window:
    auto w = createNewDrawingWindow(nullptr, fileName);
    qApp->processEvents(QEventLoop::AllEvents, 1000);

    // open the file in the new view:
    bool success = false;
    if (QFileInfo::exists(fileName)) {
        success = w->loadDocument(fileName, type);
    } else {
        QString msg = tr("Cannot open the file\n%1\nPlease check its existence and permissions.").arg(fileName);
        m_commandWidget->appendHistory(msg);
        QMessageBox::information(this, QMessageBox::tr("Warning"), msg, QMessageBox::Ok);
    }
    if (!success) {
        // error
        QApplication::restoreOverrideCursor();

        //file opening failed, clean up QC_MDIWindow and QMdiSubWindow
        slotFilePrintPreview(false);
        doClose(w); //force closing, without asking user for confirmation
        return;
    }

    // update recent files menu:
    m_recentFilesList->add(fileName);
    openedFiles.push_back(fileName);

    if (m_mdiAreaCAD->viewMode() == QMdiArea::TabbedView) {
        QList<QTabBar *> tabBarList = m_mdiAreaCAD->findChildren<QTabBar *>();
        QTabBar *tabBar = tabBarList.at(0);
        if (tabBar) {
            tabBar->setExpanding(false);
            tabBar->setTabToolTip(tabBar->currentIndex(), fileName);
        }
    } else {
        doArrangeWindows(RS2::CurrentMode);
    }

    doActivate(w);

    updateWidgetsAsDocumentLoaded(w);

    auto graphicView = w->getGraphicView();
    autoZoomAfterLoad(graphicView);

    int blocksWithGeometry = maybeSurfaceBlocksDock(w->getGraphic());
    QString message;
    int messageTimeout = 0;
    if (blocksWithGeometry > 0) {
      message = tr("Loaded %1 — modelspace is empty; %n block(s) in the Blocks "
                   "dock contain geometry.",
                   "", blocksWithGeometry)
                    .arg(fileName);
      messageTimeout = 8000;
    } else {
      message = tr("Loaded document: ") + fileName;
      messageTimeout = 2000;
    }
    notificationMessage(message, messageTimeout);

    QApplication::restoreOverrideCursor();
}

void QC_ApplicationWindow::changeDrawingOptions(int tabToShow){
    auto graphicView = getCurrentGraphicView();
    RS_Graphic* graphic = graphicView->getGraphic(true);

    int dialogResult = m_dlgHelpr->requestOptionsDrawingDialog(*graphic, tabToShow);
    if (dialogResult == QDialog::Accepted) {
        updateCoordinateWidgetFormat();
        m_quickInfoWidget->updateFormats();
        m_anglesBasisWidget->update(graphic);
        graphicView->loadSettings();
        graphic->update();
        graphicView->redraw();
        graphicView->repaint();
        // fixme - sand - emit signal?
    } else {
    }
}

void QC_ApplicationWindow::openFile(const QString &fileName) {
    openFile(fileName, RS2::FormatUnknown);
}

// ---------------------------------------------------------------------------
// Single-instance support
// ---------------------------------------------------------------------------
//
// Goal: double-clicking a .dxf in Explorer while LibreCAD is already running
// must (1) NOT spawn a second process, (2) open the file as a new MDI subwindow
// inside the existing instance, and (3) raise + activate the existing window.
//
// Wire is a QLocalSocket (named pipe on Windows, Unix socket elsewhere).
// Payload is UTF-8 file paths separated by `\n`. A trailing `\n` flushes the
// last path. An empty payload means "just raise the main window."
//
// The client side lives in main.cpp (it must run before heavy GUI startup so
// the secondary process can exit fast). See main.cpp for the connect / forward.

QString QC_ApplicationWindow::singleInstanceSocketName() {
    // Per-user key so multiple users on the same machine don't collide.
    QByteArray user = qgetenv("USERNAME");                 // Windows
    if (user.isEmpty()) user = qgetenv("USER");            // POSIX
    if (user.isEmpty()) user = "default";
    return QStringLiteral("LibreCAD_singleinstance_v1_") + QString::fromLocal8Bit(user);
}

void QC_ApplicationWindow::startSingleInstanceServer(const QString &socketName) {
    if (m_singleInstanceServer != nullptr) return;          // already running

    // Stale-socket cleanup: removeServer is a no-op when nothing's there, and
    // safely deletes a leftover Unix socket from a crashed previous run.
    QLocalServer::removeServer(socketName);

    m_singleInstanceServer = new QLocalServer(this);
    // World-readable but not world-writable - default access on Windows; on
    // POSIX we set UserAccessOption so other users can't shove paths at us.
    m_singleInstanceServer->setSocketOptions(QLocalServer::UserAccessOption);

    if (!m_singleInstanceServer->listen(socketName)) {
        // Listen failed; surface but don't crash startup. App keeps working
        // as a non-single-instance build.
        qWarning() << "Single-instance server failed to listen on"
                   << socketName << ":" << m_singleInstanceServer->errorString();
        delete m_singleInstanceServer;
        m_singleInstanceServer = nullptr;
        return;
    }

    connect(m_singleInstanceServer, &QLocalServer::newConnection,
            this, &QC_ApplicationWindow::onSingleInstanceConnection);
}

void QC_ApplicationWindow::onSingleInstanceConnection() {
    while (m_singleInstanceServer != nullptr
           && m_singleInstanceServer->hasPendingConnections()) {
        QLocalSocket *sock = m_singleInstanceServer->nextPendingConnection();
        if (sock == nullptr) continue;
        // Drain the full payload. Client writes paths, then closes, so we
        // wait for disconnect (bounded - 2s is plenty for a few file names).
        sock->waitForReadyRead(2000);
        // In case more arrives between waitForReadyRead and read:
        while (sock->bytesAvailable() == 0 && sock->state() == QLocalSocket::ConnectedState) {
            if (!sock->waitForReadyRead(500)) break;
        }
        const QByteArray payload = sock->readAll();
        // Tail of payload may arrive after a small delay; absorb that too.
        while (sock->waitForReadyRead(100)) {
            // accumulate
        }
        QByteArray more = sock->readAll();
        QByteArray full = payload + more;
        sock->disconnectFromServer();
        sock->deleteLater();

        const QStringList paths = QString::fromUtf8(full)
                                      .split(QChar('\n'), Qt::SkipEmptyParts);
        for (const QString &p : paths) {
            const QString trimmed = p.trimmed();
            if (!trimmed.isEmpty()) {
                openFileOrSwitchToTab(trimmed);
            }
        }

        // Always raise/activate, even if payload was empty (lets a bare second
        // launch act as "bring window forward").
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        show();
        raise();
        QWidget::activateWindow();   // disambiguate from our QMdiSubWindow* overload
        // On Windows the calls above only flash the taskbar for a background
        // process; force the native foreground so the window actually comes up.
        LC_Foreground::raiseToForeground(winId());
    }
}

void QC_ApplicationWindow::openFileOrSwitchToTab(const QString &filePath) {
    QFileInfo fi(filePath);
    QString canonical = fi.canonicalFilePath();
    if (canonical.isEmpty()) {
        // File may not exist - fall back to absolute path so openFile() can
        // surface the right error to the user.
        canonical = fi.absoluteFilePath();
    }

    QMdiArea *mdi = getMdiArea();
    if (mdi != nullptr) {
        const auto subs = mdi->subWindowList();
        for (QMdiSubWindow *sub : subs) {
            auto *win = qobject_cast<QC_MDIWindow*>(sub);
            if (win == nullptr) continue;
            const QString openName = win->getFileName();
            if (openName.isEmpty()) continue;
            const QString openCanon = QFileInfo(openName).canonicalFilePath();
            const QString cmpA = openCanon.isEmpty() ? openName : openCanon;
            if (QString::compare(cmpA, canonical, Qt::CaseInsensitive) == 0) {
                doActivate(sub);
                return;
            }
        }
    }
    openFile(canonical);
}

namespace {
// Case-insensitive substring filter. Subsequence fuzzy was tried here and
// produced way too many false positives — recent-file entries include the
// full directory path (often deeply nested), and a 5-letter query against
// 100+ characters of path almost always has SOME scattered subsequence
// match. Substring is what users actually expect from "type to filter": if
// the typed string appears literally anywhere in the displayed text (file
// name OR directory), it matches.
bool fuzzyFileMatch(const QString& pattern, const QString& target) {
    if (pattern.isEmpty()) return true;
    return target.contains(pattern, Qt::CaseInsensitive);
}

// Forwards Up/Down/PgUp/PgDn/Home/End from the search QLineEdit to whichever
// item view drives the recent-files list, so arrow nav works without leaving
// the search field. Works for both QListWidget and QTableWidget (QWidget*).
class RecentNavFilter : public QObject {
public:
    RecentNavFilter(QWidget* view, QObject* parent) : QObject(parent), m_view(view) {}
protected:
    bool eventFilter(QObject*, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            switch (ke->key()) {
                case Qt::Key_Up: case Qt::Key_Down:
                case Qt::Key_PageUp: case Qt::Key_PageDown:
                case Qt::Key_Home: case Qt::Key_End:
                    QCoreApplication::sendEvent(m_view, ke);
                    return true;
                default: break;
            }
        }
        return false;
    }
private:
    QWidget* m_view;
};
} // namespace

void QC_ApplicationWindow::showRecentFilesPalette() {
    // Visible diagnostic so we can tell whether the shortcut fires and the
    // function reaches this point at all. If you see this message in the
    // status bar but no dialog, the QDialog::exec() is failing; if you don't
    // see it, the Ctrl+Shift+Space QShortcut never activated.
    if (statusBar() != nullptr) {
        statusBar()->showMessage(
            tr("Recent-files palette: %1 entries")
                .arg(m_recentFilesList ? m_recentFilesList->count() : -1),
            3000);
    }
    if (m_recentFilesList == nullptr) return;

    // Snapshot of currently-open files (path -> QMdiSubWindow*). Lets us mark
    // each recent-file entry as "open in tab" and drive cheap tab-switch
    // previews when the user arrows over an already-loaded file.
    QHash<QString, QMdiSubWindow*> openByCanon;
    if (auto* mdi = getMdiArea()) {
        for (QMdiSubWindow* sub : mdi->subWindowList()) {
            auto* w = qobject_cast<QC_MDIWindow*>(sub);
            if (w == nullptr) continue;
            const QString p = w->getFileName();
            if (p.isEmpty()) continue;
            const QString canon = QFileInfo(p).canonicalFilePath();
            openByCanon.insert(canon.isEmpty() ? p : canon, sub);
        }
    }

    // Build entries in MRU order (newest first). QG_RecentFiles stores newest
    // at the back of its list, so we walk descending.
    struct Entry {
        QString path;
        QMdiSubWindow* openWindow;  // nullptr if not currently loaded
    };
    QList<Entry> entries;
    for (int i = m_recentFilesList->count() - 1; i >= 0; --i) {
        const QString path = m_recentFilesList->get(i);
        if (path.isEmpty()) continue;
        const QString canon = QFileInfo(path).canonicalFilePath();
        QMdiSubWindow* open = openByCanon.value(canon.isEmpty() ? path : canon, nullptr);
        entries.append({path, open});
    }
    if (entries.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Recent Files"));
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);

    auto* search = new QLineEdit(&dlg);
    search->setPlaceholderText(tr("Filter recent files..."));
    search->setClearButtonEnabled(true);

    // "Preview" toggles whether arrowing over a row auto-loads that file so it
    // shows behind the palette. Loading parses the whole DXF, so on big files it
    // stutters on every keystroke - this lets the user turn it off. The choice is
    // persisted so it survives across sessions. Default on preserves prior behavior.
    auto* previewCb = new QCheckBox(tr("Preview"), &dlg);
    const bool previewEnabled = LC_GET_ONE_BOOL("RecentFilesPalette", "Preview", true);
    previewCb->setChecked(previewEnabled);
    previewCb->setToolTip(tr("Auto-load the highlighted file as a preview. "
                             "Turn off to avoid stutter on large drawings."));

    auto* searchRow = new QHBoxLayout();
    searchRow->addWidget(search, /*stretch=*/1);
    searchRow->addWidget(previewCb);
    layout->addLayout(searchRow);

    auto* list = new QTableWidget(&dlg);
    list->setColumnCount(3);
    list->setHorizontalHeaderLabels({tr("Name"), tr("Modified"), tr("Path")});
    list->setSelectionBehavior(QAbstractItemView::SelectRows);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setAlternatingRowColors(false);   // per project UI rules
    list->setTextElideMode(Qt::ElideMiddle); // keep root + filename visible on long paths
    list->setShowGrid(false);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->verticalHeader()->setVisible(false);
    list->verticalHeader()->setDefaultSectionSize(16); // tight rows
    list->horizontalHeader()->setStretchLastSection(true);
    list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    list->horizontalHeader()->setSortIndicatorShown(true);
    list->setSortingEnabled(false); // enable AFTER populating so insert order = MRU
    list->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const Entry& e = entries.at(i);
        const QFileInfo fi(e.path);
        const QString prefix   = e.openWindow != nullptr ? QStringLiteral("● ") : QStringLiteral("  ");
        const QString basename = fi.fileName();
        const QString dirPath  = fi.absolutePath();
        const QDateTime mtime  = fi.lastModified();

        auto* nameItem = new QTableWidgetItem(prefix + basename);
        nameItem->setData(Qt::UserRole, i);  // index into entries
        nameItem->setToolTip(e.path);

        // Store the QDateTime under EditRole so sort-by-Modified is
        // chronological, not lexical. Display column gets a readable string.
        auto* dateItem = new QTableWidgetItem();
        const QString dateStr = mtime.isValid()
                                    ? mtime.toString(QStringLiteral("yyyy-MM-dd hh:mm"))
                                    : QString();
        dateItem->setData(Qt::DisplayRole, dateStr);
        dateItem->setData(Qt::EditRole,    mtime);
        dateItem->setToolTip(dateStr);

        auto* pathItem = new QTableWidgetItem(dirPath);
        pathItem->setToolTip(dirPath);

        list->setItem(i, 0, nameItem);
        list->setItem(i, 1, dateItem);
        list->setItem(i, 2, pathItem);
    }
    list->setSortingEnabled(true);

    layout->addWidget(list, /*stretch=*/1);

    // Right-click row → context menu (Open Containing Folder, Copy Path).
    QObject::connect(list, &QWidget::customContextMenuRequested, &dlg,
        [list, &entries](const QPoint& pos) {
            QTableWidgetItem* hit = list->itemAt(pos);
            if (hit == nullptr) return;
            QTableWidgetItem* nameItem = list->item(hit->row(), 0);
            if (nameItem == nullptr) return;
            const int idx = nameItem->data(Qt::UserRole).toInt();
            if (idx < 0 || idx >= entries.size()) return;
            const QString path = entries.at(idx).path;
            const QString dir  = QFileInfo(path).absolutePath();

            QMenu menu;
            QAction* openFolder = menu.addAction(tr("Open Containing &Folder"));
            QAction* copyPath   = menu.addAction(tr("Copy &Path"));
            QAction* chosen = menu.exec(list->viewport()->mapToGlobal(pos));
            if (chosen == openFolder) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
            } else if (chosen == copyPath) {
                QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path));
            }
        });

    // Snapshot the active tab when the palette opens so Esc can revert. Any
    // tabs WE auto-loaded as previews get torn down silently on Esc — the
    // user never sees a "save changes?" prompt for files they were merely
    // peeking at.
    QC_MDIWindow* originalActive = getCurrentMDIWindow();

    // Preview behaviour:
    //   * Highlighted file is already in a tab → switch to it.
    //   * Highlighted file isn't loaded → load it as a "preview tab", evicting
    //     any prior preview so at most one preview ever exists.
    //   * Originally-open tabs are never closed by preview navigation.
    QC_MDIWindow* previewTab = nullptr;   // window WE opened, may evict

    auto silentClose = [this](QC_MDIWindow* w) {
        if (w == nullptr) return;
        // Belt-and-braces: clear the document's modified flag AND set the
        // policy so neither closeWindow() nor QC_MDIWindow::closeEvent() pops
        // a save dialog. A freshly opened file shouldn't be modified, but
        // some load paths set the flag (DXF format up-conversion, our own
        // auto-DIMSCALE, etc.). Either step alone is sufficient; together
        // they're robust against future changes to the close-prompt logic.
        if (auto* doc = w->getDocument()) doc->setModified(false);
        w->setSaveOnClosePolicy(QC_MDIWindow::SaveOnClosePolicy::DONT_SAVE);
        closeWindow(w);
    };

    auto closePreviewTab = [&entries, &previewTab, silentClose]() {
        if (previewTab == nullptr) return;
        QMdiSubWindow* asSub = qobject_cast<QMdiSubWindow*>(previewTab);
        // Clear any entry pointing at this window so a later navigation
        // re-loads from disk instead of pointing at a freed pointer.
        if (asSub != nullptr) {
            for (Entry& en : entries) {
                if (en.openWindow == asSub) en.openWindow = nullptr;
            }
        }
        silentClose(previewTab);
        previewTab = nullptr;
    };

    auto previewByRow = [this, &entries, &list, &previewTab, &closePreviewTab, previewCb]() {
        // Preview disabled: don't auto-load anything on navigation - the whole
        // point is to avoid parsing big DXFs on every keystroke. The file still
        // loads on Enter via the accept path below.
        if (!previewCb->isChecked())
            return;
        const int row = list->currentRow();
        if (row < 0) {
            if (statusBar()) statusBar()->showMessage(tr("Preview: no current item"), 2000);
            return;
        }
        QTableWidgetItem* nameItem = list->item(row, 0);
        if (nameItem == nullptr) return;
        const int idx = nameItem->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= entries.size()) {
            if (statusBar()) statusBar()->showMessage(tr("Preview: invalid idx %1").arg(idx), 2000);
            return;
        }
        Entry& e = entries[idx];

        // Already-loaded files: just switch tabs. The originally-open tab is
        // untouched. If we had previously auto-loaded a different preview,
        // evict it now since we're moving off it.
        if (e.openWindow != nullptr) {
            if (statusBar()) statusBar()->showMessage(
                tr("Preview: switch to open tab: %1").arg(QFileInfo(e.path).fileName()), 2000);
            if (previewTab != nullptr &&
                qobject_cast<QMdiSubWindow*>(previewTab) != e.openWindow) {
                closePreviewTab();
            }
            doActivate(e.openWindow);
            return;
        }

        // Unloaded files: auto-load every selection so the user always sees a
        // preview of whatever's highlighted (per the user's spec — no size
        // cap). Evict the prior preview first so we never accumulate tabs.
        QFileInfo fi(e.path);
        if (!fi.exists()) {
            if (statusBar()) statusBar()->showMessage(
                tr("Preview: file missing: %1").arg(e.path), 3000);
            return;
        }

        if (statusBar()) statusBar()->showMessage(
            tr("Preview: loading %1").arg(QFileInfo(e.path).fileName()), 3000);
        closePreviewTab();
        QC_MDIWindow* before = getCurrentMDIWindow();
        openFile(e.path);
        QC_MDIWindow* after = getCurrentMDIWindow();
        if (after != nullptr && after != before) {
            previewTab = after;
            e.openWindow = qobject_cast<QMdiSubWindow*>(after);
        }
    };
    // NOTE: the currentCellChanged connection is intentionally deferred until
    // after the initial setCurrentCell() call below. The initial selection of
    // row 0 used to fire previewByRow, which auto-loaded the topmost recent
    // file — if that file was large the palette stalled the app on every open.
    // Connecting after the initial selection means the first preview only
    // fires when the user actively navigates (arrow keys, click, or filter).

    // Substring filter on every keystroke. Hides rows whose Name OR Path
    // cell doesn't contain the pattern (case-insensitive). After filtering
    // we resnap selection to the first visible row so preview tracks what
    // the user can actually see.
    QObject::connect(search, &QLineEdit::textChanged, &dlg,
        [list](const QString& pattern) {
            int firstVisible = -1;
            for (int row = 0; row < list->rowCount(); ++row) {
                const QString name = list->item(row, 0) != nullptr
                                         ? list->item(row, 0)->text() : QString();
                const QString path = list->item(row, 2) != nullptr
                                         ? list->item(row, 2)->text() : QString();
                const bool match = fuzzyFileMatch(pattern, name) ||
                                   fuzzyFileMatch(pattern, path);
                list->setRowHidden(row, !match);
                if (match && firstVisible < 0) firstVisible = row;
            }
            if (firstVisible >= 0) {
                list->setCurrentCell(firstVisible, 0);
                list->scrollToItem(list->item(firstVisible, 0));
            } else {
                list->clearSelection();
            }
        });

    // Enter on the search box accepts. (QLineEdit doesn't consume the key, so
    // a default button would also work, but we don't have one here.)
    QObject::connect(search, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    // Double-clicking a row is the mouse equivalent of Enter.
    QObject::connect(list, &QTableWidget::itemActivated, &dlg,
        [&dlg](QTableWidgetItem*) { dlg.accept(); });

    search->installEventFilter(new RecentNavFilter(list, &dlg));
    search->setFocus();

    // Default selection: first row. Set BEFORE wiring currentCellChanged so
    // the initial highlight doesn't trigger an auto-load of the first file.
    if (list->rowCount() > 0) {
        list->setCurrentCell(0, 0);
    }
    QObject::connect(list, &QTableWidget::currentCellChanged, &dlg,
        [previewByRow](int, int, int, int) { previewByRow(); });

    // Persist the Preview choice immediately; when turned back on, preview the
    // currently-highlighted row so the toggle feels live.
    QObject::connect(previewCb, &QCheckBox::toggled, &dlg,
        [previewByRow](bool checked) {
            LC_SET_ONE("RecentFilesPalette", "Preview", checked);
            if (checked) previewByRow();
        });

    {
        // Per-column widths + sort order — captured by QHeaderView::saveState
        // so the user's "Name column was perfect" setting comes back.
        LC_GROUP_GUARD("WindowGeometry");
        const QByteArray headerState = LC_GET_BARRAY("RecentFilesPaletteHeader");
        if (!headerState.isEmpty()) {
            list->horizontalHeader()->restoreState(headerState);
        }
    }
    // The filename is the whole point of this column, so never let it elide:
    // widen to fit the longest entry. Must run AFTER restoreState(), which
    // would otherwise stamp a stale narrow width over any earlier auto-fit.
    // A user-dragged width wider than the content is preserved.
    const int savedNameWidth = list->columnWidth(0);
    list->resizeColumnToContents(0);
    list->setColumnWidth(0, std::max(savedNameWidth, list->columnWidth(0) + 24));
    // Anchored to the main window so the palette follows it across monitors.
    LC_Dialog::loadRelativeGeometry(&dlg, "RecentFilesPalette", QSize(820, 480));

    const int result = dlg.exec();

    LC_Dialog::saveRelativeGeometry(&dlg, "RecentFilesPalette");
    {
        LC_GROUP_GUARD("WindowGeometry");
        RS_SETTINGS->writeEntry(QStringLiteral("RecentFilesPaletteHeader"),
                                QVariant(list->horizontalHeader()->saveState()));
    }

    if (result == QDialog::Accepted) {
        // Enter — commit. Whatever's currently active (the preview tab or an
        // already-open file we switched to) stays. If the selection is some-
        // how an unloaded entry that never got previewed (shouldn't normally
        // happen with auto-preview on selection change), load it now.
        const int row = list->currentRow();
        if (row >= 0) {
            QTableWidgetItem* nameItem = list->item(row, 0);
            if (nameItem != nullptr) {
                const int idx = nameItem->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < entries.size()) {
                    const Entry& e = entries.at(idx);
                    if (e.openWindow == nullptr) {
                        openFileOrSwitchToTab(e.path);
                    }
                }
            }
        }
    } else {
        // Esc — revert. Tear down any preview tab we loaded (silently, no
        // save prompt) and put the user back on whichever tab they were on
        // before the palette opened.
        closePreviewTab();
        if (originalActive != nullptr) {
            doActivate(qobject_cast<QMdiSubWindow*>(originalActive));
        }
    }
}

/**
 * Menu file -> save.
 */
void QC_ApplicationWindow::slotFileSave() {
    if (doSave(getCurrentMDIWindow())) {
        m_recentFilesList->updateRecentFilesMenu();
    }
}

/**
 * Menu file -> save as.
 */
void QC_ApplicationWindow::slotFileSaveAs() {
    if (doSave(getCurrentMDIWindow(), true)) {
        m_recentFilesList->updateRecentFilesMenu();
    }
}

bool QC_ApplicationWindow::doSaveAllFiles(){
    QC_MDIWindow *current = getCurrentMDIWindow();
    bool result{true};
    for (auto w: m_windowList) {
        if (w != nullptr && w->isModified()) {
            result = doSave(w);
            if (!result) {
                showStatusMessage(tr("Save All cancelled"), 2000);
                break;
            }
        }
    }
    doActivate(current);
    m_recentFilesList->updateRecentFilesMenu();
    return result;
}

void QC_ApplicationWindow::slotFileSaveAll(){
    doSaveAllFiles();
}

/**
 * Autosave.
 */
void QC_ApplicationWindow::autoSaveCurrentDrawing() {
    RS_DEBUG->print("QC_ApplicationWindow::slotFileAutoSave(): begin");
    if (!LC_GET_ONE_BOOL("Defaults", "AutoBackupDocument", true)) {
        startAutoSaveTimer(false);
        return;
    }
    showStatusMessage(tr("Auto-saving drawing..."), 2000);

    QC_MDIWindow *w = getCurrentMDIWindow();
    if (w != nullptr) {
        QString autosaveFileName;
        if (w->autoSaveDocument(autosaveFileName)) {
            showStatusMessage(tr("Auto-saved drawing"), 2000);
            addPendingAutosave(autosaveFileName);
        } else {
            // error
            m_autosaveTimer->stop();
            QMessageBox::information(this, QMessageBox::tr("Warning"),
                                     tr("Cannot auto-save the file\n%1\nPlease check the permissions.\n"
                                        "Auto-save disabled.").arg(autosaveFileName),QMessageBox::Ok);
            showStatusMessage(tr("Auto-saving failed"), 2000);
        }
    }
}

void QC_ApplicationWindow::showStatusMessage(const QString& msg, int timeout) const {
    statusBar()->showMessage(msg, timeout);
}

void QC_ApplicationWindow::notificationMessage(const QString& msg, int timeout) const {
    statusBar()->showMessage(msg, timeout);
    bool duplicateMessageInCmdWidget = true; // fixme - sand - complete - setting? Rework later with cmd
    if (duplicateMessageInCmdWidget){
        m_commandWidget->appendHistory(msg);
    }
}

void QC_ApplicationWindow::initCompleted() {
    enableFileActions();
}

void QC_ApplicationWindow::slotFileExport() {
    auto *w = getCurrentMDIWindow();
    if (w != nullptr) {
        auto graphic = w->getGraphic();
        if (graphic != nullptr) {
            QString currentDocumentFileName = w->getFileName();
            LC_ExportToImageService exportService(this, m_dlgHelpr.get());
            exportService.exportGraphicsToImage(graphic, currentDocumentFileName);
        }
    }
}

/**
 * Called when a sub window is about to close.
 * If modified, show the Save/Close/Cancel dialog, then do the request.
 * If a save is needed but the user cancels, the window is not closed.
 */
void QC_ApplicationWindow::closeWindow(QC_MDIWindow *win) {
   if (win != nullptr) {
       bool cancel = false;
       bool hasParent = win->getParentWindow() != nullptr;
       if (win->isModified() && !hasParent) {  // fixme - sand - files - simplify the logic there
           switch (showCloseDialog(win)) {
               case QG_ExitDialog::Save:
                   cancel = !doSave(win);
               break;
               case QG_ExitDialog::Cancel:
                   cancel = true;
               break;
               default:
                   break;
           }
       }
       if (!cancel) {
           doClose(win);
           doArrangeWindows(RS2::CurrentMode);
       }
   }
}

bool QC_ApplicationWindow::doCloseAllFiles(){
    // When more than one modified document is open, show a single unified dialog
    // with checkboxes so the user doesn't get N pop-ups in a row.
    {
        QList<QC_MDIWindow*> modified;
        for (auto w : m_windowList) {
            if (w != nullptr && w->isModified() && w->getParentWindow() == nullptr) {
                modified.append(w);
            }
        }
        if (modified.size() > 1) {
            LC_UnsavedChangesDialog dlg(modified, this);
            if (dlg.exec() != QDialog::Accepted) {
                showStatusMessage(tr("Close All cancelled"), 2000);
                return true;
            }
            QList<QC_MDIWindow*> toSave;
            switch (dlg.outcome()) {
                case LC_UnsavedChangesDialog::SaveAll:  toSave = modified; break;
                case LC_UnsavedChangesDialog::SaveSome: toSave = dlg.getCheckedWindows(); break;
                case LC_UnsavedChangesDialog::SaveNone: break;
                default:
                    showStatusMessage(tr("Close All cancelled"), 2000);
                    return true;
            }
            // Mark every window's policy up front so the per-window loop below
            // skips the legacy per-doc QG_ExitDialog entirely.
            for (auto w : modified) {
                w->setSaveOnClosePolicy(toSave.contains(w)
                    ? QC_MDIWindow::SaveOnClosePolicy::SAVE
                    : QC_MDIWindow::SaveOnClosePolicy::DONT_SAVE);
            }
            for (auto w : toSave) {
                doActivate(w);
                if (!doSave(w)) {
                    showStatusMessage(tr("Close All cancelled"), 2000);
                    return true;
                }
            }
            for (auto w : m_windowList) {
                if (w == nullptr) continue;
                doClose(w);
                if (w->getSaveOnClosePolicy() == QC_MDIWindow::SaveOnClosePolicy::CANCEL) {
                    showStatusMessage(tr("Close All cancelled"), 2000);
                    return true;
                }
                doArrangeWindows(RS2::CurrentMode);
                qApp->processEvents(QEventLoop::AllEvents, 1000);
            }
            return false;
        }
    }

    bool hasParent(false);
    QC_MDIWindow::SaveOnClosePolicy policy = QC_MDIWindow::SaveOnClosePolicy::ASK;
    for (auto w: m_windowList) {
        if (w) {
            hasParent = w->getParentWindow() != nullptr;
            if (w->isModified() && !hasParent && policy  == QC_MDIWindow::SaveOnClosePolicy::ASK) {
                doActivate(w);
                switch (showCloseDialog(w, m_windowList.count() > 1)) {
                    case QG_ExitDialog::DontSaveAll:
                        policy = QC_MDIWindow::SaveOnClosePolicy::DONT_SAVE;
                        break;
                    case QG_ExitDialog::DontSave:
                        w->setSaveOnClosePolicy(QC_MDIWindow::SaveOnClosePolicy::DONT_SAVE);
                        break;
                    case QG_ExitDialog::SaveAll:
                        policy = QC_MDIWindow::SaveOnClosePolicy::SAVE;
                        break;
                    case QG_ExitDialog::Save: {
                        w->setSaveOnClosePolicy(QC_MDIWindow::SaveOnClosePolicy::SAVE);
                        if (!doSave(w)) {
                            showStatusMessage(tr("Close All cancelled"), 2000);
                            return true;
                        }
                        break;
                    }
                    case QG_ExitDialog::Cancel:
                        showStatusMessage(tr("Close All cancelled"), 2000);
                        return true;
                    default:
                        break;
                }
            }
            if (policy != QC_MDIWindow::SaveOnClosePolicy::ASK) {
                w->setSaveOnClosePolicy(policy);
            }
            doClose(w);
            if (w->getSaveOnClosePolicy() == QC_MDIWindow::SaveOnClosePolicy::CANCEL) {
                // this may occur if window contains new untitled document. In such case, the user will be
                // prompted to enter the name of the document via save dialog - and here it is possible to cancel
                // save - so cancelling save for new doc affects the process of all windows save.
                showStatusMessage(tr("Close All cancelled"), 2000);
                return true;
            }
            doArrangeWindows(RS2::CurrentMode);
        }
        qApp->processEvents(QEventLoop::AllEvents, 1000);
    }
    return false;
}

/**
 * File > Close All - loop through all open windows, and close them.
 * Prompt user to save changes for modified documents.  If the user cancels
 * the remaining unsaved documents will not be closed.
 *
 * @return true success
 * @return false the user cancelled.
 */
void QC_ApplicationWindow::slotFileCloseAll() {
    doCloseAllFiles();
}

void QC_ApplicationWindow::slotFilePrintPDF() {
    slotFilePrint(true);
}

/**
 * Menu file -> print.
 */
void QC_ApplicationWindow::slotFilePrint(bool printPDF) {
    QC_MDIWindow *w = getCurrentMDIWindow();
    if (w  == nullptr) {
        return;
    }

    // Avoid printing without print preview
    if (!w->getGraphicView()->isPrintPreview()) {
        slotFilePrintPreview(true);
        return;
    }

    RS_Graphic *graphic = w->getDocument()->getGraphic();
    if (graphic != nullptr) {
        showStatusMessage(tr("Printing..."));
        using namespace LC_Printing;
        PrinterType type = printPDF ? PrinterType::PDF : PrinterType::Printer;
        LC_Printing::Print(*w, type);
        showStatusMessage(tr("Printing complete"), 2000);
    }
}

bool QC_ApplicationWindow::closePrintPreview(QC_MDIWindow *parent){
    QG_GraphicView *graphicView = parent->getGraphicView();
    if (graphicView->isPrintPreview()) {
        graphicView->hideOptions();
        RS_DEBUG->print("QC_ApplicationWindow::slotFilePrintPreview(): close");
        updateActionsAndWidgetsForPrintPreview(false);
        doClose(parent);
        doArrangeWindows(RS2::CurrentMode);
        return true;
    }
    return false;
}

void QC_ApplicationWindow::openPrintPreview(QC_MDIWindow *parent){
    // look for an existing print preview:
    QC_MDIWindow *existingPrintPreview = parent->getPrintPreview();

    if (existingPrintPreview != nullptr) {
        doActivate(existingPrintPreview);
        doArrangeWindows(RS2::CurrentMode);
        updateActionsAndWidgetsForPrintPreview(true);
    } else {
        if (!parent->getGraphicView()->isPrintPreview()) {
            auto *w = new QC_MDIWindow(parent->getDocument(), m_mdiAreaCAD, true, m_actionContext);
            m_mdiAreaCAD->addSubWindow(w);
            parent->addChildWindow(w);

            bool draftMode = LC_GET_ONE_BOOL("Appearance","DraftMode");
            setupMDIWindowTitleByFile(w, parent->getFileName(), draftMode, true);

            w->setWindowIcon(QIcon(":/icons/document.lci"));
            QG_GraphicView *view = w->getGraphicView();
            view->setDeviceName(LC_GET_ONE_STR("Hardware","Device", "Mouse"));
            //                gv->setBackground(RS_Color(255, 255, 255));
            view->setDefaultAction(new RS_ActionPrintPreview(m_actionContext)); // fixme - sand - is it correct for preview?

            connect(view, &RS_GraphicView::currentActionChanged, this, &QC_ApplicationWindow::onViewCurrentActionChanged);

            // only graphics offer block lists, blocks don't
            /*RS_DEBUG->print("  adding listeners");*/

            doActivate(w);
            doArrangeWindows(RS2::CurrentMode);

            view->zoomAuto(false);
            RS_Graphic *graphic = w->getDocument()->getGraphic();
            if (graphic != nullptr) {
                bool bigger = graphic->isBiggerThanPaper();
                bool fixed  = graphic->getPaperScaleFixed();

                graphic->fitToPage();

                // Calling zoomPage() after fitToPage() always fits
                // preview paper in preview window. The only reason not
                // to call zoomPage() is when drawing is bigger than paper,
                // plus it is fixed. In that case, not calling zoomPage()
                // prevents displaying empty paper (when drawing is actually
                // outside the paper and the preview window) and displays
                // full drawing and smaller paper inside it.
                if (bigger && fixed) {
                    RS_DEBUG->print("%s: don't call zoomPage()", __func__);
                } else {
                    RS_DEBUG->print("%s: call zoomPage()", __func__);
                    view->getViewPort()->zoomPage();
                }
            }
            updateActionsAndWidgetsForPrintPreview(true);
        }
    }
}

void QC_ApplicationWindow::slotFilePrintPreview(bool on) {
    QC_MDIWindow *parent = getCurrentMDIWindow();
    if (parent == nullptr) {
        return;
    }

    if (on) {
        openPrintPreview(parent);
    }
    else {
        RS_DEBUG->print("QC_ApplicationWindow::slotFilePrintPreview(): off");
        closePrintPreview(parent);
    }
}

/**
 * Menu file -> quit.
 */
void QC_ApplicationWindow::slotFileQuit() {
    showStatusMessage(tr("Exiting application..."));
    qApp->quit();  // signal handler closeEvent() will take care of modifications
}

/**
 * Shows / hides the grid.
 *
 * @param toggle true: show, false: hide.
 */
void QC_ApplicationWindow::slotViewGrid(bool toggle) {
    QC_MDIWindow *m = getCurrentMDIWindow();
    if (m != nullptr) {
        RS_Graphic *g = m->getGraphic();
        if (g != nullptr) {
            g->setGridOn(toggle);
            // Capture as the global default so every other drawing the user
            // opens picks up the same visibility.
            g->saveGlobalGridDefaults();
        }
    }
    updateGrids();
    redrawAll();
}

/**
 * Enables / disables the draft mode.
 *
 * @param toggle true: enable, false: disable.
 */
void QC_ApplicationWindow::slotViewDraft(bool toggle) {
    LC_SET_ONE("Appearance","DraftMode", toggle);
    // fixme - sand - files - probably just rely on signal??

    doForEachWindowGraphicView([toggle, this](QG_GraphicView *gv, QC_MDIWindow* w){ // fixme - sand - files - probably just rely on signal??
        gv->setDraftMode(toggle);
        QString fileName = w->getFileName();
        setupMDIWindowTitleByFile(w, fileName, toggle, gv->isPrintPreview());
   });
    emit draftChanged(toggle);
    redrawAll();
}

void QC_ApplicationWindow::slotShowEntityDescriptionOnHover(bool toggle) {
    doForEachWindowGraphicView([toggle](QG_GraphicView *gv, [[maybe_unused]] QC_MDIWindow* w){ // fixme - sand - files - probably just rely on signal??
        gv->setShowEntityDescriptionOnHover(toggle);
    });
    emit showEntityDescriptionOnHoverChanged(toggle);
    redrawAll();
}

void QC_ApplicationWindow::slotViewDraftLines(bool toggle) {
    LC_SET_ONE("Appearance","DraftLinesMode", toggle);
    doForEachWindowGraphicView([toggle](QG_GraphicView *gv, [[maybe_unused]]QC_MDIWindow* w){ // fixme - sand - files - probably just rely on signal??
        gv->setDraftLinesMode(toggle);
    });
    emit draftLinesChanged(toggle);
    redrawAll();
}

void QC_ApplicationWindow::slotViewAntialiasing(bool toggle) {
    LC_SET_ONE("Appearance","Antialiasing", toggle);

    doForEachSubWindowGraphicView([toggle](QG_GraphicView *gv, [[maybe_unused]] QC_MDIWindow *w){ // fixme - sand - files - probably just rely on signal??
        gv->setAntialiasing(toggle);
    });
    emit antialiasingChanged(toggle);
    redrawAll();
}

/**
 * Updates all grids of all graphic views.
 */
void QC_ApplicationWindow::updateGrids() {
    doForEachSubWindowGraphicView([](QG_GraphicView *gv, [[maybe_unused]] QC_MDIWindow *w){
            gv->loadSettings();
            gv->redraw(RS2::RedrawGrid);
    });
}

/**
 * Shows / hides the status bar.
 *
 * @param toggle true: show, false: hide.
 */
void QC_ApplicationWindow::slotViewStatusBar(bool toggle) {
    statusBar()->setVisible(toggle);
    LC_SET_ONE("Appearance", "StatusBarVisible", toggle);
}

void QC_ApplicationWindow::applyToolbarsLocked(bool locked) {
    // findChildren walks the whole QObject tree, so every toolbar - whether it
    // currently lives in a dock area, is floating, or is hidden - is updated.
    const auto bars = findChildren<QToolBar*>();
    for (QToolBar *tb : bars) {
        if (tb != nullptr) tb->setMovable(!locked);
    }
}

/**
 * Menu: View -> Lock Toolbars. Hides / restores the grip handle on every
 * QToolBar so the user can't accidentally drag a toolbar out of its dock.
 * State is persisted under Appearance/LockToolbars and re-applied on startup
 * by initSettings().
 */
void QC_ApplicationWindow::slotViewLockToolbars(bool toggle) {
    applyToolbarsLocked(toggle);
    LC_SET_ONE("Appearance", "LockToolbars", toggle);
}

/**
 * Dimensions menu: "Set Dimension Font...". One-click shortcut that pops the
 * Dimension Style Manager dialog already on the Text tab, focused on the font
 * picker. Edits the active document's default dim style; on Accept, dims
 * using that style pick up the new font on the next redraw automatically
 * because RS_Dimension::createDimTextData reads style->text()->style() live.
 */
void QC_ApplicationWindow::slotDimSetFont() {
    QC_MDIWindow *win = getCurrentMDIWindow();
    if (win == nullptr) {
        showStatusMessage(tr("Open a drawing first to change its dimension font."), 4000);
        return;
    }
    RS_Graphic *graphic = win->getGraphic();
    if (graphic == nullptr) return;

    const QString name = graphic->getDefaultDimStyleName();
    LC_DimStyle *style = graphic->getDimStyleByName(name, RS2::EntityDimLinear);
    if (style == nullptr) {
        showStatusMessage(tr("No default dimension style available to edit."), 4000);
        return;
    }

    LC_DlgDimStyleManager dlg(this, style, graphic, RS2::EntityDimLinear);
    dlg.setWindowTitle(tr("Set Dimension Font - %1").arg(name));
    dlg.refreshPreview();
    dlg.focusFontPicker();
    if (dlg.exec() == QDialog::Accepted) {
        // The style was edited in place; redraw so dimensions pick it up now.
        redrawAll();
    }
}

void QC_ApplicationWindow::slotViewGridOrtho(bool toggle) {
     m_gridViewInvoker->setGridView(toggle, false, RS2::IsoGridViewType::IsoLeft);
}

void QC_ApplicationWindow::slotViewGridIsoLeft(bool toggle) {
     m_gridViewInvoker->setGridView(toggle, true, RS2::IsoGridViewType::IsoLeft);
}

void QC_ApplicationWindow::slotViewGridIsoRight(bool toggle) {
     m_gridViewInvoker->setGridView(toggle, true, RS2::IsoGridViewType::IsoRight);
}

void QC_ApplicationWindow::slotViewGridIsoTop(bool toggle) {
     m_gridViewInvoker->setGridView(toggle, true, RS2::IsoGridViewType::IsoTop);
}

void QC_ApplicationWindow::updateGridViewActions(bool isometric, RS2::IsoGridViewType type) const{
    m_gridViewInvoker->updateGridViewActions(isometric, type);
}

void QC_ApplicationWindow::slotOptionsShortcuts() {
    LC_ActionsShortcutsDialog dlg(this, m_actionGroupManager.get());
    dlg.exec();
}

void QC_ApplicationWindow::rebuildMenuIfNecessary(){
    m_menuFactory->recreateMainMenuIfNeeded(menuBar());
}

/**
 * Shows the dialog for general application preferences.
 */
void QC_ApplicationWindow::slotOptionsGeneral() {
    int dialogResult = m_dlgHelpr->showGeneralOptionsDialog();
    if (dialogResult == QDialog::Accepted){
        m_actionOptionsManager->update();
        // fixme - check this signal, probably it's better to rely on settings change
        bool hideRelativeZero = LC_GET_ONE_BOOL("Appearance", "hideRelativeZero");
        emit signalEnableRelativeZeroSnaps(!hideRelativeZero);

        bool antialiasing = LC_GET_ONE_BOOL("Appearance", "Antialiasing", false);
        emit antialiasingChanged(antialiasing);

        m_statusbarManager->loadSettings();
        onCADTabBarIndexChanged(0); // force update if settings changed

        doForEachSubWindowGraphicView([this](QG_GraphicView *gv, const QC_MDIWindow* w){
            gv->loadSettings();
            if (w == m_activeMdiSubWindow) {
                gv->redraw();
            }
        });

        // fixme - sand - consider emitting signal on properties change instead of processing changes there
        m_infoCursorSettingsManager->loadFromSettings();
        rebuildMenuIfNecessary();
    }
    fireCurrentActionIconChanged(nullptr);
}

void QC_ApplicationWindow::slotImportBlock() {
    if (getCurrentMDIWindow() == nullptr) {
        return;
    }

    QString dxfPath  = m_dlgHelpr->requestDrawingFileName().first;
    if (dxfPath.isEmpty()) {
        return;
    }

    // fixme - sand - files - rework - provide the user with error info?
    if (QFileInfo(dxfPath).isReadable()) {
        if (m_actionHandler!=nullptr) {
		std::shared_ptr<RS_ActionInterface> a =
                m_actionHandler->setCurrentAction(RS2::ActionLibraryInsert);
            if (a) {
                auto action = static_cast<RS_ActionLibraryInsert*>(a.get());
                action->setFile(dxfPath);
            } else {
                RS_DEBUG->print(RS_Debug::D_ERROR,"QC_ApplicationWindow::slotImportBlock:"
                                "Cannot create action RS_ActionLibraryInsert");
            }
        }
    } else {
        RS_DEBUG->print(RS_Debug::D_ERROR,
                        "QC_ApplicationWindow::slotImportBlock: Can't read file: '%s'", dxfPath.toLatin1().data());
    }
}

void QC_ApplicationWindow::openFilesOnStartup(QStringList &fileList) {
    m_lastFilesOpener->openLastOpenFiles(fileList);
}

bool QC_ApplicationWindow::tryCloseAllBeforeExist() {
    m_lastFilesOpener->collectFilesList(m_windowList, m_activeMdiSubWindow);
    bool mayExit = !doCloseAllFiles();

    if (mayExit) {
        m_lastFilesOpener->saveSettings();
        storeSettings();
        clearPendingAutosaves();
    }
    return mayExit;
}

/**
 * Handle hotkeys. Don't let it to the default handler of Qt.
 * it will consume them also if a text field is active
 * which means it's impossible to enter a command.
 */
void QC_ApplicationWindow::keyPressEvent(QKeyEvent *e) {
    int key = e->key();
    switch (key) {
        case Qt::Key_Escape: {
            // Double-Escape (two presses in quick succession) always bails all
            // the way out to the idle/default action, no matter how deep in a
            // multi-step mode we are. A single Escape keeps the existing
            // step-back behavior for interactive input actions.
            static qint64 s_lastEscapeMs = 0;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const bool doubleEscape = (nowMs - s_lastEscapeMs) < 700;
            s_lastEscapeMs = nowMs;
            if (doubleEscape) {
                slotKillAllActions();
                e->accept();
                break;
            }

            bool doDefaultProcessing = true;
            RS_GraphicView *graphicView = getCurrentGraphicView();
            if (graphicView != nullptr) {
                auto currentAction = m_actionHandler->getCurrentAction();
                RS2::ActionType actionType = currentAction->rtti();
                if (RS2::isInteractiveInputAction(actionType)) {
                    graphicView->keyPressEvent(e);
                    e->accept();
                    doDefaultProcessing = false;
                }
            }
            if (doDefaultProcessing){
                slotKillAllActions();
                e->accept();
            }
            break;
        }

        case Qt::Key_Return:
        case Qt::Key_Enter:
            // slotKillAllActions();
            onEnterKey();
            e->accept();
            break;

        case Qt::Key_Plus:
        case Qt::Key_Equal:
            m_actionHandler->setCurrentAction(RS2::ActionZoomIn);
            e->accept();
            break;

        case Qt::Key_Minus:
            m_actionHandler->setCurrentAction(RS2::ActionZoomOut);
            e->accept();
            break;
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: {
            RS_GraphicView* graphicView = getCurrentGraphicView();
            if (graphicView) {
                QWidget* focusWidget = QApplication::focusWidget();
                bool focuseNotInLineEdit = dynamic_cast<QLineEdit*>(focusWidget) == nullptr;
                if (focuseNotInLineEdit || true) {
                    graphicView->keyPressEvent(e);
                    if (!e->isAccepted()) {
                        if (key == Qt::Key_Shift || key == Qt::Key_Control) {
                            e->accept();
                        }
                    }
                }
            }
            [[fallthrough]];
        }
        default:
            e->ignore();
            // fixme - tmp-end
            RS_DEBUG->print("QC_ApplicationWindow::KeyPressEvent: IGNORED");
            break;
    }

    if (e->isAccepted()) {
        RS_DEBUG->print("QC_ApplicationWindow::KeyPressEvent: Accepted");
        return;
    }

    QMainWindow::keyPressEvent(e);
}

void QC_ApplicationWindow::relayAction(QAction *q_action) {
    auto view = getCurrentGraphicView();
    if (view == nullptr) {
        // this is possible if there are not open windows at all
        // when switching back to LibreCAD from another program
        // occasionally no drawings are activated
        qWarning("relayAction: graphicView is nullptr");
        return;
    }

    if (q_action != nullptr) {
        bool setAsCurrentActionInView = true;
        auto property = q_action->property("_SetAsCurrentActionInView");
        if (property.isValid()) {
            setAsCurrentActionInView = property.toBool();
        }

        if (setAsCurrentActionInView) {
            auto* graphicView = dynamic_cast<QG_GraphicView*>(view);
            graphicView->setCurrentQAction(q_action);
        }

        const QString commands(q_action->data().toString());
        if (!commands.isEmpty()) {
            const QString title(q_action->text().remove("&"));
            m_commandWidget->appendHistory(title + " : " + commands);
        }
    }

    fireCurrentActionIconChanged(q_action);
}

/**
 * Called by Qt after a toolbar or dockwidget right-click.
 * See QMainWindow::createPopupMenu() for more information.
 */
QMenu *QC_ApplicationWindow::createPopupMenu() {
   return  m_menuFactory->createMainWindowPopupMenu();
}

void QC_ApplicationWindow::toggleFullscreen(bool checked) {
    if (checked) {
        showFullScreen();
    }
    else {
        showNormal();
        zoomWindow(true);
    }
}

QMenuBar* QC_ApplicationWindow::menuBar() const {
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        return m_titleBar->menuBar();
    }
#endif
    return QMainWindow::menuBar();
}

void QC_ApplicationWindow::zoomWindow(bool on) {
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        m_titleBar->zoom(on);
        return;
    }
#endif
    on ? showMaximized() : showNormal();
}

bool QC_ApplicationWindow::restoreFrameGeometry(const QByteArray& geometry) {
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        return m_titleBar->restoreFrameGeometry(geometry);
    }
#endif
    restoreGeometry(geometry);
    return false;
}

bool QC_ApplicationWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr && m_titleBar->nativeEvent(message, result)) {
        return true;
    }
#endif
    return LC_MDIApplicationWindow::nativeEvent(eventType, message, result);
}

void QC_ApplicationWindow::paintEvent(QPaintEvent* event) {
    LC_MDIApplicationWindow::paintEvent(event);
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        m_titleBar->paintBorder();
    }
#endif
}

void QC_ApplicationWindow::resizeEvent(QResizeEvent* event) {
    LC_MDIApplicationWindow::resizeEvent(event);
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        m_titleBar->windowResized();
    }
#endif
}

/**
 * This slot manipulates the widget options dialog,
 * and reads / writes the associated settings.
 */
void QC_ApplicationWindow::widgetOptionsDialog() {
    if (m_dlgHelpr->showWidgetOptionsDialog()) {
        fireWidgetSettingsChanged();
    }
}

bool QC_ApplicationWindow::loadStyleSheet(const QString &path) {
   return m_styleHelper->loadStyleSheet(path);
}

void QC_ApplicationWindow::reloadStyleSheet() {
    m_styleHelper->reloadStyleSheet();
}

bool QC_ApplicationWindow::eventFilter(QObject *obj, QEvent *event) {
    if (QEvent::FileOpen == event->type()) {
        auto *openEvent = static_cast<QFileOpenEvent *>(event);
        openFile(openEvent->file(), RS2::FormatUnknown);
        return true;
    }
    return QObject::eventFilter(obj, event);
}

void QC_ApplicationWindow::onViewCurrentActionChanged(RS2::ActionType actionType){
   if (actionType  != RS2::ActionNone) {
        auto qAction = m_actionGroupManager->getActionByType(actionType);
        relayAction(qAction);
    }
    else {
        fireCurrentActionIconChanged(nullptr);
    }
}

void QC_ApplicationWindow::updateGridStatus(const QString &status) {
    m_gridStatusWidget->setBottomLabel(status);
}

void QC_ApplicationWindow::showDeviceOptions() {
   m_dlgHelpr->showDeviceOptions();
}

void QC_ApplicationWindow::updateDevice(const QString& device) {
    LC_SET_ONE("Hardware", "Device", device);
    for (const auto &win: m_windowList) {
        win->getGraphicView()->setDeviceName(device);
    }
}

void QC_ApplicationWindow::saveNamedView() {
    m_namedViewsWidget->addNewView();
}

void QC_ApplicationWindow::saveWorkspace(bool on) {
    m_workspacesInvoker->saveWorkspace(on);
}

void  QC_ApplicationWindow::fillWorkspacesList(QList<QPair<int, QString>> &list){
    m_workspacesInvoker->fillWorkspacesList(list);
}

void QC_ApplicationWindow::applyWorkspaceById(int id){
    m_workspacesInvoker->applyWorkspaceById(id);
}

void QC_ApplicationWindow::removeWorkspace(bool on){
    m_workspacesInvoker->removeWorkspace(on);
}

void QC_ApplicationWindow::restoreWorkspace(bool on){
    m_workspacesInvoker->restoreWorkspace(on);
}

void QC_ApplicationWindow::restoreNamedView1() {
    doRestoreNamedView(1);
}

void QC_ApplicationWindow::restoreNamedView2() {
    doRestoreNamedView(2);
}

void QC_ApplicationWindow::restoreNamedView3() {
    doRestoreNamedView(3);
}

void QC_ApplicationWindow::restoreNamedView4() {
    doRestoreNamedView(4);
}

void QC_ApplicationWindow::restoreNamedViewCurrent() {
     m_namedViewsWidget->restoreSelectedView();
}

void QC_ApplicationWindow::restoreNamedView5() {
    doRestoreNamedView(5);
}

void QC_ApplicationWindow::restoreNamedView(const QString& viewName){
    m_namedViewsWidget->restoreView(viewName);
}

void QC_ApplicationWindow::doRestoreNamedView(int i) const {
    m_namedViewsWidget->restoreView(i);
}

void QC_ApplicationWindow::invokeToolbarCreator() {
    m_creatorInvoker->invokeToolbarCreator();
}

void QC_ApplicationWindow::invokeMenuCreator()  {
    m_creatorInvoker->invokeMenuCreator();
}

void QC_ApplicationWindow::changeEvent([[maybe_unused]] QEvent *event) {
#ifdef Q_OS_WIN
    if (m_titleBar != nullptr) {
        m_titleBar->windowChanged(event);
    }
#endif
    // returning to LC via Command+Tab won't always activate a subwindow #821

#if defined(Q_OS_MACOS)
    if (event->type() == QEvent::ActivationChange)
    {
        if (isActiveWindow())
        {
            if (m_currentSubWindow)
                m_mdiAreaCAD->setActiveSubWindow(m_currentSubWindow);
        }
        else
        {
            m_currentSubWindow = m_mdiAreaCAD->currentSubWindow();
        }
    }
#endif
}

void QC_ApplicationWindow::showBlockActivated(const RS_Block *block) const {
    if (block != nullptr) {
        m_blockWidget->activateBlock(const_cast<RS_Block *>(block));
    }
}

QAction *QC_ApplicationWindow::getAction(const QString &actionName) const {
    return m_actionGroupManager->getActionByName(actionName);
}

LC_ActionGroup* QC_ApplicationWindow::getActionGroup(const QString &groupName) const {
    return m_actionGroupManager->getActionGroup(groupName);
}

// todo - think later about staying with signal-slot approach... current one is too explicit
void QC_ApplicationWindow::updateActionsAndWidgetsForPrintPreview(bool printPreviewOn) {
    bool enable = !printPreviewOn;
    enableWidgets(enable);
    for (auto a: m_actionsToDisableInPrintPreviewList) {
        if (a->isEnabled() != enable) {
            a->setEnabled(enable);
        }
    }
    enableWidgetList(enable, {
        m_coordinateWidget,
        m_selectionWidget,
        m_activeLayerNameWidget,
        m_gridStatusWidget,
        m_relativeZeroCoordinatesWidget
        });

    if (printPreviewOn){
        m_mouseWidget->setActionIcon(QIcon());
    }

    emit printPreviewChanged(printPreviewOn);
}

void QC_ApplicationWindow::enableWidgets(bool enable) {
    enableWidgetList(enable, {
                          m_penPaletteWidget,
                          m_quickInfoWidget,
                          m_blockWidget,
                          m_penToolBar,
                          m_penWizard,
                          m_ucsListWidget,
                          m_ucsStateWidget,
                          m_anglesBasisWidget,
                          m_libraryWidget->getInsertButton(),
                         m_snapToolBar
                     });
    //  enableWidget(namedViewsWidget,enable);

    if (enable) {
        enableWidgetList(enable, {
                             m_layerTreeWidget,
                             m_layerWidget
                         });
        // command widget should be enabled for print preview as it supports commands...
        // fixme - command widget should be aware of print preview mode and do not support other commands...
        enableWidget(m_commandWidget, enable);
    }
    // fixme - disable widgets from status bar ??
}

void QC_ApplicationWindow::fireIconsRefresh(){
    emit iconsRefreshed();
}

void QC_ApplicationWindow::fireWidgetSettingsChanged(){
    emit widgetSettingsChanged();
}

void QC_ApplicationWindow::fireCurrentActionIconChanged(QAction *actionIcon){
    emit currentActionIconChanged(actionIcon);
}

void QC_ApplicationWindow::fireWorkspacesChanged(){
    bool hasWorkspaces = m_workspacesInvoker->hasWorkspaces();
    emit workspacesChanged(hasWorkspaces);
}
