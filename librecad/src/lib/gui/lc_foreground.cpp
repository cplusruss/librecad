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

#include "lc_foreground.h"

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace LC_Foreground {

void raiseToForeground(quintptr winId) {
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (hwnd == nullptr) {
        return;
    }

    // Restore if minimized, otherwise just make sure it is shown.
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread =
        (foreground != nullptr) ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const DWORD targetThread = GetWindowThreadProcessId(hwnd, nullptr);

    // Temporarily drop the foreground lock timeout so SetForegroundWindow() is
    // honoured instead of merely flashing the taskbar button.
    DWORD savedTimeout = 0;
    SystemParametersInfoW(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &savedTimeout, 0);
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
                          reinterpret_cast<PVOID>(0), SPIF_SENDCHANGE);

    // Share input state with the current foreground thread so we are permitted to
    // change the foreground window.
    bool attached = false;
    if (foregroundThread != 0 && foregroundThread != targetThread) {
        attached = AttachThreadInput(foregroundThread, targetThread, TRUE);
    }

    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (attached) {
        AttachThreadInput(foregroundThread, targetThread, FALSE);
    }

    // Restore the previous foreground lock timeout.
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
                          reinterpret_cast<PVOID>(static_cast<UINT_PTR>(savedTimeout)),
                          SPIF_SENDCHANGE);
}

}   // namespace LC_Foreground

#else   // !Q_OS_WIN

namespace LC_Foreground {
void raiseToForeground(quintptr) {}
}

#endif  // Q_OS_WIN
