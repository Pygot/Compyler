@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "OUT=%ROOT%bin"
set "OBJ=%ROOT%build\obj"
set "PF86=%ProgramFiles(x86)%"
set "PF64=%ProgramFiles%"

if defined VCINSTALLDIR goto have_vc

set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%PF64%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto scan_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if defined VSDIR goto call_vc

:scan_vs
for /d %%v in ("%PF64%\Microsoft Visual Studio\*") do call :probe "%%~v"
for /d %%v in ("%PF86%\Microsoft Visual Studio\*") do call :probe "%%~v"
if not defined VSDIR echo build: no visual studio c++ toolchain found & exit /b 1

:call_vc
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 echo build: vcvars64 failed & exit /b 1

:have_vc
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

set "CFLAGS=/nologo /c /O2 /Oi /GL /MT /W3 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /Fo%OBJ%\"
set "LFLAGS=/nologo /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO"

echo [1/2] stub
cl %CFLAGS% "%ROOT%src\stub\stub.c" "%ROOT%src\common\cpy_util.c" "%ROOT%src\common\cpy_pyapi.c"
if errorlevel 1 exit /b 1
link %LFLAGS% /SUBSYSTEM:CONSOLE /OUT:"%OUT%\stub.exe" "%OBJ%\stub.obj" "%OBJ%\cpy_util.obj" "%OBJ%\cpy_pyapi.obj" kernel32.lib user32.lib psapi.lib
if errorlevel 1 exit /b 1

echo [2/2] compyler
cl %CFLAGS% "%ROOT%src\compiler\main.c" "%ROOT%src\compiler\pack.c" "%ROOT%src\compiler\scan.c" "%ROOT%src\compiler\pe.c" "%ROOT%src\nc\nc.c" "%ROOT%src\nc\cc.c" "%ROOT%src\common\cpy_util.c" "%ROOT%src\common\cpy_pyapi.c"
if errorlevel 1 exit /b 1
link %LFLAGS% /SUBSYSTEM:CONSOLE /OUT:"%OUT%\compyler.exe" "%OBJ%\main.obj" "%OBJ%\pack.obj" "%OBJ%\scan.obj" "%OBJ%\pe.obj" "%OBJ%\nc.obj" "%OBJ%\cc.obj" "%OBJ%\cpy_util.obj" "%OBJ%\cpy_pyapi.obj" kernel32.lib user32.lib psapi.lib
if errorlevel 1 exit /b 1

copy /y "%ROOT%src\nc\cpyrt.h" "%OUT%\cpyrt.h" >nul

echo.
echo built: %OUT%\compyler.exe
echo built: %OUT%\stub.exe
goto :eof

:probe
for /d %%e in ("%~1\*") do if exist "%%~e\VC\Auxiliary\Build\vcvars64.bat" set "VSDIR=%%~e"
goto :eof
