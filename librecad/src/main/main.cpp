/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2026 LibreCAD www.librecad.org
** Copyright (C) 2018 A. Stebich (librecad@mail.lordofbikes.de)
** Copyright (C) 2018 Simon Wells <simonrwells@gmail.com>
** Copyright (C) 2020 Nikita Letov <letovnn@gmail.com>
** Copyright (C) 2015-2016 ravas (github.com/r-a-v-a-s)
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
#include <clocale>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <cstring>

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>

#include <QDir>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>

#include "console_dxf2pdf.h"
#include "console_dxf2png.h"
#include "lc_appicon.h"
#include "lc_application.h"
#include "lc_crash_handler.h"
#include "lc_dialogtabkeys.h"
#include "lc_theme.h"
#include "main.h"

#include "lc_iconcolorsoptions.h"
#include "qc_applicationwindow.h"
#include "qg_dlginitial.h"
#include "rs_debug.h"
#include "rs_fontlist.h"
#include "rs_patternlist.h"
#include "rs_settings.h"
#include "rs_system.h"

// default version: if not supplied by during building
#ifndef LC_VERSION
#define LC_VERSION "2.2.2-alpha"
#endif

// fixme - sand - files - complete refactoring
namespace
{
// To a plain text string to the compiled binary
const std::string g_lcVersion{"LC_VISION=" XSTR(LC_VERSION)};
}

void showFirstLoadSetupDialog(bool first_load) {
    LC_GROUP_GUARD("Defaults");
    {
        QString unit = LC_GET_STR("Unit", "Invalid");
        // show initial config dialog:
        if (first_load){
            RS_DEBUG->print("main: show initial config dialog..");
            QG_DlgInitial di(nullptr);
            QPixmap pxm(":/images/intro_librecad.png");
            di.setPixmap(pxm);
            if (di.exec()) {
                unit = LC_GET_STR("Unit", "None");
            }
            RS_DEBUG->print("main: show initial config dialog: OK");
        }
    }
}

void showDebugSetupHelpMessage() {
    RS_DEBUG->print(RS_Debug::D_NOTHING, "possible debug levels:");
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Nothing", RS_Debug::D_NOTHING);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Critical", RS_Debug::D_CRITICAL);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Error", RS_Debug::D_ERROR);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Warning", RS_Debug::D_WARNING);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Notice", RS_Debug::D_NOTICE);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Informational", RS_Debug::D_INFORMATIONAL);
    RS_DEBUG->print(RS_Debug::D_NOTHING, "    %d Debugging", RS_Debug::D_DEBUGGING);
}

void initFontList() {
    RS_DEBUG->print("main: init fontlist..");
    RS_FONTLIST->init();
    RS_DEBUG->print("main: init fontlist: OK");
}

void initPatternList() {
    RS_DEBUG->print("main: init patternlist..");
    RS_PATTERNLIST->init();
    RS_DEBUG->print("main: init patternlist: OK");
}

void initSystem(char** argv, LC_Application& app) {
    RS_DEBUG->print("param 0: %s", argv[0]);

    QFileInfo prgInfo( QFile::decodeName(argv[0]) );
    QString prgDir(prgInfo.absolutePath());

    RS_SYSTEM->init(app.applicationName(), app.applicationVersion(), XSTR(QC_APPDIR), prgDir);
}

void loadFilesOnStartup(QC_ApplicationWindow& appWin, [[maybe_unused]]LC_Application& app, QStringList fileList) {
    RS_DEBUG->print("main: loading files..");
#ifdef __APPLE__
    // get the file list from LC_Application
    fileList << app.fileList();
#endif

    // reopen files that we open during last close of application
    // we'll reopen them if no explicit files to open are provided in command line

    appWin.openFilesOnStartup(fileList);
    RS_DEBUG->print("main: loading files: OK");
}

void loadIconsStylingOptions() {
    LC_IconColorsOptions iconColorsOptions;
    iconColorsOptions.loadSettings();
    iconColorsOptions.applyOptions();
}

int execApplication(LC_Application& app) {
    RS_DEBUG->print("main: entering Qt event loop");
    QCoreApplication::processEvents();

    int return_code = app.exec();

    RS_DEBUG->print("main: exited Qt event loop");

    // Destroy the singleton
    QC_ApplicationWindow::getAppWindow().reset();
    RS_SETTINGS->flush();
    return return_code;
}

//
bool setupDebugLevel(char level) {
    switch(level){
        case '?' : {
            showDebugSetupHelpMessage();
            return false;
        }
        case '0' + RS_Debug::D_NOTHING : {
            RS_DEBUG->setLevel(RS_Debug::D_NOTHING);
            break;
        }
        case '0' + RS_Debug::D_CRITICAL : {
            RS_DEBUG->setLevel(RS_Debug::D_CRITICAL);
            break;
        }
        case '0' + RS_Debug::D_ERROR : {
            RS_DEBUG->setLevel(RS_Debug::D_ERROR);
            break;
        }
        case '0' + RS_Debug::D_WARNING : {
            RS_DEBUG->setLevel(RS_Debug::D_WARNING);
            break;
        }
        case '0' + RS_Debug::D_NOTICE : {
            RS_DEBUG->setLevel(RS_Debug::D_NOTICE);
            break;
        }
        case '0' + RS_Debug::D_INFORMATIONAL : {
            RS_DEBUG->setLevel(RS_Debug::D_INFORMATIONAL);
            break;
        }
        case '0' + RS_Debug::D_DEBUGGING : {
            RS_DEBUG->setLevel(RS_Debug::D_DEBUGGING);
            break;
        }
        default : {
            RS_DEBUG->setLevel(RS_Debug::D_DEBUGGING);
            break;
        }
    }
    return true;
}


#define QUICK_TEST_

/**
 * Main. Creates Application window.
 */
 // fixme - sand - refactor and split to several specialized functions
#ifndef BUILD_TESTS

int main(int argc, char** argv) {

#ifdef QUICK_TEST

#    else

    // Console-only error mode — installed BEFORE Qt initialization so any
    // qDebug/qWarning the Qt subsystem emits during early bring-up goes to
    // stderr (terminal) and no Windows critical-error popups are shown.
    LC_CrashHandler::redirectToConsole();
    LC_CrashHandler::install();

    // Create compilater's error: this QT macros may be in .pro file only.
    //QT_REQUIRE_VERSION(argc, argv, "6.4");

    // May be this code must be on begin of main.cpp?
    #if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
        #error "This programm requires Qt6 ver.6.4.0 or higher."
    #endif

    // Check first two arguments in order to decide if we want to run LibreCAD
    // as console dxf2pdf or dxf2png tools. On Linux we can create a link to
    // LibreCAD executable and  name it dxf2pdf. So, we can run either:
    //
    //     LibreCAD dxf2pdf [options] ...
    //
    // or just:
    //
    //     dxf2pdf [options] ...
    //
    for (int i = 0; i < qMin(argc, 2); i++) {
        QString arg(argv[i]);
        if (i == 0) {
            arg = QFileInfo(QFile::decodeName(argv[i])).baseName();
        }
        if (arg.compare("dxf2pdf") == 0) {
            return console_dxf2pdf(argc, argv);
        }
        if (arg.compare("dxf2png") == 0 || arg == "dxf2svg") {
            return console_dxf2png(argc, argv);
        }
    }

    RS_DEBUG->setLevel(RS_Debug::D_WARNING);

    // Per-monitor DPI rounding policy must be set BEFORE QGuiApplication
    // construction. Qt6's default is PassThrough (full fractional scales
    // honoured); on Windows with 125% / 150% scaling that leaks subpixel
    // metrics into widget layouts and can grow a window 1-2 logical pixels
    // larger than the screen edge would otherwise allow. RoundPreferFloor
    // gives stable integer scales and is the conventional Qt6 hardening.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

    LC_Application app(argc, argv);
    QCoreApplication::setOrganizationName("LibreCAD");
    QCoreApplication::setApplicationName("LibreCAD");
    QCoreApplication::setApplicationVersion(XSTR(LC_VERSION));

    // ---------------------------------------------------------------------
    // Single-instance: if another LibreCAD is already running, forward our
    // file args to it and exit. The primary instance opens them as new MDI
    // tabs (or activates an existing tab if the file is already loaded) and
    // raises its main window. See QC_ApplicationWindow for the server side.
    //
    // This block intentionally runs BEFORE settings, fonts, etc.,
    // so the secondary process is cheap and disappears quickly.
    // ---------------------------------------------------------------------
    {
        QStringList earlyFiles;
        for (int i = 1; i < argc; ++i) {
            const QString a = QString::fromLocal8Bit(argv[i]);
            if (a.startsWith('-')) continue;
            QFileInfo fi(a);
            earlyFiles << QDir::toNativeSeparators(fi.absoluteFilePath());
        }

        QLocalSocket probe;
        probe.connectToServer(QC_ApplicationWindow::singleInstanceSocketName());
        if (probe.waitForConnected(400)) {
            QByteArray payload;
            for (const QString &f : earlyFiles) {
                payload += f.toUtf8();
                payload += '\n';
            }
            // Even with an empty payload we still write+flush so the server
            // wakes up and raises its main window.
            probe.write(payload);
            probe.flush();
            probe.waitForBytesWritten(1000);
            probe.disconnectFromServer();
            if (probe.state() != QLocalSocket::UnconnectedState) {
                probe.waitForDisconnected(500);
            }
            return 0;
        }
    }

    // fixme - sand - NEED TO CHECK WHERE lc_svgicons.so is located under linux and mac!!! That's tested for Windows
    auto appDir = app.applicationDirPath();
    auto inconEnginesDir = appDir + "/iconengines";
    app.addLibraryPath(inconEnginesDir);
    app.installEventFilter(new LC_DialogTabKeys(&app));

    try {
        RS_Settings::init(RS_Settings::defaultIniPath(), RS_Settings::legacyStores());
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, app.applicationName(), e.what());
        return 1;
    }

    QGuiApplication::setDesktopFileName("librecad");
    LC_Theme::apply();
    LC_AppIcon::apply();

    loadIconsStylingOptions();

    bool first_load = LC_GET_ONE_BOOL("Startup", "FirstLoad", true);

    bool allowOptions=true;
    QList<int> argClean;

    for (int i=0; i<argc; i++)   {
        QString argstr(argv[i]);
        if(allowOptions&&QString::compare("--", argstr)==0){
            allowOptions=false;
            continue;
        }
        const QString lpDebugSwitch0("-d"),lpDebugSwitch1("--debug") ;

        if (allowOptions&& (argstr.startsWith(lpDebugSwitch0, Qt::CaseInsensitive) ||
                             argstr.startsWith(lpDebugSwitch1, Qt::CaseInsensitive) )){
            argClean<<i;

            // to control the level of debugging output use --debug with level 0-6, e.g. --debug3
            // for a list of debug levels use --debug?
            // if no level follows, the debugging level is set
            argstr.remove(QRegularExpression("^"+lpDebugSwitch0));
            argstr.remove(QRegularExpression("^"+lpDebugSwitch1));
            char level = '3';
            if(argstr.size()==0){
                if(i+1<argc){
                    if(QRegularExpression(R"(\d*)").match(argv[i+1]).hasMatch()){
                        ++i;
                        qDebug()<<"reading "<<argv[i]<<" as debugging level";
                        level=argv[i][0];
                        argClean<<i;
                    }
                    else {
                        level = '3';
                    }
                }
                else {
                    level = '3'; //default to D_WARNING
                }
            }
            else {
                level = argstr.toStdString()[0];
            }

            if (!setupDebugLevel(level)) {
                return 0;
            }
        }
    }
    initSystem(argv, app);
    showFirstLoadSetupDialog(first_load);

    initFontList();
    initPatternList();

    RS_DEBUG->print("main: creating main window..");
    QC_ApplicationWindow& appWin = *QC_ApplicationWindow::getAppWindow();
    auto& appWindow = QC_ApplicationWindow::getAppWindow();
    if (appWindow != nullptr) {
        appWindow->fireIconsRefresh();
    }

    // Now that the main window exists, start listening for secondary launches.
    appWin.startSingleInstanceServer(QC_ApplicationWindow::singleInstanceSocketName());
#ifdef __APPLE__
    app.installEventFilter(&appWin);
#endif
    RS_DEBUG->print("main: setting caption");
#ifdef LC_BUILD_DATE
    appWin.setWindowTitle(QStringLiteral("%1  [%2]")
                          .arg(app.applicationName(),
                               QStringLiteral(LC_BUILD_DATE)));
#else
    appWin.setWindowTitle(app.applicationName());
#endif

    RS_DEBUG->print("main: show main window");

    const auto settings = RS_Settings::createQSettings();
    settings->beginGroup("Defaults");
    if( !settings->contains("UseQtFileOpenDialog")) {
#ifdef __linux__
        // on Linux don't use native file dialog
        // because of case insensitive filters (issue #791)
        settings->setValue("UseQtFileOpenDialog", QVariant(1));
#else
        settings->setValue("UseQtFileOpenDialog", QVariant(0));
#endif
    }
    settings->endGroup();

    bool maximize = LC_GET_ONE_BOOL("Startup","Maximize", false);

    appWin.show();
    if (maximize || first_load) {
        appWin.zoomWindow(true);
    }

    RS_DEBUG->print("main: set focus");
    appWin.setFocus();
    RS_DEBUG->print("main: creating main window: OK");

    // Set LC_NUMERIC so that entering numeric values uses . as the decimal separator
    setlocale(LC_NUMERIC, "C");

    // parse command line arguments that might not need a launched program:
    // fixme - sand - add support of skipping of loading via cmdline flag
    QStringList fileList = handleArgs(argc, argv, argClean);
    loadFilesOnStartup(appWin, app, fileList);

    appWin.initCompleted();

    LC_GROUP("Startup");
    {
        // Auto-update system disabled: an update can brick the install.
        if (first_load){
            LC_SET("FirstLoad", false);
        }
    }
    LC_GROUP_END();

    return execApplication(app);
#    endif
}
#endif // BUILD_TESTS

/**
 * Handles command line arguments that might not require a GUI.
 *
 * @return list of files to load on startup.
 */
QStringList handleArgs(int argc, char** argv, const QList<int>& argClean){
    RS_DEBUG->print("main: handling args..");
    QStringList ret;

    bool doexit = false;
    for (int i = 1; i < argc; i++) {
        if (argClean.indexOf(i) >= 0) {
            continue;
        }
        auto localFileName = argv[i];
        if (!QString(localFileName).startsWith("-")) {
            auto decodedName = QFile::decodeName(localFileName);
            QFileInfo fileInfo(decodedName);
            auto absolutePath = fileInfo.absoluteFilePath();
            QString fname = QDir::toNativeSeparators(absolutePath);
            ret.append(fname);
        }
        else if (QString(localFileName) == "--exit") {
            doexit = true;
        }
    }
    if (doexit) {
        exit(0);
    }
    RS_DEBUG->print("main: handling args: OK");
    return ret;
}
