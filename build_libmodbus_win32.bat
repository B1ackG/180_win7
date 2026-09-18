@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Build static libmodbus.a with the same MinGW as this project's Qt kit.
REM Output: third_party\libmodbus-win32\lib\libmodbus.a  (NO DLL)
REM The .a is linked into 180_win7.exe; industrial PCs do not need libmodbus.dll or Qt.

set "QTDIR=D:\Qt\5.15.2\mingw81_64"
set "MINGW=D:\Qt\Tools\mingw810_64"
set "PATH=%MINGW%\bin;%QTDIR%\bin;%PATH%"

set "LIBMODBUS_VER=3.1.11"
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "TP_DIR=%ROOT%\third_party"
set "ARCHIVE=%TP_DIR%\libmodbus-%LIBMODBUS_VER%.tar.gz"
set "SRC_DIR=%TP_DIR%\libmodbus-%LIBMODBUS_VER%"
set "PREFIX=%TP_DIR%\libmodbus-win32"
REM GitHub releases often reset in some networks; fall back to the tag archive.

if /I "%~1"=="rebuild" (
  if exist "%PREFIX%\lib\libmodbus.a" del /q "%PREFIX%\lib\libmodbus.a"
)

where gcc >nul 2>&1
if errorlevel 1 (
  echo [error] gcc not found. Expected MinGW at %MINGW%\bin
  exit /b 1
)
where ar >nul 2>&1
if errorlevel 1 (
  echo [error] ar not found in MinGW bin
  exit /b 1
)

for /f "delims=" %%M in ('gcc -dumpmachine') do set "GCC_MACHINE=%%M"
echo [tool] gcc=%GCC_MACHINE%
echo %GCC_MACHINE% | findstr /I "x86_64" >nul
if errorlevel 1 (
  echo [error] Need 64-bit MinGW ^(x86_64-w64-mingw32^) to match Qt mingw81_64.
  exit /b 1
)

if exist "%PREFIX%\lib\libmodbus.a" if exist "%PREFIX%\include\modbus\modbus.h" (
  echo [skip] already built: %PREFIX%\lib\libmodbus.a
  echo        headers: %PREFIX%\include\modbus
  echo        Re-run with: build_libmodbus_win32.bat rebuild
  goto :verify
)

mkdir "%TP_DIR%" 2>nul

if not exist "%SRC_DIR%\src\modbus.c" (
  if not exist "%ARCHIVE%" (
    call :download_src
    if errorlevel 1 exit /b 1
  )
  echo [extract] %ARCHIVE%
  tar xf "%ARCHIVE%" -C "%TP_DIR%"
  if errorlevel 1 (
    echo [error] tar extract failed
    exit /b 1
  )
  if not exist "%SRC_DIR%\src\modbus.c" if exist "%TP_DIR%\libmodbus-v%LIBMODBUS_VER%\src\modbus.c" (
    move "%TP_DIR%\libmodbus-v%LIBMODBUS_VER%" "%SRC_DIR%" >nul
  )
)

if not exist "%SRC_DIR%\src\modbus.c" (
  echo [error] Source not found: %SRC_DIR%\src\modbus.c
  exit /b 1
)

echo [config] generate config.h and modbus-version.h
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ver='%LIBMODBUS_VER%'; $p=$ver.Split('.'); $src='%SRC_DIR:\=\\%\src';" ^
  "$h=Get-Content -Raw (Join-Path $src 'modbus-version.h.in');" ^
  "$h=$h.Replace('@LIBMODBUS_VERSION_MAJOR@',$p[0]).Replace('@LIBMODBUS_VERSION_MINOR@',$p[1]).Replace('@LIBMODBUS_VERSION_MICRO@',$p[2]).Replace('@LIBMODBUS_VERSION@',$ver);" ^
  "Set-Content -NoNewline -Encoding ascii (Join-Path $src 'modbus-version.h') $h;" ^
  "$c=Get-Content -Raw (Join-Path $src 'win32\config.h.win32');" ^
  "$c=$c.Replace('@LIBMODBUS_VERSION@',$ver);" ^
  "Set-Content -NoNewline -Encoding ascii (Join-Path $src 'config.h') $c;"
if errorlevel 1 (
  echo [error] Failed to generate config headers
  exit /b 1
)

set "BUILD_DIR=%SRC_DIR%\src\build-mingw-static"
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

REM Win7-compatible APIs; same gcc that Qt uses. Static .a only — never a DLL.
set "CFLAGS=-O2 -Wall -DHAVE_CONFIG_H -D_WIN32_WINNT=0x0601 -I. -I%SRC_DIR%\src"

echo [build] static objects
pushd "%SRC_DIR%\src"
gcc %CFLAGS% -c modbus.c      -o "%BUILD_DIR%\modbus.o"      || goto :gcc_fail
gcc %CFLAGS% -c modbus-data.c -o "%BUILD_DIR%\modbus-data.o" || goto :gcc_fail
gcc %CFLAGS% -c modbus-rtu.c  -o "%BUILD_DIR%\modbus-rtu.o"  || goto :gcc_fail
gcc %CFLAGS% -c modbus-tcp.c  -o "%BUILD_DIR%\modbus-tcp.o"  || goto :gcc_fail
popd

echo [archive] libmodbus.a
ar rcs "%BUILD_DIR%\libmodbus.a" ^
  "%BUILD_DIR%\modbus.o" ^
  "%BUILD_DIR%\modbus-data.o" ^
  "%BUILD_DIR%\modbus-rtu.o" ^
  "%BUILD_DIR%\modbus-tcp.o"
if errorlevel 1 (
  echo [error] ar failed
  exit /b 1
)

mkdir "%PREFIX%\lib" 2>nul
mkdir "%PREFIX%\include\modbus" 2>nul
copy /y "%BUILD_DIR%\libmodbus.a" "%PREFIX%\lib\libmodbus.a" >nul
copy /y "%SRC_DIR%\src\modbus.h" "%PREFIX%\include\modbus\modbus.h" >nul
copy /y "%SRC_DIR%\src\modbus-version.h" "%PREFIX%\include\modbus\modbus-version.h" >nul
copy /y "%SRC_DIR%\src\modbus-rtu.h" "%PREFIX%\include\modbus\modbus-rtu.h" >nul
copy /y "%SRC_DIR%\src\modbus-tcp.h" "%PREFIX%\include\modbus\modbus-tcp.h" >nul

echo [install] %PREFIX%

:verify
if not exist "%PREFIX%\lib\libmodbus.a" (
  echo [error] missing %PREFIX%\lib\libmodbus.a
  exit /b 1
)
nm "%PREFIX%\lib\libmodbus.a" | findstr "modbus_new_tcp T" >nul
if errorlevel 1 (
  echo [error] libmodbus.a does not contain modbus_new_tcp
  exit /b 1
)

echo.
echo Done: static libmodbus %LIBMODBUS_VER% ready for qmake.
echo   lib:     %PREFIX%\lib\libmodbus.a
echo   include: %PREFIX%\include\modbus
echo This library is statically linked. Do NOT copy it to the industrial PC.
echo Next: rebuild 180_win7 in Qt Creator, then run deploy_win.bat
exit /b 0

:download_src
echo [download] trying mirrors...
curl.exe -L --fail --retry 2 --connect-timeout 20 --max-time 90 -o "%ARCHIVE%" "https://codeload.github.com/stephane/libmodbus/tar.gz/refs/tags/v%LIBMODBUS_VER%" && exit /b 0
curl.exe -L --fail --retry 2 --connect-timeout 20 --max-time 90 -o "%ARCHIVE%" "https://github.com/stephane/libmodbus/releases/download/v%LIBMODBUS_VER%/libmodbus-%LIBMODBUS_VER%.tar.gz" && exit /b 0
curl.exe -L --fail --retry 2 --connect-timeout 20 --max-time 90 -o "%ARCHIVE%" "https://github.com/stephane/libmodbus/archive/refs/tags/v%LIBMODBUS_VER%.tar.gz" && exit /b 0
echo [error] Download failed. Place the tarball at:
echo         %ARCHIVE%
echo         then re-run this script.
exit /b 1

:gcc_fail
popd
echo [error] gcc compile failed
exit /b 1
