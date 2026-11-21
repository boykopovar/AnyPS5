@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launcher\Check-PC.ps1" %*
pause
