# Lineage2MBot PowerShell Build Script
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "     Lineage2MBot Pure C Native Build Script (PowerShell)" -ForegroundColor Cyan
Write-Host "===================================================================" -ForegroundColor Cyan

$gccExe = "C:\ProgramData\mingw64\mingw64\bin\gcc.exe"
if (-not (Test-Path $gccExe)) {
    if (Get-Command gcc -ErrorAction SilentlyContinue) {
        $gccExe = (Get-Command gcc).Source
    } elseif (Test-Path "C:\mingw64\bin\gcc.exe") {
        $gccExe = "C:\mingw64\bin\gcc.exe"
    } elseif (Test-Path "C:\msys64\mingw64\bin\gcc.exe") {
        $gccExe = "C:\msys64\mingw64\bin\gcc.exe"
    } else {
        Write-Host "[ERROR] GCC compiler not found in system!" -ForegroundColor Red
        exit 1
    }
}

Write-Host "[*] Using Compiler: $gccExe" -ForegroundColor Green
& $gccExe --version | Select-Object -First 1

if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Path "bin" | Out-Null }
if (-not (Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }

$coreSrcs = @(
    "src/core/image_buffer.c",
    "src/core/logger.c",
    "src/core/cbt_manager.c",
    "src/core/window_profile_manager.c",
    "src/vision/color_mask.c",
    "src/vision/morphology.c",
    "src/vision/contour.c",
    "src/game/hp_engine.c",
    "src/game/popup_engine.c",
    "src/game/map_zone_engine.c",
    "src/platform/win_capture.c",
    "src/platform/win_input.c",
    "src/api/l2m_api.c"
)

$guiSrcs = @(
    "src/gui/win_debug_dialog.c",
    "src/gui/win_main_gui.c",
    "src/main_app.c"
)

# 1. Build Native Desktop GUI Application
Write-Host "`n[*] [1/2] Building Desktop Application: bin/Lineage2MBot_GUI.exe ..." -ForegroundColor Yellow
$guiArgs = @("-O3", "-Wall", "-Wextra", "-std=c99", "-DL2M_USE_STATIC", "-municode", "-o", "bin/Lineage2MBot_GUI.exe") + $coreSrcs + $guiSrcs + @("-Iinclude", "-lgdi32", "-luser32", "-lcomctl32", "-lcomdlg32", "-lpsapi", "-mwindows")
& $gccExe $guiArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to compile Lineage2MBot_GUI.exe!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Copy-Item "bin/Lineage2MBot_GUI.exe" -Destination "Lineage2MBot_GUI.exe" -Force
Write-Host "[+] bin/Lineage2MBot_GUI.exe successfully built!" -ForegroundColor Green

# 2. Build Dynamic Link Library DLL
Write-Host "`n[*] [2/2] Building Dynamic Link Library: bin/Lineage2MBot.dll ..." -ForegroundColor Yellow
$dllArgs = @("-O3", "-Wall", "-Wextra", "-std=c99", "-shared", "-DL2M_BUILD_DLL", "-o", "bin/Lineage2MBot.dll") + $coreSrcs + @("-Iinclude", "-lgdi32", "-luser32", "-lpsapi")
& $gccExe $dllArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to compile Lineage2MBot.dll!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Copy-Item "bin/Lineage2MBot.dll" -Destination "Lineage2MBot.dll" -Force
if (Test-Path "python_bindings") {
    Copy-Item "bin/Lineage2MBot.dll" -Destination "python_bindings/Lineage2MBot.dll" -Force
}
Write-Host "[+] bin/Lineage2MBot.dll successfully built!" -ForegroundColor Green

Write-Host "`n===================================================================" -ForegroundColor Cyan
Write-Host "[SUCCESS] Build Completed!" -ForegroundColor Green
Write-Host "Output: Lineage2MBot_GUI.exe, Lineage2MBot.dll" -ForegroundColor White
Write-Host "===================================================================`n" -ForegroundColor Cyan
