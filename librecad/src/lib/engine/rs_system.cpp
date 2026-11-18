/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2021 A. Stebich (librecad@mail.lordofbikes.de)
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

#include "rs_system.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include "rs_debug.h"
#include "rs_settings.h"

RS_System* RS_System::instance() {
    static RS_System* uniqueInstance = new RS_System();
    return uniqueInstance;
}

/**
 * Initializes the system.
 *
 * @param appName Application name (e.g. "LibreCAD II")
 * @param appVersion Application version (e.g. "1.2.3")
 * @param appDirName Application directory name used for
 *     subdirectories in /usr, /etc ~/.
 */
void RS_System::init(const QString& appName,
                     const QString& appVersion,
                     const QString& appDirName,
                     const char *arg0) {
    this->appName = appName;
    this->appVersion = appVersion;
    this->appDirName = appDirName;

    if (QFile::decodeName( arg0).contains( "/.mount")) {
        // in AppImage QCoreApplication::applicationDirPath() directs to /lib64 of mounted AppImage
        // thus use argv[0] to extract the correct path to LibreCAD executable
        appDir = QFileInfo( QFile::decodeName( arg0)).absoluteFilePath();
        RS_DEBUG->print("%s\n", (QString("arg0:")+ QString(arg0)).toUtf8().constData());
        RS_DEBUG->print("%s\n", (QString("appDir:")+ appDir).toUtf8().constData());
    }
    else {
        // in regular application QCoreApplication::applicationDirPath() is preferred, see GitHub #1488
        appDir = QCoreApplication::applicationDirPath();
        RS_DEBUG->print("%s\n", (QString("appDir2:")+ appDir).toUtf8().constData());
    }

    // when appDir is not HOME or CURRENT dir, search appDir too in getDirectoryList()
    externalAppDir = (!appDir.isEmpty()
                   && "/" != appDir
                   && getHomeDir() != appDir
                   && getCurrentDir() != appDir);

    RS_DEBUG->print("RS_System::init: System %s initialized.", appName.toLatin1().data());
    RS_DEBUG->print("RS_System::init: App dir: %s", appDir.toLatin1().data());
    initialized = true;
}


void RS_System::init(const QString& appName,
                     const QString& appVersion,
                     const QString& appDirName,
                     const QString& arg0)
{
    init(appName, appVersion, appDirName, arg0.toLatin1().data());
}


/**
 * Checks if the system has been initialized and prints a warning
 * otherwise to stderr.
 */
bool RS_System::checkInit() const
{
    if (!initialized) {
        RS_DEBUG->print(RS_Debug::D_WARNING,
                        "RS_Syste.m::checkInit: System not initialized.\n"
                        "Use RS_SYSTEM->init(appname, appdirname) to do so.");
    }
    return initialized;
}




/**
 * Creates all given directories in the user's home.
 */
bool RS_System::createPaths(const QString& directory) {
    QDir dir;
    dir.cd(QDir::homePath());
    dir.mkpath(directory);
    return true;
}


// Settings files (aliases, shortcuts, workspaces, pen palette) live beside the exe with
// LibreCAD.ini so they sync together.
QString RS_System::getAppDataDir() const
{
    return QCoreApplication::applicationDirPath();
}


/**
 * Searches for files in an application shared directory in the given
 * subdirectory with the given extension.
 *
 * @return List of the absolute paths of the files found.
 */
QStringList RS_System::getFileList(const QString& subDirectory,
                                   const QString& fileExtension) const
{
    checkInit();

    RS_DEBUG->print( "RS_System::getFileList: subdirectory %s ", subDirectory.toLatin1().data());
    RS_DEBUG->print( "RS_System::getFileList: appDirName %s ", appDirName.toLatin1().data());
    RS_DEBUG->print( "RS_System::getFileList: getCurrentDir %s ", getCurrentDir().toLatin1().data());

    QStringList fileList;

    auto directoryList = getDirectoryList( subDirectory);

    foreach(const QString& path, directoryList) {
        QDir dir {path};

        if (dir.exists() && dir.isReadable()) {
            QStringList files = dir.entryList( QStringList( "*." + fileExtension));
            for(QString& file: files)
            {
                fileList += path + "/" + file;
            }
        }
    }

    LC_LOG<<__func__<<"():: fileList:";
    foreach(const auto& file, fileList)
        LC_LOG<<file;

    return fileList;
}


/**
 * @return List of all directories in subdirectory 'subDirectory' in
 * all possible LibreCAD directories.
 */
QStringList RS_System::getDirectoryList(const QString& _subDirectory) const
{
        QStringList dirList;

    QString subDirectory = QDir::fromNativeSeparators( _subDirectory);

#ifdef Q_OS_MAC
    dirList.append(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + appDirName + "/" + subDirectory);
#endif // Q_OS_MAC

#if (defined(Q_OS_WIN32) || defined(Q_OS_WIN64))
    dirList.append(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + appDirName + "/" + subDirectory);
#endif // Q_OS_WIN32 or Q_OS_WIN64

    // Unix home directory, it's old style but some people might have stuff there.
    dirList.append( getHomeDir() + "/." + appDirName + "/" + subDirectory);

    //local (application) directory has priority over other dirs:
    if (!subDirectory.compare( "plugins")) {
        // 17 Aug, 2011, Dongxu Li, do not look for plugins in the current folder,
        // we should install plugins to system or ~/.librecad/plugins/
        if (externalAppDir) {
            dirList.append( appDir + "/" + subDirectory);
        }
    }

    RS_DEBUG->print("%s\n", QString("%1(): line %2: dir=%3").arg(__func__).arg(__LINE__).arg(appDir).toUtf8().constData());

#if (defined(Q_OS_WIN32) || defined(Q_OS_WIN64) || defined(Q_OS_UNIX))
    // for AppImage use relative paths from executable
    // from package manager the executable is in /usr/bin
    // in AppImage the executable is APPDIR/usr/bin
    // so this should work for package manager and AppImage distribution
    dirList.append( QDir::cleanPath( appDir + "/../share/doc/" + appDirName + "/" + subDirectory));

    // try various locations for different Linux distributions
    dirList.append( QDir::cleanPath( appDir + "/../share/" + appDirName + "/" + subDirectory));
    dirList.append( QDir::cleanPath( appDir + "/../lib64/" + appDirName + "/" + subDirectory));
    dirList.append( QDir::cleanPath( appDir + "/../lib/" + appDirName + "/" + subDirectory));

    if (QStringLiteral( "plugins") == subDirectory) {
        dirList.append( QDir::cleanPath( appDir + "/../lib64/" + appDirName));
        dirList.append( QDir::cleanPath( appDir + "/../lib/" + appDirName));
    }
#endif
    for (auto& dir: dirList) {

        RS_DEBUG->print("%s\n", QString("%1(): line %2: dir=%3\n").arg(__func__).arg(__LINE__).arg(dir).toUtf8().constData());
    }

#ifdef Q_OS_MAC
    // Apple uses the resource directory
    if (!appDir.isEmpty() && appDir!="/") {
        dirList.append( QDir::cleanPath( appDir + "/../Resources/" + subDirectory));
    }
#endif

#ifndef Q_OS_MAC
    // Add support directory if LibreCAD is run-in-place,
    // not for Apple because it uses resources this is more for unix systems
    dirList.append( appDir + "/resources/" + subDirectory);
#endif

    // Individual directories:
    {
        LC_GROUP_GUARD( "Paths");
        {
            auto option = Qt::SkipEmptyParts;

            if (subDirectory == "fonts") {
                QString savedFonts = LC_GET_STR("Fonts", "");
                RS_DEBUG->print("saved fonts: %s\n", savedFonts.toUtf8().constData());
                dirList += (LC_GET_STR("Fonts", "")).split(QRegularExpression("[;]"),
                                                           option);
            } else if (subDirectory == "patterns") {
                dirList += (LC_GET_STR("Patterns", "")).split(QRegularExpression("[;]"),
                                                              option);
            } else if (subDirectory.startsWith("scripts")) {
                dirList += (LC_GET_STR("Scripts", "")).split(QRegularExpression("[;]"),
                                                             option);
            } else if (subDirectory.startsWith("library")) {
                dirList += (LC_GET_STR("Library", "")).split(QRegularExpression("[;]"),
                                                             option);
            }
        }
    }

    QStringList ret;

    RS_DEBUG->print("RS_System::getDirectoryList: Paths:");
    for (const QString& dir: std::as_const(dirList)) {
        if (QFileInfo{dir}.isDir()) {
            ret += dir;
        }
    }

    for (const QString& dir: std::as_const(ret)) {
        RS_DEBUG->print("%s\n", QString("%1(): line %2: dir=%3").arg(__func__).arg(__LINE__).arg(dir).toUtf8().constData());
    }

    return ret;
}

/**
 * Tries to convert the given encoding string to an encoding Qt knows.
 */
QString RS_System::getEncoding(const QString& str) {
    QString l=str.toLower();

    if (l=="latin1" || l=="ansi_1252" || l=="iso-8859-1" ||
            l=="cp819" || l=="csiso" || l=="ibm819" || l=="iso_8859-1" ||
            l=="iso8859-1" || l=="iso-ir-100" || l=="l1") {
        return "Latin1";
    } else if (l=="big5" || l=="ansi_950" || l=="cn-big5" || l=="csbig5" ||
               l=="x-x-big5") {
        return "Big5";
    } else if (l=="big5-hkscs") {
        return "Big5-HKSCS";
    } else if (l=="eucjp" || l=="euc-jp" || l=="cseucpkdfmtjapanese" ||
               l=="x-euc" || l=="x-euc-jp") {
        return "eucJP";
    } else if (l=="euckr") {
        return "eucKR";
    } else if (l=="gb2312" || l=="chinese" || l=="cn-gb" ||
               l=="csgb2312" || l=="csgb231280" || l=="csiso58gb231280" ||
               l=="gb_2312-80" || l=="gb231280" || l=="gb2312-80" || l=="gbk" ||
               l=="iso-ir-58") {
        return "GB2312";
    } else if (l=="gbk") {
        return "GBK";
    } else if (l=="gb18030") {
        return "GB18030";
    } else if (l=="jis7") {
        return "JIS7";
    } else if (l=="shift-jis" || l=="ansi_932" || l=="shift_jis" || l=="csShiftJIS" ||
               l=="cswindows31j" || l=="ms_kanji" || l=="x-ms-cp932" || l=="x-sjis") {
        return "Shift-JIS";
    } else if (l=="tscii") {
        return "TSCII";
    } else if (l=="utf88-bit") {
        return "utf88-bit";
    } else if (l=="utf16") {
        return "utf16";
    } else if (l=="utf8" || l=="utf-8") {
        return "utf-8";
    } else if (l=="koi8-r") {
        return "KOI8-R";
    } else if (l=="koi8-u") {
        return "KOI8-U";
    } else if (l=="iso8859-1") {
        return "ISO8859-1";
    } else if (l=="iso8859-2") {
        return "ISO8859-2";
    } else if (l=="iso8859-3") {
        return "ISO8859-3";
    } else if (l=="iso8859-4" || l=="ansi_1257") {
        return "ISO8859-4";
    } else if (l=="iso8859-5") {
        return "ISO8859-5";
    } else if (l=="iso8859-6" || l=="ansi_1256") {
        return "ISO8859-6";
    } else if (l=="iso8859-7" || l=="ansi_1253") {
        return "ISO8859-7";
    } else if (l=="iso8859-8") {
        return "ISO8859-8";
    } else if (l=="iso8859-8-i" || l=="ansi_1255") {
        return "ISO8859-8-i";
    } else if (l=="iso8859-9" || l=="ansi_1254") {
        return "ISO8859-9";
    } else if (l=="iso8859-10") {
        return "ISO8859-10";
    } else if (l=="iso8859-13") {
        return "ISO8859-13";
    } else if (l=="iso8859-14") {
        return "ISO8859-14";
    } else if (l=="iso8859-15") {
        return "ISO8859-15";
    } else if (l=="ibm 850") {
        return "IBM 850";
    } else if (l=="ibm 866") {
        return "IBM 866";
    } else if (l=="cp874") {
        return "CP874";
    } else if (l=="cp1250") {
        return "CP1250";
    } else if (l=="cp1251" || l=="ansi_1251") {
        return "CP1251";
    } else if (l=="cp1252") {
        return "CP1252";
    } else if (l=="cp1253") {
        return "CP1253";
    } else if (l=="cp1254") {
        return "CP1254";
    } else if (l=="cp1255") {
        return "CP1255";
    } else if (l=="cp1256") {
        return "CP1256";
    } else if (l=="cp1257") {
        return "CP1257";
    } else if (l=="cp1258") {
        return "CP1258";
    } else if (l=="apple roman") {
        return "Apple Roman";
    } else if (l=="tis-620") {
        return "TIS-620";
    }

    return "latin1";
}


/** Returns ISO code for given locale. Needed for win32 to convert
 from system encodings.
 Locale names mostly copied from XFree86.

 The code may be incomplete (chinese/japanese locales, etc.)

 2004-05-13, J Staniek
*/
static QMap<QByteArray,QByteArray> loc_map;

QByteArray RS_System::localeToISO(const QByteArray& locale) {
    if (loc_map.isEmpty()) {
        loc_map["croatian"]="ISO8859-2";
        loc_map["cs"]="ISO8859-2";
        loc_map["cs_CS"]="ISO8859-2";
        loc_map["cs_CZ"]="ISO8859-2";
        loc_map["cz"]="ISO8859-2";
        loc_map["cz_CZ"]="ISO8859-2";
        loc_map["czech"]="ISO8859-2";
        loc_map["hr"]="ISO8859-2";
        loc_map["hr_HR"]="ISO8859-2";
        loc_map["hu"]="ISO8859-2";
        loc_map["hu_HU"]="ISO8859-2";
        loc_map["hungarian"]="ISO8859-2";
        loc_map["pl"]="ISO8859-2";
        loc_map["pl_PL"]="ISO8859-2";
        loc_map["polish"]="ISO8859-2";
        loc_map["ro"]="ISO8859-2";
        loc_map["ro_RO"]="ISO8859-2";
        loc_map["rumanian"]="ISO8859-2";
        loc_map["serbocroatian"]="ISO8859-2";
        loc_map["sh"]="ISO8859-2";
        loc_map["sh_SP"]="ISO8859-2";
        loc_map["sh_YU"]="ISO8859-2";
        loc_map["sk"]="ISO8859-2";
        loc_map["sk_SK"]="ISO8859-2";
        loc_map["sl"]="ISO8859-2";
        loc_map["sl_CS"]="ISO8859-2";
        loc_map["sl_SI"]="ISO8859-2";
        loc_map["slovak"]="ISO8859-2";
        loc_map["slovene"]="ISO8859-2";
        loc_map["sr_SP"]="ISO8859-2";

        loc_map["eo"]="ISO8859-3";

        loc_map["ee"]="ISO8859-4";
        loc_map["ee_EE"]="ISO8859-4";

        loc_map["mk"]="ISO8859-5";
        loc_map["mk_MK"]="ISO8859-5";
        loc_map["sp"]="ISO8859-5";
        loc_map["sp_YU"]="ISO8859-5";

        loc_map["ar_AA"]="ISO8859-6";
        loc_map["ar_SA"]="ISO8859-6";
        loc_map["arabic"]="ISO8859-6";

        loc_map["el"]="ISO8859-7";
        loc_map["el_GR"]="ISO8859-7";
        loc_map["greek"]="ISO8859-7";

        loc_map["hebrew"]="ISO8859-8";
        loc_map["he"]="ISO8859-8";
        loc_map["he_IL"]="ISO8859-8";
        loc_map["iw"]="ISO8859-8";
        loc_map["iw_IL"]="ISO8859-8";

        loc_map["tr"]="ISO8859-9";
        loc_map["tr_TR"]="ISO8859-9";
        loc_map["turkish"]="ISO8859-9";

        loc_map["lt"]="ISO8859-13";
        loc_map["lt_LT"]="ISO8859-13";
        loc_map["lv"]="ISO8859-13";
        loc_map["lv_LV"]="ISO8859-13";

        loc_map["et"]="ISO8859-15";
        loc_map["et_EE"]="ISO8859-15";
        loc_map["br_FR"]="ISO8859-15";
        loc_map["ca_ES"]="ISO8859-15";
        loc_map["de"]="ISO8859-15";
        loc_map["de_AT"]="ISO8859-15";
        loc_map["de_BE"]="ISO8859-15";
        loc_map["de_DE"]="ISO8859-15";
        loc_map["de_LU"]="ISO8859-15";
        loc_map["en_IE"]="ISO8859-15";
        loc_map["es"]="ISO8859-15";
        loc_map["es_EC"]="ISO8859-15";
        loc_map["es_ES"]="ISO8859-15";
        loc_map["eu_ES"]="ISO8859-15";
        loc_map["fi"]="ISO8859-15";
        loc_map["fi_FI"]="ISO8859-15";
        loc_map["finnish"]="ISO8859-15";
        loc_map["fr"]="ISO8859-15";
        loc_map["fr_FR"]="ISO8859-15";
        loc_map["fr_BE"]="ISO8859-15";
        loc_map["fr_LU"]="ISO8859-15";
        loc_map["french"]="ISO8859-15";
        loc_map["ga_IE"]="ISO8859-15";
        loc_map["gl_ES"]="ISO8859-15";
        loc_map["it"]="ISO8859-15";
        loc_map["it_IT"]="ISO8859-15";
        loc_map["oc_FR"]="ISO8859-15";
        loc_map["nl"]="ISO8859-15";
        loc_map["nl_BE"]="ISO8859-15";
        loc_map["nl_NL"]="ISO8859-15";
        loc_map["pt"]="ISO8859-15";
        loc_map["pt_PT"]="ISO8859-15";
        loc_map["sv_FI"]="ISO8859-15";
        loc_map["wa_BE"]="ISO8859-15";

        loc_map["uk"]="KOI8-U";
        loc_map["uk_UA"]="KOI8-U";
        loc_map["ru_YA"]="KOI8-U";
        loc_map["ukrainian"]="KOI8-U";

        loc_map["be"]="KOI8-R";
        loc_map["be_BY"]="KOI8-R";
        loc_map["bg"]="KOI8-R";
        loc_map["bg_BG"]="KOI8-R";
        loc_map["bulgarian"]="KOI8-R";
        loc_map["ba_RU"]="KOI8-R";
        loc_map["ky"]="KOI8-R";
        loc_map["ky_KG"]="KOI8-R";
        loc_map["kk"]="KOI8-R";
        loc_map["kk_KZ"]="KOI8-R";
    }
    QByteArray l = loc_map[locale];
    if (l.isEmpty()) {
        return "ISO8859-1";
    }
    return l;
}

QString RS_System::getHomeDir() {
    return QDir::homePath();
}

/**
 * @return Users home directory.
 */
QString RS_System::getTempDir() {
    return QDir::tempPath();
}

/**
 * @return Current directory.
 */
QString RS_System::getCurrentDir() {
    return QDir::currentPath();
}
