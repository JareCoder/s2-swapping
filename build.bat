@echo off
rem Helper script to execute powershell build script
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
pause
