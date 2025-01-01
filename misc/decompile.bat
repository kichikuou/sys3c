@echo off
sys3dc . --outdir=src
if errorlevel 1 (
    echo.
    pause
)
