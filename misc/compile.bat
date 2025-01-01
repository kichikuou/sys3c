@echo off
sys3c --project=src/sys3c.cfg --outdir=.
if errorlevel 1 (
    echo.
    pause
)
