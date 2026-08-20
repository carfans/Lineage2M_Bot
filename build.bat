@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo     Lineage2MBot Pure C Native Build Script
echo ===================================================================

cd /d "%~dp0"

set "CC="
if exist "C:\ProgramData\mingw64\mingw64\bin\gcc.exe" (
    set "CC=C:\ProgramData\mingw64\mingw64\bin\gcc.exe"
) else if exist "C:\mingw64\bin\gcc.exe" (
    set "CC=C:\mingw64\bin\gcc.exe"
) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "CC=C:\msys64\mingw64\bin\gcc.exe"
) else (
    where gcc >nul 2>nul
    if %errorlevel% equ 0 set "CC=gcc"
)

if "%CC%"=="" (
    echo [ERROR] GCC compiler not found in system!
    echo Please make sure MinGW-w64 is installed at C:\ProgramData\mingw64 or in system PATH.
    exit /b 1
)

echo [*] Using Compiler: %CC%

if not exist "bin" mkdir bin
if not exist "build" mkdir build

set "CORE_SRCS=src\core\image_buffer.c src\core\logger.c src\core\cbt_manager.c src\vision\color_mask.c src\vision\morphology.c src\vision\contour.c src\game\hp_engine.c src\game\popup_engine.c src\game\map_zone_engine.c src\platform\win_capture.c src\platform\win_input.c src\api\l2m_api.c"
set "GUI_SRCS=src\gui\win_debug_dialog.c src\gui\win_main_gui.c src\main_app.c"

echo.
echo [*] [1/2] Building Desktop Application: bin\Lineage2MBot_GUI.exe ...
"%CC%" -O3 -Wall -Wextra -std=c99 -DL2M_USE_STATIC -municode -o bin\Lineage2MBot_GUI.exe %CORE_SRCS% %GUI_SRCS% -Iinclude -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -mwindows
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile Lineage2MBot_GUI.exe!
    exit /b 1
)
copy /y bin\Lineage2MBot_GUI.exe . > nul
echo [+] bin\Lineage2MBot_GUI.exe successfully built!

echo.
echo [*] [2/2] Building Dynamic Library: bin\Lineage2MBot.dll ...
"%CC%" -O3 -Wall -Wextra -std=c99 -shared -DL2M_BUILD_DLL -o bin\Lineage2MBot.dll %CORE_SRCS% -Iinclude -lgdi32 -luser32
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile Lineage2MBot.dll!
    exit /b 1
)
copy /y bin\Lineage2MBot.dll . > nul
echo [+] bin\Lineage2MBot.dll successfully built!

echo.
echo ===================================================================
echo [SUCCESS] Build Completed!
echo Outputs:
echo   - bin\Lineage2MBot_GUI.exe
echo   - bin\Lineage2MBot.dll
echo ===================================================================
