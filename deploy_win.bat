@echo off
setlocal
set QTDIR=D:\Qt\5.15.2\mingw81_64
set PATH=%QTDIR%\bin;D:\Qt\Tools\mingw810_64\bin;%PATH%

set ROOT=%~dp0
set DIST=%ROOT%dist
set EXE=%ROOT%release\180_win7.exe

if not exist "%EXE%" (
  echo Missing %EXE%
  exit /b 1
)

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy /y "%EXE%" "%DIST%\" >nul
if exist "%ROOT%release\feature_switches.ini" copy /y "%ROOT%release\feature_switches.ini" "%DIST%\" >nul

REM MinGW Qt plugins may be mis-detected as debug; do not use --release filter alone.
"%QTDIR%\bin\windeployqt.exe" --force --compiler-runtime "%DIST%\180_win7.exe"
if errorlevel 1 exit /b 1

echo.
echo Deployed to: %DIST%
echo Run: %DIST%\180_win7.exe
