@echo off
setlocal

rem Run the setup script without changing the machine or user execution policy.
rem The bypass applies only to this child PowerShell process.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-windows.ps1" %*
exit /b %ERRORLEVEL%
