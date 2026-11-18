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

#ifndef LC_EMFEXPORTER_H
#define LC_EMFEXPORTER_H

#include <QtGlobal>

class RS_Graphic;

/**
 * Renders an RS_Graphic to a Windows Enhanced Metafile (EMF) and places it on the
 * system clipboard (CF_ENHMETAFILE), so the geometry can be pasted as editable
 * vector graphics into Visio, PowerPoint, Word, etc.
 *
 * On non-Windows platforms this is a no-op returning false.
 */
class LC_EmfExporter {
public:
    // When blackAndWhite is true, all entities are rendered in black (on a white
    // background) instead of their DXF colours.
    static bool copyGraphicToClipboard(RS_Graphic* graphic, bool blackAndWhite = false);
};

#endif // LC_EMFEXPORTER_H
