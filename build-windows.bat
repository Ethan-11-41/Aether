@echo off
rem ===========================================================================
rem  Builds Aether on Windows, runs the tests, and makes Setup.exe.
rem
rem  Just double-click this file (or run it from a command prompt).
rem
rem  You need (see README.md for step-by-step instructions):
rem    - Visual Studio 2022 or 2026 with "Desktop development with C++"
rem    - Git for Windows
rem    - Inno Setup 6 or 7 (optional; without it you get Aether.exe but no Setup.exe)
rem
rem  The GitHub build runs this same file, so if it works there it should work here.
rem ===========================================================================
setlocal EnableExtensions
cd /d "%~dp0"

set "PRESET=windows-release"
set /p AETHER_VERSION=<VERSION
set "STAGE_DIR=%CD%\build\%PRESET%\stage"
set "INSTALLER_DIR=%CD%\build\installer"

echo.
echo ==== Building Aether %AETHER_VERSION% ====
echo.

rem --- 1. Find Visual Studio and set up its build tools ------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_visual_studio

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto :no_visual_studio

echo Using Visual Studio at: %VSINSTALL%
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo
rem VsDevCmd sometimes reports harmless errors, so check for the tools we need instead.
where cl >nul 2>nul || goto :no_visual_studio
where cmake >nul 2>nul || goto :no_cmake

rem --- 2. Configure, build and test ------------------------------------------
echo.
echo ==== Step 1 of 4: preparing the build (the first time downloads JUCE, ~150 MB) ====
cmake --preset %PRESET%
if errorlevel 1 goto :fail

echo.
echo ==== Step 2 of 4: compiling (the first time takes 10-20 minutes) ====
cmake --build --preset %PRESET%
if errorlevel 1 goto :fail

echo.
echo ==== Step 3 of 4: running the automated tests ====
ctest --preset %PRESET%
if errorlevel 1 goto :fail

rem --- 3. Collect everything that gets installed into one folder ---------------
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
cmake --install "build\%PRESET%" --prefix "%STAGE_DIR%"
if errorlevel 1 goto :fail

rem --- 4. Make Setup.exe with Inno Setup ---------------------------------------
echo.
echo ==== Step 4 of 4: making Setup.exe ====
set "ISCC="
for %%P in ("%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" "%ProgramFiles%\Inno Setup 7\ISCC.exe" "%LocalAppData%\Programs\Inno Setup 7\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe" "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe") do (
    if not defined ISCC if exist "%%~P" set "ISCC=%%~P"
)
if not defined ISCC for %%X in (ISCC.exe) do set "ISCC=%%~$PATH:X"
if not defined ISCC goto :no_inno_setup

"%ISCC%" /Qp "/DAppVersion=%AETHER_VERSION%" "/DStageDir=%STAGE_DIR%" "/DOutputDir=%INSTALLER_DIR%" installer\aether.iss
if errorlevel 1 goto :fail

echo.
echo ==== Done! ====
echo Installer: %INSTALLER_DIR%\Aether-Setup-%AETHER_VERSION%.exe
echo Program:   %STAGE_DIR%\Aether.exe
goto :end

:no_inno_setup
echo.
echo Inno Setup was not found, so no Setup.exe was made.
echo The program itself is ready to run: %STAGE_DIR%\Aether.exe
echo To also get Setup.exe, install Inno Setup from https://jrsoftware.org/isdl.php and run this again.
goto :end

:no_visual_studio
echo.
echo ERROR: Visual Studio with C++ support was not found.
echo Install "Visual Studio Community" and tick "Desktop development with C++". See README.md.
goto :fail

:no_cmake
echo.
echo ERROR: CMake was not found. In the Visual Studio Installer, choose Modify and make sure
echo "C++ CMake tools for Windows" is ticked under "Desktop development with C++".
goto :fail

:fail
echo.
echo ==== BUILD FAILED - see the messages above ====
if not defined CI pause
exit /b 1

:end
if not defined CI pause
exit /b 0
