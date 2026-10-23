@echo off
rem Build and run the host tests of jsonCredential.c (SHA-256, password check,
rem login request parser) and jsonName.c (canonical names, access list
rem matching) on this PC. Neither has B&R dependencies.
rem
rem The toolset is found here rather than through vcvars32.bat, which shells
rem out to a vswhere.exe that is missing from some installs and still exits 0
rem with LIB unset; the link then fails as if a library were missing.

setlocal

set "VCROOT="
if defined VCINSTALLDIR if exist "%VCINSTALLDIR%Tools\MSVC" set "VCROOT=%VCINSTALLDIR:~0,-1%"
for %%E in (Enterprise Professional Community BuildTools) do (
	for %%Y in (2022 2019) do (
		if not defined VCROOT if exist "C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Tools\MSVC" set "VCROOT=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC"
		if not defined VCROOT if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Tools\MSVC" set "VCROOT=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC"
	)
)
if not defined VCROOT (
	echo Cannot find a Visual Studio with the C++ tools.
	exit /b 1
)

set "MSVCDIR="
for /d %%D in ("%VCROOT%\Tools\MSVC\*") do set "MSVCDIR=%%~fD"

rem 32-bit, like the controller
set "CLBIN=%MSVCDIR%\bin\Hostx64\x86"
if not exist "%CLBIN%\cl.exe" set "CLBIN=%MSVCDIR%\bin\Hostx86\x86"
if not exist "%CLBIN%\cl.exe" (
	echo Found "%MSVCDIR%" but no 32-bit-targeting cl.exe under bin.
	exit /b 1
)

set "SDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKV="
for /d %%D in ("%SDK%\Lib\*") do if exist "%%~fD\um\x86\kernel32.lib" set "SDKV=%%~nxD"
if not defined SDKV (
	echo Cannot find a Windows 10 SDK with x86 libraries under "%SDK%\Lib".
	exit /b 1
)

set "PATH=%CLBIN%;%SDK%\bin\%SDKV%\x64;%PATH%"
set "INCLUDE=%MSVCDIR%\include;%SDK%\Include\%SDKV%\ucrt;%SDK%\Include\%SDKV%\um;%SDK%\Include\%SDKV%\shared"
set "LIB=%MSVCDIR%\lib\x86;%SDK%\Lib\%SDKV%\ucrt\x86;%SDK%\Lib\%SDKV%\um\x86"

set "HERE=%~dp0"
set "SRC=%HERE%..\..\src\Ar\OMJSON"
if not exist "%HERE%obj" mkdir "%HERE%obj"

cl /nologo /W4 /WX /TC /D_CRT_SECURE_NO_WARNINGS /I "%SRC%" ^
	"%HERE%test_credential.c" "%SRC%\jsonCredential.c" "%SRC%\jsonName.c" ^
	/Fo"%HERE%obj\\" /Fe"%HERE%obj\test_credential.exe" || exit /b 1

"%HERE%obj\test_credential.exe"
