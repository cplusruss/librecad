#include "lc_lastopenfilesopener.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <functional>
#include <memory>
#include <qcoreapplication.h>

#include "qc_applicationwindow.h"
#include "qc_mdiwindow.h"
#include "rs_debug.h"
#include "rs_settings.h"

LC_LastOpenFilesOpener::LC_LastOpenFilesOpener(QC_ApplicationWindow* appWin):m_appWindow{appWin} {
}

LC_LastOpenFilesOpener::~LC_LastOpenFilesOpener() = default;

namespace {
// Append a line to %TEMP%/librecad_tabrestore.log so we can see what happened
// even though the build is `-mwindows` (no console for stderr). The file lives
// in the user's temp dir, autonamed; share with us if tab restore still
// misbehaves.
void diagLog(const QString& line) {
    const QString path = QDir::tempPath() + "/librecad_tabrestore.log";
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream s(&f);
    s << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
      << "  " << line << "\n";
}

// Application-wide input-activity watcher. Each KeyPress / MouseButtonPress /
// MouseMove / Wheel event resets an elapsed timer. The background tab loader
// uses millisSinceLastInput() to decide whether the user is "idle enough"
// that loading another DXF (which monopolises the main thread for seconds)
// won't be felt. Plain QObject — no Q_OBJECT macro because we don't need
// signals/slots; eventFilter is virtual on the base.
class IdleTracker : public QObject {
public:
    explicit IdleTracker(QObject* parent) : QObject(parent) {
        m_timer.start();
        if (qApp != nullptr) qApp->installEventFilter(this);
    }
    qint64 millisSinceLastInput() const { return m_timer.elapsed(); }

protected:
    bool eventFilter(QObject*, QEvent* ev) override {
        switch (ev->type()) {
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseMove:
            case QEvent::Wheel:
                m_timer.restart();
                break;
            default:
                break;
        }
        return false;
    }

private:
    QElapsedTimer m_timer;
};
}

void LC_LastOpenFilesOpener::collectFilesList(const QList<QC_MDIWindow*>& m_windowList, const QMdiSubWindow* activWindow) {
    // We save unconditionally now. The "Open last opened files" Settings
    // toggle still gates whether files are RESTORED on next launch (see
    // openLastOpenFiles), but persisting the active-tab pointer is harmless
    // when restore is off - it's also useful diagnostics.
    m_activeFile  = "";
    m_openedFiles = "";
    m_activeIndex = -1;

    int idx = 0;
    for (auto w : m_windowList) {
        if (w == nullptr) continue;
        QString fileName = w->getFileName();
        if (fileName.isEmpty()) continue;
        if (activWindow != nullptr && static_cast<const QMdiSubWindow*>(w) == activWindow) {
            m_activeFile  = fileName;
            m_activeIndex = idx;
        }
        m_openedFiles += fileName;
        m_openedFiles += ";";
        ++idx;
    }
    diagLog(QStringLiteral("save: activeIndex=%1 total=%2 activeFile=\"%3\"")
            .arg(m_activeIndex).arg(idx).arg(m_activeFile));
}

void LC_LastOpenFilesOpener::saveSettings() {
    LC_GROUP_GUARD("Startup"); {
        // Save unconditionally. See note in collectFilesList.
        LC_SET("LastOpenFilesList",        m_openedFiles);
        LC_SET("LastOpenFilesActive",      m_activeFile);
        LC_SET("LastOpenFilesActiveIndex", m_activeIndex);
    }
    // Force-flush so the writes are on disk BEFORE process exit. QSettings
    // batches writes; without explicit sync() the data is only flushed when
    // its destructor runs, and on Windows that can lose the race with
    // process teardown - leaving the registry unchanged from the previous
    // session. That was the actual cause of "last tab not restored".
    RS_SETTINGS->flush();
    diagLog(QStringLiteral("saveSettings: flushed activeIndex=%1 file=\"%2\"")
            .arg(m_activeIndex).arg(m_activeFile));
}


void LC_LastOpenFilesOpener::openLastOpenFiles(QStringList &fileList) {
    diagLog(QStringLiteral("openLastOpenFiles: ENTER (fileList.size=%1)").arg(fileList.size()));
    bool files_loaded = false;
    // Read everything we need from "Startup" UP FRONT, while m_group is
    // still under our control. openFile() loads DXFs and each load touches
    // many settings via LC_GROUP(...), which mutates RS_Settings::m_group as
    // a side effect (it's not RAII). If we read after the openFile loop,
    // m_group has drifted to whatever the last DXF-load access set it to and
    // LC_GET_INT silently reads from the wrong group, returning the default.
    // Use the *Single forms with explicit group from here on, defensively,
    // even though we cached the values into locals first.
    const QString lastFiles    = LC_GET_ONE_STR("Startup", "LastOpenFilesList", QString{});
    // Reopen last session's tabs on startup, gated by a Startup setting that
    // defaults ON. collectFilesList() snapshots only windows still open at
    // exit, so tabs closed during a session do not come back.
    const bool    reopenLastFiles = LC_GET_ONE_BOOL("Startup", "OpenLastOpenedFiles", true);
    const int     savedIdx     = LC_GET_ONE_INT("Startup", "LastOpenFilesActiveIndex", -1);
    const QString activeFile   = LC_GET_ONE_STR("Startup", "LastOpenFilesActive", QString{});
    diagLog(QStringLiteral("openLastOpenFiles: read lastFiles=\"%1\" reopen=%2 savedIdx=%3 activeFile=\"%4\"")
            .arg(lastFiles).arg(reopenLastFiles).arg(savedIdx).arg(activeFile));

    if (reopenLastFiles) {
        foreach(const QString &filename, lastFiles.split(";")) {
            if (!filename.isEmpty() && QFileInfo::exists(filename))
                fileList << filename;
        }
    }
    if (!fileList.isEmpty()) {
        // Pick the eventual active tab BEFORE doing any I/O. For a restored
        // session that's the saved index/filename; for command-line opens
        // it's the last argument (matches historical behaviour where each
        // openFile() activated, so the last one won).
        QString target;
        if (reopenLastFiles) {
            if (savedIdx >= 0 && savedIdx < fileList.size()) {
                target = fileList.at(savedIdx);
            } else if (!activeFile.isEmpty() && fileList.contains(activeFile)) {
                target = activeFile;
            }
        }
        if (target.isEmpty() || !QFileInfo::exists(target)) {
            target = fileList.last();
        }
        diagLog(QStringLiteral("restore: target=\"%1\"").arg(target));

        // 1) Load the target synchronously. This is the file the user wants
        //    to see immediately, so it gets the full eager-load path and
        //    paints as soon as openFile() returns. Without this, the user
        //    stares at a blank window while every DXF parses in serial.
        m_appWindow->openFile(target);
        files_loaded = true;

        // 2) Lazy-load the remaining files — but only while the user is idle.
        //    Each openFile() synchronously parses a DXF, monopolising the
        //    main thread for hundreds of ms or more. If we kept chaining
        //    those eagerly (0-ms timer) the user would feel "locked up" the
        //    moment they try to draw while restoration is still finishing.
        //
        //    Strategy: install an application-wide event filter that resets
        //    a timer on every input event. Before each background openFile()
        //    we check that timer; if the user has been idle for ≥ kIdleMs,
        //    we load one file. Otherwise we poll again in kPollMs. The
        //    instant the user touches mouse/keyboard, the queue pauses;
        //    when they sit still, loading resumes.
        //
        //    Per-iteration we still snapshot the active tab and restore it
        //    after openFile() so the freshly-loaded window doesn't yank
        //    focus. setUpdatesEnabled is briefly toggled so the activation
        //    flicker doesn't show.
        QStringList remaining;
        for (const QString& f : fileList) {
            if (f != target) remaining << f;
        }
        if (!remaining.isEmpty()) {
            // QPointer guards against the window being destroyed mid-restore.
            QPointer<QC_ApplicationWindow> winGuard(m_appWindow);
            // Tracker owns its eventFilter install; parented to the main
            // window so it dies with it.
            QPointer<IdleTracker> tracker(new IdleTracker(m_appWindow));

            auto idx      = std::make_shared<int>(0);
            auto loadNext = std::make_shared<std::function<void()>>();

            constexpr qint64 kIdleMs = 400;   // idle threshold before next load
            constexpr int    kPollMs = 150;   // re-check cadence while busy
            constexpr int    kKickMs = 400;   // delay before the very first try

            *loadNext = [winGuard, tracker, remaining, idx, loadNext]() {
                if (winGuard.isNull() || tracker.isNull()) return;
                if (*idx >= remaining.size()) return;

                if (tracker->millisSinceLastInput() < kIdleMs) {
                    // User is actively interacting — back off and check later.
                    QTimer::singleShot(kPollMs, [loadNext]() { (*loadNext)(); });
                    return;
                }

                const QString f = remaining.at(*idx);
                ++(*idx);

                QC_ApplicationWindow* win = winGuard.data();
                QC_MDIWindow* preActive   = win->getCurrentMDIWindow();
                const QString preName     = (preActive != nullptr)
                                                ? preActive->getFileName()
                                                : QString();

                const bool prevUE = win->updatesEnabled();
                win->setUpdatesEnabled(false);
                win->openFile(f);
                if (!preName.isEmpty()) {
                    win->activateWindowWithFile(preName);
                }
                win->setUpdatesEnabled(prevUE);

                // Schedule the next check. The user gets at minimum kPollMs
                // of guaranteed responsiveness between loads, plus the full
                // kIdleMs gate if they're still active.
                QTimer::singleShot(kPollMs, [loadNext]() { (*loadNext)(); });
            };
            // First attempt is delayed a bit longer to guarantee the user has
            // a fair chance to start editing the active tab before any
            // background work begins.
            QTimer::singleShot(kKickMs, [loadNext]() { (*loadNext)(); });
        }
    }

    RS_DEBUG->print("main: loading files: OK");

    if (!files_loaded) {
        m_appWindow->slotFileNewFromDefaultTemplate();
    }
}
