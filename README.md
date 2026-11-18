# LibreCAD (Windows portable fork)

A Windows-only, portable build of [LibreCAD](https://github.com/LibreCAD/LibreCAD) 2.2.2,
tuned for PCB-scale mechanical drawings (mil units, coordinates in the tens of thousands).

Unzip a release to any 64-bit Windows 10/11 machine and run `LibreCAD.exe`. Nothing is installed, nothing goes in the registry, and no Qt or MinGW install
is needed on the target machine.

## Download

Grab `LibreCAD-portable-win64.zip` from the [Releases](../../releases) page, unzip anywhere,
run `LibreCAD.exe`.

## What differs from upstream LibreCAD

### Portable by design
- Every runtime dependency (Qt 6.9 DLLs and plugins, MinGW runtime, software OpenGL fallback
  for VMs and RDP sessions) ships beside the exe; `qt.conf` points Qt at the local folder.
- All settings live in one `LibreCAD.ini` beside the exe, including window layout, toolbars,
  recent files and crash-recovery data. Put the folder in Dropbox and settings follow you.
- On first run with no INI present, existing LibreCAD registry settings are imported once.
  The registry is never written or deleted.
- The build script audits every shipped `.exe`/`.dll` and fails if any import is not in the
  folder or part of every Windows install.

### Drawing and editing
- **Snapping that works at PCB scale.** The on-entity/free-snap switch distance is a fixed
  on-screen pixel size (default 16 px) instead of 5 drawing units, so midpoint, endpoint,
  center and intersection snaps are easy to hit at any zoom.
- **Bigger grips.** Grip squares default to 8 px, and the grip pick radius is a constant
  pixel distance at every zoom level.
- **Thin lines by default.** New entities and dimension/extension lines default to 0.00 mm
  line weight.
- **Centered dimension text.** New drawings put dimension text on the dimension line with the
  line broken around it (AutoCAD `DIMTAD = 0`). Loaded DXF files keep their own dim style.
- **AutoCAD-style trim** for lines and arcs: remove the middle span between two cutters, or
  split at a crossing.
- **Offset** fixes for large-coordinate polylines.
- **Copy and paste between open documents** works (upstream armed the paste with no preview
  and placed nothing).
- **Unitless paste rule:** pasting a unitless drawing into an inch drawing no longer rescales
  it by 1/25.4.
- **Arc rendering fixes:** arcs no longer vanish when zoomed out or sprout extra segments when
  zoomed in tight on reversed arcs.

### Output
- **Copy as EMF / Copy as EMF (black and white):** puts the selection on the Windows clipboard
  as a vector EMF for pasting into Visio, PowerPoint or Word.
- **Console converters** with bundled fonts:
  ```
  LibreCAD.exe dxf2pdf foo.dxf
  LibreCAD.exe dxf2png foo.dxf
  LibreCAD.exe dxf2svg foo.dxf
  ```

### Interface
- Frameless main window with a custom title bar holding the menu, with native Windows
  move, snap, resize and double-click maximize.
- Five app icons (Flat, Dark tile, Bold, Light tile, Classic), chosen in
  Application Preferences > Graphic View and applied immediately. Each is also an icon
  resource in the exe for shortcuts.
- Numpad `+` / `-` step through the tabs of any dialog.
- Crash handler writes `LibreCAD.exe.crash.log` beside the exe.

### Removed
- Help menu, About and License dialogs, online-doc links and the release checker.
- Splash screen.
- Translations and language selection (English only).
- qmake project files, Linux/macOS packaging and CI, the sample plugin, `ttf2lff`.

### Plugins included
align, asciifile, divide, gear, list, picfile, plotequation, pointstocsv, sameprop.

## Building from source

Requirements (paths are the ones the scripts expect):
- Qt 6.9.0 MinGW 64-bit at `C:\Qt\6.9.0\mingw_64`
- MinGW 13.1 at `C:\Qt\Tools\mingw1310_64`, Ninja at `C:\Qt\Tools\Ninja`
- CMake 3.28+
- Boost headers (header-only use; pass the folder as `BOOST_ROOT`)

Configure and build:
```powershell
cmake -S . -B build\x64-release-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe `
  -DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe `
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/mingw_64 `
  -DBOOST_ROOT=C:/path/to/boost
cmake --build build\x64-release-mingw
```

Assemble the self-contained `portable\` folder from the build and run the dependency audit:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\make_portable.ps1
```

Associate `.dxf` files with the portable exe: copy `scripts\register_dxf_association.bat`
beside `LibreCAD.exe` and run it (undo with `unregister_dxf_association.bat`).

## License

[GNU General Public License version 2](https://www.gnu.org/licenses/gpl-2.0.html) (GPLv2), as upstream LibreCAD.
LibreCAD is based on the community edition of [QCAD](https://www.qcad.org); DXF/DWG I/O by
[libdxfrw](https://github.com/LibreCAD/libdxfrw).
