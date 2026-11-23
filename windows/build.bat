@echo off
setlocal
cd /d "%~dp0"

echo ==> Building MeoCat.exe (Windows native C++17 / Win32 + GDI+ + Raw Input)...

where cl >nul 2>nul
if %ERRORLEVEL%==0 (
    rc /nologo /fo meocat-version.res version.rc
    if errorlevel 1 exit /b 1
    cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /D_WIN32_WINNT=0x0601 ^
       /W3 src\main.cpp meocat-version.res /Fe:MeoCat.exe ^
       /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest ^
       user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib crypt32.lib dwmapi.lib
    if errorlevel 1 exit /b 1
    del /q main.obj meocat-version.res 2>nul
    echo ==> Built successfully with MSVC: %~dp0MeoCat.exe
    exit /b 0
)

where g++ >nul 2>nul
if %ERRORLEVEL%==0 (
    windres app.rc -O coff -o meocat-res.o
    if errorlevel 1 exit /b 1
    g++ -std=c++17 -O2 -mwindows -municode -D_WIN32_WINNT=0x0601 -static ^
        src\main.cpp meocat-res.o -o MeoCat.exe ^
        -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lcrypt32 -ldwmapi
    if errorlevel 1 exit /b 1
    del /q meocat-res.o 2>nul
    echo ==> Built successfully with MinGW: %~dp0MeoCat.exe
    exit /b 0
)

echo [ERROR] Neither MSVC (cl.exe) nor MinGW (g++.exe) found in PATH.
echo Please run from "x64 Native Tools Command Prompt for VS" or install Visual Studio Build Tools.
exit /b 1
