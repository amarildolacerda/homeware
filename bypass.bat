@echo off
REM Script para configurar PowerShell ExecutionPolicy como Bypass
echo Configurando ExecutionPolicy para Bypass...
powershell -Command "Set-ExecutionPolicy Bypass -Scope CurrentUser -Force"
echo Concluído. Agora você pode executar scripts sem bloqueio.
pause
