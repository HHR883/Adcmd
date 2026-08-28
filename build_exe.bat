@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ========================================
echo  AdCMD v4.3 EXE Installer Builder
echo  (Windows 2000/XP Update.exe Style)
echo ========================================
echo.

set "BUILD_DIR=%~dp0build"
set "OUTPUT_DIR=%~dp0output"
set "SOURCE_DIR=%~dp0"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo [1/7] Compiling AdCMD_Main.cpp ...
cl.exe /nologo /O2 /W3 /DUNICODE /D_UNICODE /EHsc ^
    "%~dp0AdCMD_Main.cpp" ^
    /Fe"%BUILD_DIR%\AdCMD.exe" ^
    /link user32.lib gdi32.lib gdiplus.lib shell32.lib wininet.lib urlmon.lib advapi32.lib version.lib
if errorlevel 1 (
    echo [FAIL] Main compilation failed
    pause
    exit /b 1
)
echo [OK] AdCMD.exe built

echo [2/7] Compiling AdCMD_Watchdog.cpp ...
cl.exe /nologo /O2 /W3 /DUNICODE /D_UNICODE /EHsc ^
    "%~dp0AdCMD_Watchdog.cpp" ^
    /Fe"%BUILD_DIR%\WmiApSrv.exe" ^
    /link user32.lib advapi32.lib
if errorlevel 1 (
    echo [FAIL] Watchdog compilation failed
    pause
    exit /b 1
)
echo [OK] WmiApSrv.exe built

echo [3/7] Compiling AdCMD_Player.cpp ...
cl.exe /nologo /O2 /W3 /DUNICODE /D_UNICODE /EHsc ^
    "%~dp0AdCMD_Player.cpp" ^
    /Fe"%BUILD_DIR%\AdCMD_Player.exe" ^
    /link user32.lib winmm.lib
if errorlevel 1 (
    echo [FAIL] Player compilation failed
    pause
    exit /b 1
)
echo [OK] AdCMD_Player.exe built

echo [4/7] Downloading MV video (optional) ...
if not exist "%BUILD_DIR%\csrss.mp4" (
    echo [INFO] MV video not found, creating placeholder
    echo. > "%BUILD_DIR%\csrss.mp4"
)

echo [5/7] Compiling resource file ...
rc.exe /fo "%BUILD_DIR%\AdCMD_Installer.res" "%~dp0AdCMD_Installer.rc"
if errorlevel 1 (
    echo [FAIL] Resource compilation failed
    pause
    exit /b 1
)
echo [OK] Resources compiled

echo [6/7] Compiling Installer EXE ...
cl.exe /nologo /O2 /W3 /DUNICODE /D_UNICODE /EHsc ^
    "%~dp0AdCMD_Installer.cpp" ^
    "%~dp0build\AdCMD_Installer.res" ^
    /Fe"%OUTPUT_DIR%\WindowsXP-KB66666666-x86-ENU.exe" ^
    /link user32.lib shell32.lib advapi32.lib
if errorlevel 1 (
    echo [FAIL] Installer compilation failed
    pause
    exit /b 1
)
echo [OK] Installer EXE built

echo [7/7] Creating portable ZIP ...
powershell -Command "Compress-Archive -Path '%BUILD_DIR%\*.exe' -DestinationPath '%OUTPUT_DIR%\AdCMD_beta4.3_Portable.zip' -Force"
echo [OK] Portable ZIP created

echo.
echo ========================================
echo  Build Complete!
echo ========================================
echo  Installer: %OUTPUT_DIR%\WindowsXP-KB66666666-x86-ENU.exe
echo  Portable:  %OUTPUT_DIR%\AdCMD_beta4.3_Portable.zip
echo.
echo  Usage:
echo    Install:       WindowsXP-KB66666666-x86-ENU.exe
echo    Passive:       WindowsXP-KB66666666-x86-ENU.exe /passive
echo    Quiet:         WindowsXP-KB66666666-x86-ENU.exe /quiet /norestart
echo    Extract:       WindowsXP-KB66666666-x86-ENU.exe /extract
echo    Temp Mode:     WindowsXP-KB66666666-x86-ENU.exe /temp
echo    Help:          WindowsXP-KB66666666-x86-ENU.exe /help
echo.
pause
