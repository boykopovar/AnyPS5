@echo off
setlocal
if not exist "%~dp0b\tools\launcher\AnyPS5Launcher.exe" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launcher\Start-Native.ps1"
    exit /b %ERRORLEVEL%
)
start "" /D "%~dp0" "%~dp0b\tools\launcher\AnyPS5Launcher.exe"
exit /b %ERRORLEVEL%
