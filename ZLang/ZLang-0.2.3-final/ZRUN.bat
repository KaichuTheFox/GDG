@echo off
if "%~1"=="" (echo Usage: ZRUN source.z & exit /b 2)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ZRUN.ps1" "%~1"
