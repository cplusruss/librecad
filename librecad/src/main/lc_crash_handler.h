/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2026 LibreCAD www.librecad.org
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
#ifndef LC_CRASH_HANDLER_H
#define LC_CRASH_HANDLER_H

class LC_CrashHandler {
public:
    static void install();

    // Console-only error mode. Call this BEFORE Qt is constructed:
    //  - suppresses Windows system error popups (SEM_FAILCRITICALERRORS etc.)
    //  - attaches to the parent console (cmd.exe / PowerShell) when launched
    //    interactively, so stderr/stdout from qDebug() and crash dumps appear
    //    in the terminal instead of a GUI message box.
    //  - installs a Qt message handler that routes qDebug/qWarning/qCritical/
    //    qFatal lines to stderr.
    // Safe to call once; subsequent calls are no-ops.
    static void redirectToConsole();

    LC_CrashHandler() = delete;
    LC_CrashHandler(const LC_CrashHandler&) = delete;
    LC_CrashHandler& operator=(const LC_CrashHandler&) = delete;
};

#endif // LC_CRASH_HANDLER_H
