@echo off
REM ===========================================================================
REM Register .dxf -> LibreCAD (current user, no admin required).
REM
REM What this does, in HKCU\Software\Classes:
REM   * Creates a ProgID  "LibreCAD.dxf"  with description, default icon,
REM     and an Open verb pointing at the librecad.exe in THIS folder.
REM   * Adds  LibreCAD.dxf  to .dxf's OpenWithProgids list, so Explorer's
REM     "Open with" menu offers LibreCAD.
REM
REM What this does NOT do: it does not silently override the user's current
REM default app for .dxf (Windows protects that choice with a hashed key).
REM After running this script:
REM   right-click any .dxf -> Open with -> Choose another app -> LibreCAD,
REM   tick "Always use this app", click OK.
REM From then on Explorer will show the DXF document icon.
REM
REM Re-run anytime the librecad.exe path changes.
REM Unregister with: unregister_dxf_association.bat
REM ===========================================================================
setlocal EnableExtensions

set "EXE=%~dp0librecad.exe"
if not exist "%EXE%" (
    echo [error] librecad.exe not found next to this script: %EXE%
    echo Copy register_dxf_association.bat into the same folder as librecad.exe
    echo and run it from there.
    exit /b 1
)

REM -2  ==  resource ID 2 in the EXE's icon table (see librecad.rc).
set "ICON=%EXE%,-2"
set "PROGID=LibreCAD.dxf"
set "PROGID_KEY=HKCU\Software\Classes\%PROGID%"
set "DXF_KEY=HKCU\Software\Classes\.dxf"

echo Registering %PROGID% ...
reg add "%PROGID_KEY%" /ve /t REG_SZ /d "DXF Drawing (LibreCAD)" /f >nul
reg add "%PROGID_KEY%\DefaultIcon" /ve /t REG_SZ /d "%ICON%" /f >nul
reg add "%PROGID_KEY%\shell\open\command" /ve /t REG_SZ /d "\"%EXE%\" \"%%1\"" /f >nul

echo Adding LibreCAD to .dxf "Open with" list ...
reg add "%DXF_KEY%\OpenWithProgids" /v "%PROGID%" /t REG_NONE /f >nul

REM Nudge Explorer to refresh its icon cache for the new ProgID.
REM (Cheap version: no shell broadcast, just touch HKCU and let Explorer
REM  pick it up on next session.  If you want immediate refresh, log out
REM  and back in, or run ie4uinit -show.)
where ie4uinit.exe >nul 2>&1 && ie4uinit.exe -show

echo.
echo Done.
echo Next step: right-click a .dxf in Explorer, "Open with" -^> "Choose
echo another app" -^> LibreCAD, tick "Always use this app", OK.
endlocal
