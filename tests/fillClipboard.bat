@echo off
setlocal enabledelayedexpansion

for /L %%i in (1, 1, 1000) do (
    echo %%i | clip.exe
    powershell -Command "Start-Sleep -Milliseconds 15"
)
