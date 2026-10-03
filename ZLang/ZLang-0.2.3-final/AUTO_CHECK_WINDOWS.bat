@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0AUTO_CHECK_WINDOWS.ps1" %*
pause
