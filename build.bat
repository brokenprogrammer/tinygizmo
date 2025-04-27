@echo off
setlocal enabledelayedexpansion

REM Find and setup vs build tools.
where /Q cl.exe || (
    for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do set VisualStudio=%%i
    if "!VisualStudio!" equ "" (
        echo ERROR: Visual Studio installation not found
        exit /b 1
    )
    call "!VisualStudio!\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b
)

if not exist build mkdir build
pushd build

REM Build dll
cl.exe /nologo /EHsc /GL /O2 /MT /FC /Zi /W4 /WX /wd4100 /wd4189 /D_CRT_SECURE_NO_WARNINGS ../src/tiny-gizmo.cpp /link /NOLOGO /DLL /OUT:tiny-gizmo.dll /INCREMENTAL:NO

REM Build lib
cl.exe /nologo /EHsc /Zi /GL /O2 /MT /FC /W4 /WX /wd4100 /wd4189 /D_CRT_SECURE_NO_WARNINGS /c ../src/tiny-gizmo.cpp
lib.exe /NOLOGO /OUT:tiny-gizmo.lib tiny-gizmo.obj

popd