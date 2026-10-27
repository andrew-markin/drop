@echo off

if exist ".env" (
    for /f "usebackq delims=" %%i in (`type ".env" ^| findstr /v "^#" ^| findstr /v "^$"`) do (
        set "%%i"
    )
)

rem libsodium comes from vcpkg, linked statically: vcpkg install libsodium:x64-windows-static-md
if not defined VCPKG_ROOT set "VCPKG_ROOT=%VCPKG_INSTALLATION_ROOT%"

if exist Release (rmdir /S /Q Release)
mkdir Release\Build
cd Release\Build

cmake ..\..\Sources -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
      -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
      -DVCPKG_TARGET_TRIPLET=x64-windows-static-md || exit /b 1
cmake --build . --target PACKAGE --config Release --parallel || exit /b 1

move *-*.exe ..
cd ..\..

rmdir /S /Q Release\Build
