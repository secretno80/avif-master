@echo off
cd /d "%~dp0"
REM --- Basic Build Script for AVIF-Master ---

set SRC_DIR=src
set BUILD_DIR=build
set EXE_NAME=AVIFMaster.exe

REM --- Cleanup ---
echo Cleaning previous build...
if exist %BUILD_DIR% rd /s /q %BUILD_DIR%
mkdir %BUILD_DIR%
echo.

REM --- Compile Resources ---
echo Compiling resources...
windres -I . "%SRC_DIR%/resource.rc" -o "%BUILD_DIR%/resource.o"
if errorlevel 1 (
    echo Resource compilation failed.
    exit /b 1
)
echo.

REM --- Compile C++ Source ---
echo Compiling C++ source...
g++ -std=c++17 -mwindows "%SRC_DIR%/main.cpp" "%BUILD_DIR%/resource.o" -o "%BUILD_DIR%/%EXE_NAME%" -lcomctl32 -lgdi32 -lshell32 -lshlwapi -lole32 -lcomdlg32
if errorlevel 1 (
    echo C++ compilation failed.
    exit /b 1
)
echo.

echo Build successful!
echo Executable available at: %BUILD_DIR%\%EXE_NAME%
pause
