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

msbuild ../third-party/glfw-3.1.2/glfw3.vcxproj /p:Configuration=Release

cl.exe -nologo /MD /EHsc /GL /O2 /Zi /FC /I ../third-party/ /I ../third-party/glew /I ../third-party/glfw-3.1.2/include ../example-main.cpp ../../src/tiny-gizmo.cpp ../../src/tiny-gizmo-c.cpp  /link /IGNORE:4099 user32.lib gdi32.lib opengl32.lib shell32.lib /LIBPATH:../third-party/glfw-3.1.2/intermediate/Release-X64 glfw3.lib /LIBPATH:../third-party/glew/lib/x64/ glew32s.lib  -opt:ref -incremental:no /out:example.exe  

cl.exe -nologo /MD /EHsc /GL /O2 /Zi /FC /I ../third-party/ /I ../third-party/glew /I ../third-party/glfw-3.1.2/include ../example-main-c.cpp ../../src/tiny-gizmo.cpp ../../src/tiny-gizmo-c.cpp /link /IGNORE:4099 user32.lib gdi32.lib opengl32.lib shell32.lib /LIBPATH:../third-party/glfw-3.1.2/intermediate/Release-X64 glfw3.lib /LIBPATH:../third-party/glew/lib/x64/ glew32s.lib  -opt:ref -incremental:no /out:example-c.exe  


popd