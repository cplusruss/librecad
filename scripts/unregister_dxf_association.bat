@echo off
REM ===========================================================================
REM Undo what register_dxf_association.bat did, in HKCU only.
REM Does NOT touch the user's "default app" UserChoice key (Windows owns that).
REM ===========================================================================
setlocal EnableExtensions

set "PROGID=LibreCAD.dxf"
set "PROGID_KEY=HKCU\Software\Classes\%PROGID%"
set "DXF_KEY=HKCU\Software\Classes\.dxf"

echo Removing %PROGID% ...
reg delete "%PROGID_KEY%" /f >nul 2>&1

echo Removing LibreCAD from .dxf OpenWithProgids ...
reg delete "%DXF_KEY%\OpenWithProgids" /v "%PROGID%" /f >nul 2>&1

where ie4uinit.exe >nul 2>&1 && ie4uinit.exe -show

echo Done.
endlocal
