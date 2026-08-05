@echo off
cd /git/homeware
:loop
powershell -ExecutionPolicy Bypass -File "update_all.ps1"
timeout /t 10 /nobreak >nul
goto loop
