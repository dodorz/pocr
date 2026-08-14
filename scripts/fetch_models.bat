@echo off
rem ============================================================
rem  pocr - download & unpack OCR models -> models/
rem  PP-OCRv6 (tiny/small/medium) + aux models (PP-LCNet/UVDoc)
rem ============================================================
setlocal
set "ROOT=%~dp0.."
set "MODELS=%ROOT%\models"
set "BASE=https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0"

if not exist "%MODELS%" mkdir "%MODELS%"

rem ---- optional aria2 for faster / more reliable downloads ----
set "DL=curl -fL --retry 8 --retry-delay 3"
where aria2c >nul 2>nul
if %errorlevel%==0 set "DL=aria2c -x 8 -s 8 -k 2M -c --max-tries=0 --retry-wait=5"

set "MODELS_LIST=PP-OCRv6_tiny_det_infer PP-OCRv6_tiny_rec_infer PP-OCRv6_small_det_infer PP-OCRv6_small_rec_infer PP-OCRv6_medium_det_infer PP-OCRv6_medium_rec_infer PP-LCNet_x1_0_doc_ori_infer PP-LCNet_x1_0_textline_ori_infer UVDoc_infer"

for %%M in (%MODELS_LIST%) do (
    if not exist "%MODELS%\%%M\inference.yml" (
        if not exist "%MODELS%\%%M.tar" (
            echo   downloading %%M ...
            %DL% -o "%MODELS%\%%M.tar" "%BASE%\%%M.tar"
            if errorlevel 1 goto :fail
        )
        echo   unpacking %%M ...
        tar -xf "%MODELS%\%%M.tar" -C "%MODELS%"
        if errorlevel 1 goto :fail
    )
)

echo.
echo All models ready under %MODELS%
exit /b 0

:fail
echo.
echo FETCH MODELS FAILED
exit /b 1
