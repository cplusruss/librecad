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

#ifndef LC_FOREGROUND_H
#define LC_FOREGROUND_H

#include <QtGlobal>

namespace LC_Foreground {
/**
 * Force the native top-level window identified by `winId` to the foreground and
 * give it input focus.
 *
 * On Windows, a background process is normally not allowed to call
 * SetForegroundWindow(); the OS only flashes the taskbar button. This applies the
 * standard AttachThreadInput / foreground-lock-timeout workaround so the
 * already-running instance can actually raise itself (e.g. when a file is opened in
 * it via the single-instance handoff).
 *
 * On other platforms this is a no-op (Qt's raise()/activateWindow() are enough).
 *
 * Must be called from the GUI thread. `winId` is QWidget::winId().
 */
void raiseToForeground(quintptr winId);
}

#endif // LC_FOREGROUND_H
