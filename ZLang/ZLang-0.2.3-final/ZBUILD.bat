@echo off
if "%~1"=="" (echo Usage: ZBUILD source.z [output.exe] & exit /b 2)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ZBUILD.ps1" "%~1" "%~2"
