@echo off
rem ============================================================
rem  pocr build script (Windows / MSVC / Ninja)
rem ============================================================
setlocal
set "ROOT=%~dp0.."

rem ---- locate MSVC via vswhere (works for any VS version) ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)
set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" echo [ERROR] MSVC x64 Build Tools not found. Install VS Build Tools with the C++ workload.
if not exist "%VCVARS%" exit /b 1

echo Using MSVC at: %VCVARS%

call "%VCVARS%"
if errorlevel 1 goto :fail

set "PADDLE_LIB=%ROOT%\third_party\paddle_inference"
set "OPENCV_DIR=%ROOT%\third_party\opencv\opencv\build"

if not exist "%PADDLE_LIB%\paddle\include\paddle_inference_api.h" echo [ERROR] Paddle Inference libs missing. Run scripts\fetch_deps.bat first.
if not exist "%PADDLE_LIB%\paddle\include\paddle_inference_api.h" exit /b 1
if not exist "%OPENCV_DIR%\x64\vc16\lib\OpenCVConfig.cmake" echo [ERROR] OpenCV build dir missing. Run scripts\fetch_deps.bat first.
if not exist "%OPENCV_DIR%\x64\vc16\lib\OpenCVConfig.cmake" exit /b 1

echo [1/2] CMake configure ...
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DWITH_STATIC_LIB=ON ^
    -DPADDLE_LIB="%PADDLE_LIB%" ^
    -DOPENCV_DIR="%OPENCV_DIR%"
if errorlevel 1 goto :fail

echo [2/2] Build ...
cmake --build "%ROOT%\build"
if errorlevel 1 goto :fail

echo.
echo Build OK: %ROOT%\build\pocr.exe
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
