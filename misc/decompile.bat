@echo off
setlocal

if not exist src/sys3c.cfg goto decompile
set /p answer="src/sys3c.cfg already exists. Do you want to continue? (y/n): "
if /i not "%answer%"=="y" (
    echo Operation aborted.
    pause
    exit /b 1
)

:decompile
sys3dc . --outdir=src
if errorlevel 1 (
    echo.
    pause
)
