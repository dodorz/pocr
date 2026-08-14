@echo off
rem ============================================================
rem  pocr - fetch official PaddleOCR cpp_infer source -> vendor/
rem  (sparse checkout, only deploy/cpp_infer, Apache-2.0)
rem ============================================================
setlocal
set "ROOT=%~dp0.."
set "VENDOR=%ROOT%\vendor"
set "UPSTREAM=%VENDOR%\ppocr_upstream"

if not exist "%UPSTREAM%\deploy\cpp_infer\CMakeLists.txt" (
    echo Fetching PaddleOCR cpp_infer source ...
    git clone --depth 1 --filter=blob:none --sparse https://github.com/PaddlePaddle/PaddleOCR.git "%UPSTREAM%"
    if errorlevel 1 goto :fail
    git -C "%UPSTREAM%" sparse-checkout set deploy/cpp_infer
    if errorlevel 1 goto :fail
) else (
    echo Upstream source already present, skipping.
)

echo.
echo Upstream src ready: %UPSTREAM%
exit /b 0

:fail
echo.
echo FETCH SRC FAILED
exit /b 1
