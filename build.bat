@echo off
setlocal

echo [Build] Starting build for yMonitory...

REM Setup MSVC Environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

cd /d "%~dp0"

REM Terminate running instance to avoid LNK1104
taskkill /F /IM yMonitory.exe >nul 2>&1
timeout /t 1 /nobreak >nul

if not exist build mkdir build

set SOURCES=src\main.cpp src\Config.cpp src\SystemMonitor.cpp src\OverlayWindow.cpp
set LIBS=user32.lib gdi32.lib shell32.lib pdh.lib d2d1.lib dwrite.lib advapi32.lib
set OUT_EXE=build\yMonitory.exe

echo [Build] Compiling...
rc /fo build\yMonitory.res src\yMonitory.rc
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fe%OUT_EXE% %SOURCES% build\yMonitory.res /link %LIBS% /SUBSYSTEM:WINDOWS

if %errorlevel% neq 0 (
    echo [Build] Failed!
    exit /b 1
)

echo [Build] Success! Output: %OUT_EXE%
endlocal
