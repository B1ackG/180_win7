@echo off
setlocal EnableExtensions

REM Pack a self-contained folder for an industrial PC that has NO Qt installed.
REM Staging:  <project>\dist
REM Desktop:  %USERPROFILE%\Desktop\180_win7  (same as before; copy that whole folder)

set "QTDIR=D:\Qt\5.15.2\mingw81_64"
set "MINGW=D:\Qt\Tools\mingw810_64"
set "PATH=%QTDIR%\bin;%MINGW%\bin;%PATH%"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "DIST=%ROOT%\dist"
set "DESKTOP_DIR=%USERPROFILE%\Desktop\180_win7"
set "EXE=%ROOT%\release\180_win7.exe"

if not exist "%EXE%" (
  echo [error] Missing %EXE%
  echo Build Release in Qt Creator first, then re-run this script.
  exit /b 1
)

where windeployqt.exe >nul 2>&1
if errorlevel 1 (
  echo [error] windeployqt not found. Expected %QTDIR%\bin
  exit /b 1
)

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy /y "%EXE%" "%DIST%\" >nul

if exist "%ROOT%\release\feature_switches.ini" (
  copy /y "%ROOT%\release\feature_switches.ini" "%DIST%\" >nul
) else if exist "%ROOT%\feature_switches.ini" (
  copy /y "%ROOT%\feature_switches.ini" "%DIST%\" >nul
)
if exist "%ROOT%\release\config.ini" (
  copy /y "%ROOT%\release\config.ini" "%DIST%\" >nul
) else if exist "%ROOT%\config.ini" (
  copy /y "%ROOT%\config.ini" "%DIST%\" >nul
)

REM Do not pass --release alone: MinGW Qt plugins can be mis-detected as debug.
"%QTDIR%\bin\windeployqt.exe" --force --compiler-runtime "%DIST%\180_win7.exe"
if errorlevel 1 (
  echo [error] windeployqt failed
  exit /b 1
)

if exist "%DIST%\libmodbus.dll" (
  echo [error] libmodbus.dll appeared in dist. Static link failed; do not ship this folder.
  exit /b 1
)
if not exist "%DIST%\platforms\qwindows.dll" (
  echo [error] missing platforms\qwindows.dll
  exit /b 1
)
if not exist "%DIST%\libgcc_s_seh-1.dll" (
  echo [error] missing libgcc_s_seh-1.dll
  exit /b 1
)
if not exist "%DIST%\libstdc++-6.dll" (
  echo [error] missing libstdc++-6.dll
  exit /b 1
)
if not exist "%DIST%\libwinpthread-1.dll" (
  echo [error] missing libwinpthread-1.dll
  exit /b 1
)

echo.
echo Packaged to:
echo   %DIST%

mkdir "%DESKTOP_DIR%" 2>nul
robocopy "%DIST%" "%DESKTOP_DIR%" /E /R:1 /W:1 /NFL /NDL /NJH /NJS /nc /ns /np >nul
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 (
  echo.
  echo [warn] Could not update Desktop\180_win7 ^(folder in use? close the running program^).
  echo Copy this folder to the industrial PC instead:
  echo   %DIST%
  exit /b 0
)

echo   %DESKTOP_DIR%
echo.
echo Copy the entire Desktop\180_win7 folder to the industrial PC ^(64-bit Windows 7+^).
echo Do not copy only the exe. Do not install Qt on the industrial PC.
echo Double-click 180_win7.exe inside that folder.
exit /b 0
