@echo off
setlocal enabledelayedexpansion

REM === Aquanact Web Build ===

REM Get script directory
set SOURCE_DIR=%~dp0
set SOURCE_DIR=%SOURCE_DIR:~0,-1%

set BUILD_DIR=%SOURCE_DIR%\out\build\wasm-release
set DIST_DIR=%SOURCE_DIR%\build_web
set EMSCRIPTEN_TOOLCHAIN=%SOURCE_DIR%\emsdk\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake

echo === Aquanact Web Build ===
echo Source: %SOURCE_DIR%
echo Build: %BUILD_DIR%
echo Output: %DIST_DIR%

REM Check for emcc
where emcc >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    set EMSDK_ENV=%SOURCE_DIR%\emsdk\emsdk_env.bat
    if exist "%EMSDK_ENV%" (
        echo Activating emsdk environment...
        call "%EMSDK_ENV%"
    ) else (
        echo ERROR: emcc not found on PATH and emsdk_env.bat not found at %EMSDK_ENV%
        echo        Activate emsdk first: cd emsdk ^&^& emsdk activate latest
        exit /b 1
    )
)

if not exist "%EMSCRIPTEN_TOOLCHAIN%" (
    echo ERROR: Emscripten CMake toolchain not found at %EMSCRIPTEN_TOOLCHAIN%
    echo        Install/activate emsdk first: cd emsdk ^&^& emsdk install latest ^&^& emsdk activate latest
    exit /b 1
)

REM Check VCPKG_ROOT
if "%VCPKG_ROOT%"=="" (
    echo ERROR: VCPKG_ROOT is not set. Run from a Visual Studio Developer prompt or set it manually.
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Configure with CMake
cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="%EMSCRIPTEN_TOOLCHAIN%" ^
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

REM Build (use default parallelism)
cmake --build "%BUILD_DIR%" --parallel
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

REM Stage only the itch.io runtime files
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
copy /Y "%BUILD_DIR%\index.html" "%DIST_DIR%\index.html" >nul
copy /Y "%BUILD_DIR%\index.js" "%DIST_DIR%\index.js" >nul
copy /Y "%BUILD_DIR%\index.wasm" "%DIST_DIR%\index.wasm" >nul
copy /Y "%BUILD_DIR%\index.data" "%DIST_DIR%\index.data" >nul
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo.
echo === Build complete ===
echo Files in %DIST_DIR%:

dir "%DIST_DIR%\index.html" "%DIST_DIR%\index.js" "%DIST_DIR%\index.wasm" "%DIST_DIR%\index.data" 2>nul

echo.
echo To test locally:
echo   emrun --no_browser --port 8080 "%DIST_DIR%\index.html"
echo To deploy:
echo   zip the four index.* files and upload to itch.io

endlocal
