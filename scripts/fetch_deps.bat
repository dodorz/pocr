@echo off
rem ============================================================
rem  pocr - download & unpack build dependencies
rem  Paddle Inference / OpenCV / PDFium -> third_party/
rem ============================================================
setlocal
set "ROOT=%~dp0.."
set "THIRD=%ROOT%\third_party"

if not exist "%THIRD%" mkdir "%THIRD%"

rem ---- optional aria2 for faster / more reliable downloads ----
set "DL=curl -fL --retry 8 --retry-delay 3 -C -"
where aria2c >nul 2>nul
if %errorlevel%==0 set "DL=aria2c -x 8 -s 8 -k 2M -c --max-tries=0 --retry-wait=5"

rem ============================================================
rem [1/3] Paddle Inference 3.3.1 (Windows CPU, VS2019, MKL)
rem ============================================================
echo [1/3] Paddle Inference 3.3.1 ...
if exist "%THIRD%\paddle_inference\paddle\include\paddle_inference_api.h" goto :paddle_ok
if not exist "%THIRD%\paddle_inference.zip" (
    echo   downloading paddle_inference.zip ...
    %DL% -o "%THIRD%\paddle_inference.zip" "https://paddle-inference-lib.bj.bcebos.com/3.3.1/cxx_c/Windows/CPU/x86-64_avx-mkl-vs2019/paddle_inference.zip"
    if errorlevel 1 goto :fail
)
echo   unpacking ...
mkdir "%THIRD%\paddle_inference"
tar -xf "%THIRD%\paddle_inference.zip" -C "%THIRD%\paddle_inference"
if errorlevel 1 goto :fail
:paddle_ok

rem ============================================================
rem [2/3] OpenCV 4.11.0 (Windows x64)
rem ============================================================
echo [2/3] OpenCV 4.11.0 ...
if exist "%THIRD%\opencv\opencv\build\x64\vc16\lib\OpenCVConfig.cmake" goto :opencv_ok
if not exist "%THIRD%\opencv.exe" (
    echo   downloading opencv-4.11.0-windows.exe ...
    %DL% -o "%THIRD%\opencv.exe" "https://github.com/opencv/opencv/releases/download/4.11.0/opencv-4.11.0-windows.exe"
    if errorlevel 1 goto :fail
)
echo   unpacking ...
mkdir "%THIRD%\opencv"
7z x -y "%THIRD%\opencv.exe" -o"%THIRD%\opencv" >nul
if errorlevel 1 goto :fail
:opencv_ok

rem ============================================================
rem [3/3] PDFium (Windows x64)
rem ============================================================
echo [3/3] PDFium ...
if exist "%THIRD%\pdfium\bin\pdfium.dll" goto :pdfium_ok
if not exist "%THIRD%\pdfium.tgz" (
    echo   downloading pdfium ...
    %DL% -o "%THIRD%\pdfium.tgz" "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium/7999/pdfium-win-x64.tgz"
    if errorlevel 1 goto :fail
)
echo   unpacking ...
mkdir "%THIRD%\pdfium"
tar -xzf "%THIRD%\pdfium.tgz" -C "%THIRD%\pdfium"
if errorlevel 1 goto :fail
:pdfium_ok

echo.
echo All dependencies ready under %THIRD%
exit /b 0

:fail
echo.
echo FETCH DEPS FAILED
exit /b 1
