@echo off
cd /d "%~dp0"
REM =============================================================
REM  AVIF-Master - Build + Package Script
REM  Requires: g++ (MinGW), windres, Inno Setup 6
REM =============================================================

set SRC_DIR=src
set BUILD_DIR=build
set EXE_NAME=AVIFMaster.exe
set ISS_FILE=AVIFMasterInstaller.iss
set SETUP_DIR=Setup

REM --- Inno Setup CLI 경로 (환경에 맞게 수정) ---
set "INNO1=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
set "INNO2=C:\Program Files\Inno Setup 6\ISCC.exe"

REM ──────────────────────────────────────────────
REM  Step 0 : Cleanup
REM ──────────────────────────────────────────────
echo [1/4] Cleaning previous build...
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
if exist "%SETUP_DIR%"  rd /s /q "%SETUP_DIR%"
mkdir "%BUILD_DIR%"
echo.

REM ──────────────────────────────────────────────
REM  Step 1 : Compile Resources
REM ──────────────────────────────────────────────
echo [2/4] Compiling resources...
windres -I "%SRC_DIR%" "%SRC_DIR%/resource.rc" -o "%BUILD_DIR%/resource.o"
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    pause
    exit /b 1
)
echo.

REM ──────────────────────────────────────────────
REM  Step 2 : Compile C++ Source
REM ──────────────────────────────────────────────
echo [3/4] Compiling C++ source...
g++ -std=c++17 -O2 -mwindows ^
    "%SRC_DIR%/main.cpp" "%BUILD_DIR%/resource.o" ^
    -o "%BUILD_DIR%/%EXE_NAME%" ^
    -lcomctl32 -lgdi32 -lshell32 -lshlwapi -lole32 -lcomdlg32
if errorlevel 1 (
    echo [ERROR] C++ compilation failed.
    pause
    exit /b 1
)
echo Build OK: %BUILD_DIR%\%EXE_NAME%
echo.

REM ──────────────────────────────────────────────
REM  Step 3 : Inno Setup Installer
REM ──────────────────────────────────────────────
echo [4/4] Creating installer with Inno Setup...

REM ISS 파일 존재 확인
if not exist "%ISS_FILE%" (
    echo [WARN] %ISS_FILE% not found - skipping installer creation.
    goto BUILD_OK
)

REM Setup.ico 존재 확인
if not exist "Setup.ico" (
    echo [WARN] Setup.ico not found - skipping installer creation.
    goto BUILD_OK
)

REM Inno Setup 경로 자동 탐색
set ISCC=
if exist "%INNO1%" set "ISCC=%INNO1%"
if exist "%INNO2%" set "ISCC=%INNO2%"

if "%ISCC%"=="" (
    echo [WARN] Inno Setup 6 not found at expected paths.
    echo        Install from https://jrsoftware.org/isinfo.php
    echo        or set ISCC manually in this script.
    goto BUILD_OK
)

"%ISCC%" "%ISS_FILE%"
if errorlevel 1 (
    echo [ERROR] Inno Setup compilation failed.
    pause
    exit /b 1
)
echo Installer created in: %SETUP_DIR%\
echo.

:BUILD_OK
echo =============================================================
echo  Build complete!
echo    EXE      : %BUILD_DIR%\%EXE_NAME%
if exist "%SETUP_DIR%\AVIFMaster_Setup.exe" (
    echo    Installer: %SETUP_DIR%\AVIFMaster_Setup.exe
)
echo =============================================================
pause
exit /b 0
